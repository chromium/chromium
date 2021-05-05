// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace password_manager {

namespace {

std::string ToString(PasswordForm::Store in_store) {
  switch (in_store) {
    case PasswordForm::Store::kNotSet:
      return "Not Set";
    case PasswordForm::Store::kProfileStore:
      return "Profile Store";
    case PasswordForm::Store::kAccountStore:
      return "Account Store";
  }
}

std::string ToString(PasswordForm::Scheme scheme) {
  switch (scheme) {
    case PasswordForm::Scheme::kHtml:
      return "HTML";
    case PasswordForm::Scheme::kBasic:
      return "Basic";
    case PasswordForm::Scheme::kDigest:
      return "Digest";
    case PasswordForm::Scheme::kOther:
      return "Other";
    case PasswordForm::Scheme::kUsernameOnly:
      return "UsernameOnly";
  }

  NOTREACHED();
  return std::string();
}

std::string ToString(PasswordForm::Type type) {
  switch (type) {
    case PasswordForm::Type::kManual:
      return "Manual";
    case PasswordForm::Type::kGenerated:
      return "Generated";
    case PasswordForm::Type::kApi:
      return "API";
  }

  NOTREACHED();
  return std::string();
}

std::string ToString(PasswordForm::GenerationUploadStatus status) {
  switch (status) {
    case PasswordForm::GenerationUploadStatus::kNoSignalSent:
      return "No Signal Sent";
    case PasswordForm::GenerationUploadStatus::kPositiveSignalSent:
      return "Positive Signal Sent";
    case PasswordForm::GenerationUploadStatus::kNegativeSignalSent:
      return "Negative Signal Sent";
  }

  NOTREACHED();
  return std::string();
}

// Utility function that creates a std::string from an object supporting the
// ostream operator<<.
template <typename T>
std::string ToString(const T& obj) {
  std::ostringstream ostream;
  ostream << obj;
  return ostream.str();
}

std::u16string ValueElementVectorToString(
    const ValueElementVector& value_element_pairs) {
  std::vector<std::u16string> pairs(value_element_pairs.size());
  std::transform(
      value_element_pairs.begin(), value_element_pairs.end(), pairs.begin(),
      [](const ValueElementPair& p) { return p.first + u"+" + p.second; });
  return base::JoinString(pairs, u", ");
}

// Serializes a PasswordForm to a JSON object. Used only for logging in tests.
void PasswordFormToJSON(const PasswordForm& form,
                        base::DictionaryValue* target) {
  target->SetString("scheme", ToString(form.scheme));
  target->SetString("signon_realm", form.signon_realm);
  target->SetBoolean("is_public_suffix_match", form.is_public_suffix_match);
  target->SetBoolean("is_affiliation_based_match",
                     form.is_affiliation_based_match);
  target->SetString("url", form.url.possibly_invalid_spec());
  target->SetString("action", form.action.possibly_invalid_spec());
  target->SetString("submit_element", form.submit_element);
  target->SetString("username_element", form.username_element);
  target->SetInteger("username_element_renderer_id",
                     form.username_element_renderer_id.value());
  target->SetString("username_value", form.username_value);
  target->SetString("password_element", form.password_element);
  target->SetString("password_value", form.password_value);
  target->SetInteger("password_element_renderer_id",
                     form.password_element_renderer_id.value());
  target->SetString("new_password_element", form.new_password_element);
  target->SetInteger("password_element_renderer_id",
                     form.password_element_renderer_id.value());
  target->SetString("new_password_value", form.new_password_value);
  target->SetString("confirmation_password_element",
                    form.confirmation_password_element);
  target->SetInteger("confirmation_password_element_renderer_id",
                     form.confirmation_password_element_renderer_id.value());
  target->SetString("all_possible_usernames",
                    ValueElementVectorToString(form.all_possible_usernames));
  target->SetString("all_possible_passwords",
                    ValueElementVectorToString(form.all_possible_passwords));
  target->SetBoolean("blocked_by_user", form.blocked_by_user);
  target->SetDouble("date_last_used", form.date_last_used.ToDoubleT());
  target->SetDouble("date_created", form.date_created.ToDoubleT());
  target->SetDouble("date_synced", form.date_synced.ToDoubleT());
  target->SetString("type", ToString(form.type));
  target->SetInteger("times_used", form.times_used);
  target->SetString("form_data", ToString(form.form_data));
  target->SetString("generation_upload_status",
                    ToString(form.generation_upload_status));
  target->SetString("display_name", form.display_name);
  target->SetString("icon_url", form.icon_url.possibly_invalid_spec());
  target->SetString("federation_origin", form.federation_origin.Serialize());
  target->SetBoolean("skip_next_zero_click", form.skip_zero_click);
  target->SetBoolean("was_parsed_using_autofill_predictions",
                     form.was_parsed_using_autofill_predictions);
  target->SetString("affiliated_web_realm", form.affiliated_web_realm);
  target->SetString("app_display_name", form.app_display_name);
  target->SetString("app_icon_url", form.app_icon_url.possibly_invalid_spec());
  target->SetString("submission_event", ToString(form.submission_event));
  target->SetBoolean("only_for_fallback", form.only_for_fallback);
  target->SetBoolean("is_gaia_with_skip_save_password_form",
                     form.form_data.is_gaia_with_skip_save_password_form);
  target->SetBoolean("is_new_password_reliable", form.is_new_password_reliable);
  target->SetString("in_store", ToString(form.in_store));

  std::vector<std::string> hashes;
  hashes.reserve(form.moving_blocked_for_list.size());
  for (const auto& gaia_id_hash : form.moving_blocked_for_list) {
    hashes.push_back(gaia_id_hash.ToBase64());
  }
  target->SetString("moving_blocked_for_list", base::JoinString(hashes, ", "));
}

}  // namespace

