// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_proto_utils.h"

#include "base/containers/flat_map.h"
#include "base/json/json_string_value_serializer.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/protocol/list_passwords_result.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/password_with_local_data.pb.h"

using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

// Keys used to serialize and deserialize password form data.
constexpr char kActionKey[] = "action";
constexpr char kFieldsKey[] = "fields";
constexpr char kFormControlTypeKey[] = "form_control_type";
constexpr char kFormDataKey[] = "form_data";
constexpr char kNameKey[] = "name";
constexpr char kSkipZeroClickKey[] = "skip_zero_click";
constexpr char kUrlKey[] = "url";

base::Time ConvertToBaseTime(uint64_t time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      // Use FromDeltaSinceWindowsEpoch because create_time_us has
      // always used the Windows epoch.
      base::Microseconds(time));
}

}  // namespace

sync_pb::PasswordSpecificsData_PasswordIssues PasswordIssuesMapToProto(
    const base::flat_map<InsecureType, InsecurityMetadata>&
        form_password_issues) {
  sync_pb::PasswordSpecificsData::PasswordIssues password_issues;
  for (const auto& [insecure_type, insecure_metadata] : form_password_issues) {
    sync_pb::PasswordSpecificsData::PasswordIssues::PasswordIssue issue;
    issue.set_date_first_detection_microseconds(
        insecure_metadata.create_time.ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
    issue.set_is_muted(insecure_metadata.is_muted.value());
    switch (insecure_type) {
      case InsecureType::kLeaked:
        DCHECK(!password_issues.has_leaked_password_issue());
        *password_issues.mutable_leaked_password_issue() = std::move(issue);
        break;
      case InsecureType::kPhished:
        DCHECK(!password_issues.has_phished_password_issue());
        *password_issues.mutable_phished_password_issue() = std::move(issue);
        break;
      case InsecureType::kWeak:
        DCHECK(!password_issues.has_weak_password_issue());
        *password_issues.mutable_weak_password_issue() = std::move(issue);
        break;
      case InsecureType::kReused:
        DCHECK(!password_issues.has_reused_password_issue());
        *password_issues.mutable_reused_password_issue() = std::move(issue);
        break;
    }
  }
  return password_issues;
}

base::flat_map<InsecureType, InsecurityMetadata> PasswordIssuesMapFromProto(
    const sync_pb::PasswordSpecificsData& password_data) {
  base::flat_map<InsecureType, InsecurityMetadata> form_issues;
  const auto& specifics_issues = password_data.password_issues();
  if (specifics_issues.has_leaked_password_issue()) {
    const auto& issue = specifics_issues.leaked_password_issue();
    form_issues[InsecureType::kLeaked] = InsecurityMetadata(
        ConvertToBaseTime(issue.date_first_detection_microseconds()),
        IsMuted(issue.is_muted()));
  }
  if (specifics_issues.has_reused_password_issue()) {
    const auto& issue = specifics_issues.reused_password_issue();
    form_issues[InsecureType::kReused] = InsecurityMetadata(
        ConvertToBaseTime(issue.date_first_detection_microseconds()),
        IsMuted(issue.is_muted()));
  }
  if (specifics_issues.has_weak_password_issue()) {
    const auto& issue = specifics_issues.weak_password_issue();
    form_issues[InsecureType::kWeak] = InsecurityMetadata(
        ConvertToBaseTime(issue.date_first_detection_microseconds()),
        IsMuted(issue.is_muted()));
  }
  if (specifics_issues.has_phished_password_issue()) {
    const auto& issue = specifics_issues.phished_password_issue();
    form_issues[InsecureType::kPhished] = InsecurityMetadata(
        ConvertToBaseTime(issue.date_first_detection_microseconds()),
        IsMuted(issue.is_muted()));
  }
  return form_issues;
}

sync_pb::PasswordSpecificsData TrimPasswordSpecificsDataForCaching(
    const sync_pb::PasswordSpecificsData& password_specifics_data) {
  sync_pb::PasswordSpecificsData trimmed_password_data =
      sync_pb::PasswordSpecificsData(password_specifics_data);
  trimmed_password_data.clear_scheme();
  trimmed_password_data.clear_signon_realm();
  trimmed_password_data.clear_origin();
  trimmed_password_data.clear_action();
  trimmed_password_data.clear_username_element();
  trimmed_password_data.clear_username_value();
  trimmed_password_data.clear_password_element();
  trimmed_password_data.clear_password_value();
  trimmed_password_data.clear_date_created();
  trimmed_password_data.clear_blacklisted();
  trimmed_password_data.clear_type();
  trimmed_password_data.clear_times_used();
  trimmed_password_data.clear_display_name();
  trimmed_password_data.clear_avatar_url();
  trimmed_password_data.clear_federation_url();
  trimmed_password_data.clear_date_last_used();
  trimmed_password_data.clear_password_issues();
  trimmed_password_data.clear_date_password_modified_windows_epoch_micros();
  return trimmed_password_data;
}

sync_pb::PasswordSpecifics SpecificsFromPassword(
    const PasswordForm& password_form) {
  sync_pb::PasswordSpecifics specifics;
  *specifics.mutable_client_only_encrypted_data() =
      SpecificsDataFromPassword(password_form);

  // WARNING: if you are adding support for new `PasswordSpecificsData` fields,
  // you need to update following functions accordingly:
  // `TrimPasswordSpecificsDataForCaching`
  // `TrimRemoteSpecificsForCachingPreservesOnlyUnknownFields`
  DCHECK_EQ(0u, TrimPasswordSpecificsDataForCaching(
                    specifics.client_only_encrypted_data())
                    .ByteSizeLong());
  return specifics;
}

sync_pb::PasswordSpecificsData SpecificsDataFromPassword(
    const PasswordForm& password_form) {
  sync_pb::PasswordSpecificsData password_data;
  password_data.set_scheme(static_cast<int>(password_form.scheme));
  password_data.set_signon_realm(password_form.signon_realm);
  password_data.set_origin(password_form.url.spec());
  password_data.set_action(password_form.action.spec());
  password_data.set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data.set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data.set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data.set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data.set_date_last_used(
      password_form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_date_password_modified_windows_epoch_micros(
      password_form.date_password_modified.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  password_data.set_date_created(
      password_form.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data.set_blacklisted(password_form.blocked_by_user);
  password_data.set_type(static_cast<int>(password_form.type));
  password_data.set_times_used(password_form.times_used);
  password_data.set_display_name(base::UTF16ToUTF8(password_form.display_name));
  password_data.set_avatar_url(password_form.icon_url.spec());
  password_data.set_federation_url(
      password_form.federation_origin.opaque()
          ? std::string()
          : password_form.federation_origin.Serialize());
  *password_data.mutable_password_issues() =
      PasswordIssuesMapToProto(password_form.password_issues);

  return password_data;
}

void SerializeSignatureRelevantMembersInFormData(
    const FormData& form_data,
    base::Value::Dict& serialized_data) {
  // Stored FormData is used only for signature calculations, therefore only
  // members that are used for signature calculation are stored.
  serialized_data.Set(kNameKey, form_data.name);
  serialized_data.Set(kUrlKey, form_data.url.spec());
  serialized_data.Set(kActionKey, form_data.action.spec());

  base::Value::List serialized_fields;
  for (const auto& field : form_data.fields) {
    base::Value::Dict serialized_field;
    // Stored FormFieldData is used only for signature calculations, therefore
    // only members that are used for signature calculation are stored.
    serialized_field.Set(kNameKey, field.name);
    serialized_field.Set(kFormControlTypeKey, field.form_control_type);
    serialized_fields.Append(std::move(serialized_field));
  }
  serialized_data.Set(kFieldsKey, std::move(serialized_fields));
}

std::string SerializeOpaqueLocalData(const PasswordForm& password_form) {
  base::Value::Dict local_data_json;
  local_data_json.Set(kSkipZeroClickKey, password_form.skip_zero_click);

  base::Value::Dict serialized_form_data;
  SerializeSignatureRelevantMembersInFormData(password_form.form_data,
                                              serialized_form_data);
  local_data_json.Set(kFormDataKey, std::move(serialized_form_data));

  std::string serialized_local_data;
  JSONStringValueSerializer serializer(&serialized_local_data);
  serializer.Serialize(local_data_json);
  return serialized_local_data;
}

sync_pb::PasswordWithLocalData PasswordWithLocalDataFromPassword(
    const PasswordForm& password_form) {
  sync_pb::PasswordWithLocalData password_with_local_data;

  *password_with_local_data.mutable_password_specifics_data() =
      SpecificsDataFromPassword(password_form);

  auto* local_data = password_with_local_data.mutable_local_data();
  local_data->set_opaque_metadata(SerializeOpaqueLocalData(password_form));
  if (!password_form.previously_associated_sync_account_email.empty()) {
    local_data->set_previously_associated_sync_account_email(
        password_form.previously_associated_sync_account_email);
  }

  return password_with_local_data;
}

PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password_data) {
  PasswordForm password;
  password.scheme = static_cast<PasswordForm::Scheme>(password_data.scheme());
  password.signon_realm = password_data.signon_realm();
  password.url = GURL(password_data.origin());
  password.action = GURL(password_data.action());
  password.username_element =
      base::UTF8ToUTF16(password_data.username_element());
  password.password_element =
      base::UTF8ToUTF16(password_data.password_element());
  password.username_value = base::UTF8ToUTF16(password_data.username_value());
  password.password_value = base::UTF8ToUTF16(password_data.password_value());
  if (password_data.has_date_last_used()) {
    password.date_last_used = ConvertToBaseTime(password_data.date_last_used());
  } else if (password_data.preferred()) {
    // For legacy passwords that don't have the |date_last_used| field set, we
    // should it similar to the logic in login database migration.
    password.date_last_used =
        base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
  }

  password.date_password_modified = ConvertToBaseTime(
      password_data.has_date_password_modified_windows_epoch_micros()
          ? password_data.date_password_modified_windows_epoch_micros()
          : password_data.date_created());
  password.date_created = ConvertToBaseTime(password_data.date_created());
  password.blocked_by_user = password_data.blacklisted();
  password.type = static_cast<PasswordForm::Type>(password_data.type());
  password.times_used = password_data.times_used();
  password.display_name = base::UTF8ToUTF16(password_data.display_name());
  password.icon_url = GURL(password_data.avatar_url());
  password.federation_origin =
      url::Origin::Create(GURL(password_data.federation_url()));
  password.password_issues = PasswordIssuesMapFromProto(password_data);

  return password;
}

bool DeserializeFormData(base::Value::Dict& serialized_data,
                         FormData& form_data) {
  std::string* form_name = serialized_data.FindString(kNameKey);
  std::string* form_url = serialized_data.FindString(kUrlKey);
  std::string* form_action = serialized_data.FindString(kActionKey);
  base::Value::List* fields = serialized_data.FindList(kFieldsKey);
  if (!form_name || !form_url || !form_action || !fields)
    return false;
  form_data.name = base::UTF8ToUTF16(*form_name);
  form_data.url = GURL(*form_url);
  form_data.action = GURL(*form_action);

  for (auto& serialized_field : *fields) {
    base::Value::Dict* serialized_field_dictionary =
        serialized_field.GetIfDict();
    if (!serialized_field_dictionary)
      return false;
    FormFieldData field;
    std::string* field_name = serialized_field_dictionary->FindString(kNameKey);
    std::string* field_type =
        serialized_field_dictionary->FindString(kFormControlTypeKey);
    if (!field_name || !field_type)
      return false;
    field.name = base::UTF8ToUTF16(*field_name);
    field.form_control_type = *field_type;
    form_data.fields.push_back(field);
  }
  return true;
}

void DeserializeOpaqueLocalData(const std::string& opaque_metadata,
                                PasswordForm& password_form) {
  JSONStringValueDeserializer json_deserializer(opaque_metadata);
  std::unique_ptr<base::Value> root(
      json_deserializer.Deserialize(nullptr, nullptr));
  if (!root.get() || !root->is_dict())
    return;

  base::Value::Dict serialized_data(std::move(root->GetDict()));
  auto skip_zero_click = serialized_data.FindBool(kSkipZeroClickKey);
  auto* serialized_form_data = serialized_data.FindDict(kFormDataKey);
  if (!skip_zero_click.has_value() || !serialized_form_data)
    return;
  FormData form_data;
  if (!DeserializeFormData(*serialized_form_data, form_data))
    return;
  password_form.skip_zero_click = *skip_zero_click;
  password_form.form_data = std::move(form_data);
}

PasswordForm PasswordFromProtoWithLocalData(
    const sync_pb::PasswordWithLocalData& password) {
  PasswordForm form = PasswordFromSpecifics(password.password_specifics_data());
  form.previously_associated_sync_account_email =
      password.local_data().previously_associated_sync_account_email();
  DeserializeOpaqueLocalData(password.local_data().opaque_metadata(), form);
  return form;
}

std::vector<PasswordForm> PasswordVectorFromListResult(
    const sync_pb::ListPasswordsResult& list_result) {
  std::vector<PasswordForm> forms;
  for (const sync_pb::PasswordWithLocalData& password :
       list_result.password_data()) {
    forms.push_back(PasswordFromProtoWithLocalData(password));
  }
  return forms;
}

}  // namespace password_manager
