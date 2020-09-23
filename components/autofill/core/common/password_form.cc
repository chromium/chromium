// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form.h"

#include <algorithm>
#include <ostream>
#include <sstream>

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace autofill {

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

base::string16 ValueElementVectorToString(
    const ValueElementVector& value_element_pairs) {
  std::vector<base::string16> pairs(value_element_pairs.size());
  std::transform(value_element_pairs.begin(), value_element_pairs.end(),
                 pairs.begin(), [](const ValueElementPair& p) {
                   return p.first + base::ASCIIToUTF16("+") + p.second;
                 });
  return base::JoinString(pairs, base::ASCIIToUTF16(", "));
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
  return !new_password_element.empty();
}

bool PasswordForm::IsPossibleChangePasswordFormWithoutUsername() const {
  return IsPossibleChangePasswordForm() && username_element.empty();
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
  return in_store == Store::kAccountStore;
}

bool PasswordForm::HasNonEmptyPasswordValue() const {
  return !password_value.empty() || !new_password_value.empty();
}

bool PasswordForm::operator==(const PasswordForm& form) const {
  return scheme == form.scheme && signon_realm == form.signon_realm &&
         url == form.url && action == form.action &&
         submit_element == form.submit_element &&
         username_element == form.username_element &&
         username_element_renderer_id == form.username_element_renderer_id &&
         username_value == form.username_value &&
         all_possible_usernames == form.all_possible_usernames &&
         all_possible_passwords == form.all_possible_passwords &&
         form_has_autofilled_value == form.form_has_autofilled_value &&
         password_element == form.password_element &&
         password_element_renderer_id == form.password_element_renderer_id &&
         password_value == form.password_value &&
         new_password_element == form.new_password_element &&
         confirmation_password_element_renderer_id ==
             form.confirmation_password_element_renderer_id &&
         confirmation_password_element == form.confirmation_password_element &&
         confirmation_password_element_renderer_id ==
             form.confirmation_password_element_renderer_id &&
         new_password_value == form.new_password_value &&
         date_created == form.date_created && date_synced == form.date_synced &&
         date_last_used == form.date_last_used &&
         blocked_by_user == form.blocked_by_user && type == form.type &&
         times_used == form.times_used &&
         form_data.SameFormAs(form.form_data) &&
         generation_upload_status == form.generation_upload_status &&
         display_name == form.display_name && icon_url == form.icon_url &&
         // We compare the serialization of the origins here, as we want unique
         // origins to compare as '=='.
         federation_origin.Serialize() == form.federation_origin.Serialize() &&
         skip_zero_click == form.skip_zero_click &&
         was_parsed_using_autofill_predictions ==
             form.was_parsed_using_autofill_predictions &&
         is_public_suffix_match == form.is_public_suffix_match &&
         is_affiliation_based_match == form.is_affiliation_based_match &&
         affiliated_web_realm == form.affiliated_web_realm &&
         app_display_name == form.app_display_name &&
         app_icon_url == form.app_icon_url &&
         submission_event == form.submission_event &&
         only_for_fallback == form.only_for_fallback &&
         is_new_password_reliable == form.is_new_password_reliable &&
         in_store == form.in_store &&
         moving_blocked_for_list == form.moving_blocked_for_list;
}

bool PasswordForm::operator!=(const PasswordForm& form) const {
  return !operator==(form);
}

bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right) {
  return (left.signon_realm == right.signon_realm && left.url == right.url &&
          left.username_element == right.username_element &&
          left.username_value == right.username_value &&
          left.password_element == right.password_element);
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
        it_default_key_values.value().Equals(actual_value)) {
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

}  // namespace autofill