PasswordForm::PasswordForm() = default;

PasswordForm::PasswordForm(const PasswordForm& other) = default;

PasswordForm::PasswordForm(PasswordForm&& other) = default;

PasswordForm::~PasswordForm() = default;

PasswordForm& PasswordForm::operator=(const PasswordForm& form) = default;

PasswordForm& PasswordForm::operator=(PasswordForm&& form) = default;

bool PasswordForm::IsPossibleChangePasswordForm() const {
  return !new_password_element_renderer_id.is_null();
}

bool PasswordForm::IsPossibleChangePasswordFormWithoutUsername() const {
  return IsPossibleChangePasswordForm() &&
         username_element_renderer_id.is_null();
}

bool PasswordForm::HasUsernameElement() const {
  return !username_element_renderer_id.is_null();
}

bool PasswordForm::HasPasswordElement() const {
  return !password_element_renderer_id.is_null();
}

bool PasswordForm::HasNewPasswordElement() const {
  return !new_password_element_renderer_id.is_null();
}

bool PasswordForm::IsFederatedCredential() const {
  return !federation_origin.opaque();
}

bool PasswordForm::IsSingleUsername() const {
  return HasUsernameElement() && !HasPasswordElement() &&
         !HasNewPasswordElement();
}

bool PasswordForm::IsUsingAccountStore() const {
  return (in_store & Store::kAccountStore) != Store::kNotSet;
}

bool PasswordForm::IsUsingProfileStore() const {
  return (in_store & Store::kProfileStore) != Store::kNotSet;
}

bool PasswordForm::HasNonEmptyPasswordValue() const {
  return !password_value.empty() || !new_password_value.empty();
}

bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right) {
  return (left.signon_realm == right.signon_realm && left.url == right.url &&
          left.username_element == right.username_element &&
          left.username_value == right.username_value &&
          left.password_element == right.password_element);
}

