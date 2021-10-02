// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_proto_utils.h"

#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/sync/protocol/list_passwords_result.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/password_with_local_data.pb.h"

namespace password_manager {

namespace {

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
  for (const auto& form_issue : form_password_issues) {
    sync_pb::PasswordSpecificsData::PasswordIssues::PasswordIssue issue;
    issue.set_date_first_detection_microseconds(
        form_issue.second.create_time.ToDeltaSinceWindowsEpoch()
            .InMicroseconds());
    issue.set_is_muted(form_issue.second.is_muted.value());
    switch (form_issue.first) {
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

sync_pb::PasswordSpecifics SpecificsFromPassword(
    const PasswordForm& password_form) {
  sync_pb::PasswordSpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_client_only_encrypted_data();
  password_data->set_scheme(static_cast<int>(password_form.scheme));
  password_data->set_signon_realm(password_form.signon_realm);
  password_data->set_origin(password_form.url.spec());
  password_data->set_action(password_form.action.spec());
  password_data->set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data->set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data->set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data->set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data->set_date_last_used(
      password_form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data->set_date_password_modified_windows_epoch_micros(
      password_form.date_password_modified.ToDeltaSinceWindowsEpoch()
          .InMicroseconds());
  password_data->set_date_created(
      password_form.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data->set_blacklisted(password_form.blocked_by_user);
  password_data->set_type(static_cast<int>(password_form.type));
  password_data->set_times_used(password_form.times_used);
  password_data->set_display_name(
      base::UTF16ToUTF8(password_form.display_name));
  password_data->set_avatar_url(password_form.icon_url.spec());
  password_data->set_federation_url(
      password_form.federation_origin.opaque()
          ? std::string()
          : password_form.federation_origin.Serialize());
  *password_data->mutable_password_issues() =
      PasswordIssuesMapToProto(password_form.password_issues);

  return specifics;
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

PasswordForm PasswordFromProtoWithLocalData(
    const sync_pb::PasswordWithLocalData& password) {
  PasswordForm form = PasswordFromSpecifics(password.password_specifics_data());
  // TODO(crbug.com/1229654): Consider password.local_chrome_data().
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