bool operator==(const PasswordForm& lhs, const PasswordForm& rhs) {
  return lhs.scheme == rhs.scheme && lhs.signon_realm == rhs.signon_realm &&
         lhs.url == rhs.url && lhs.action == rhs.action &&
         lhs.submit_element == rhs.submit_element &&
         lhs.username_element == rhs.username_element &&
         lhs.username_element_renderer_id == rhs.username_element_renderer_id &&
         lhs.username_value == rhs.username_value &&
         lhs.all_possible_usernames == rhs.all_possible_usernames &&
         lhs.all_possible_passwords == rhs.all_possible_passwords &&
         lhs.form_has_autofilled_value == rhs.form_has_autofilled_value &&
         lhs.password_element == rhs.password_element &&
         lhs.password_element_renderer_id == rhs.password_element_renderer_id &&
         lhs.password_value == rhs.password_value &&
         lhs.new_password_element == rhs.new_password_element &&
         lhs.confirmation_password_element_renderer_id ==
             rhs.confirmation_password_element_renderer_id &&
         lhs.confirmation_password_element ==
             rhs.confirmation_password_element &&
         lhs.confirmation_password_element_renderer_id ==
             rhs.confirmation_password_element_renderer_id &&
         lhs.new_password_value == rhs.new_password_value &&
         lhs.date_created == rhs.date_created &&
         lhs.date_synced == rhs.date_synced &&
         lhs.date_last_used == rhs.date_last_used &&
         lhs.blocked_by_user == rhs.blocked_by_user && lhs.type == rhs.type &&
         lhs.times_used == rhs.times_used &&
         lhs.form_data.SameFormAs(rhs.form_data) &&
         lhs.generation_upload_status == rhs.generation_upload_status &&
         lhs.display_name == rhs.display_name && lhs.icon_url == rhs.icon_url &&
         // We compare the serialization of the origins here, as we want unique
         // origins to compare as '=='.
         lhs.federation_origin.Serialize() ==
             rhs.federation_origin.Serialize() &&
         lhs.skip_zero_click == rhs.skip_zero_click &&
         lhs.was_parsed_using_autofill_predictions ==
             rhs.was_parsed_using_autofill_predictions &&
         lhs.is_public_suffix_match == rhs.is_public_suffix_match &&
         lhs.is_affiliation_based_match == rhs.is_affiliation_based_match &&
         lhs.affiliated_web_realm == rhs.affiliated_web_realm &&
         lhs.app_display_name == rhs.app_display_name &&
         lhs.app_icon_url == rhs.app_icon_url &&
         lhs.submission_event == rhs.submission_event &&
         lhs.only_for_fallback == rhs.only_for_fallback &&
         lhs.is_new_password_reliable == rhs.is_new_password_reliable &&
         lhs.in_store == rhs.in_store &&
         lhs.moving_blocked_for_list == rhs.moving_blocked_for_list;
}

bool operator!=(const PasswordForm& lhs, const PasswordForm& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, PasswordForm::Scheme scheme) {
  return os << ToString(scheme);
}

std::ostream& operator<<(std::ostream& os, const PasswordForm& form) {
  base::DictionaryValue form_json;
  PasswordFormToJSON(form, &form_json);

  // Serialize the default PasswordForm, and remove values from the result that
  // are equal to this to make the results more concise.
  base::DictionaryValue default_form_json;
  PasswordFormToJSON(PasswordForm(), &default_form_json);
  for (base::DictionaryValue::Iterator it_default_key_values(default_form_json);
       !it_default_key_values.IsAtEnd(); it_default_key_values.Advance()) {
    const base::Value* actual_value;
    if (form_json.Get(it_default_key_values.key(), &actual_value) &&
        it_default_key_values.value() == *actual_value) {
      form_json.Remove(it_default_key_values.key(), nullptr);
    }
  }

  std::string form_as_string;
  base::JSONWriter::WriteWithOptions(
      form_json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &form_as_string);
  base::TrimWhitespaceASCII(form_as_string, base::TRIM_ALL, &form_as_string);
  return os << "PasswordForm(" << form_as_string << ")";
}

std::ostream& operator<<(std::ostream& os, PasswordForm* form) {
  return os << "&" << *form;
}

}  // namespace password_manager
