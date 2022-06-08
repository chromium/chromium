// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
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
    case PasswordForm::Type::kFormSubmission:
      return "Form Submission";
    case PasswordForm::Type::kGenerated:
      return "Generated";
    case PasswordForm::Type::kApi:
      return "API";
    case PasswordForm::Type::kManuallyAdded:
      return "Manually Added";
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

std::string ToString(InsecureType insecure_type) {
  switch (insecure_type) {
    case InsecureType::kLeaked:
      return "Leaked";
    case InsecureType::kPhished:
      return "Phished";
    case InsecureType::kWeak:
      return "Weak";
    case InsecureType::kReused:
      return "Reused";
  }
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
void PasswordFormToJSON(const PasswordForm& form, base::Value* target) {
  target->SetStringKey("scheme", ToString(form.scheme));
  target->SetStringKey("signon_realm", form.signon_realm);
  target->SetBoolKey("is_public_suffix_match", form.is_public_suffix_match);
  target->SetBoolKey("is_affiliation_based_match",
                     form.is_affiliation_based_match);
  target->SetStringKey("url", form.url.possibly_invalid_spec());
  target->SetStringKey("action", form.action.possibly_invalid_spec());
  target->SetStringKey("submit_element", form.submit_element);
  target->SetStringKey("username_element", form.username_element);
  target->SetIntKey("username_element_renderer_id",
                    form.username_element_renderer_id.value());
  target->SetStringKey("username_value", form.username_value);
  target->SetStringKey("password_element", form.password_element);
  target->SetStringKey("password_value", form.password_value);
  target->SetIntKey("password_element_renderer_id",
                    form.password_element_renderer_id.value());
  target->SetStringKey("new_password_element", form.new_password_element);
  target->SetIntKey("password_element_renderer_id",
                    form.password_element_renderer_id.value());
  target->SetStringKey("new_password_value", form.new_password_value);
  target->SetStringKey("confirmation_password_element",
                       form.confirmation_password_element);
  target->SetIntKey("confirmation_password_element_renderer_id",
                    form.confirmation_password_element_renderer_id.value());
  target->SetStringKey("all_possible_usernames",
                       ValueElementVectorToString(form.all_possible_usernames));
  target->SetStringKey("all_possible_passwords",
                       ValueElementVectorToString(form.all_possible_passwords));
  target->SetBoolKey("blocked_by_user", form.blocked_by_user);
  target->SetDoubleKey("date_last_used", form.date_last_used.ToDoubleT());
  target->SetDoubleKey("date_password_modified",
                       form.date_password_modified.ToDoubleT());
  target->SetDoubleKey("date_created", form.date_created.ToDoubleT());
  target->SetStringKey("type", ToString(form.type));
  target->SetIntKey("times_used", form.times_used);
  target->SetStringKey("form_data", ToString(form.form_data));
  target->SetStringKey("generation_upload_status",
                       ToString(form.generation_upload_status));
  target->SetStringKey("display_name", form.display_name);
  target->SetStringKey("icon_url", form.icon_url.possibly_invalid_spec());
  target->SetStringKey("federation_origin", form.federation_origin.Serialize());
  target->SetBoolKey("skip_next_zero_click", form.skip_zero_click);
  target->SetBoolKey("was_parsed_using_autofill_predictions",
                     form.was_parsed_using_autofill_predictions);
  target->SetStringKey("affiliated_web_realm", form.affiliated_web_realm);
  target->SetStringKey("app_display_name", form.app_display_name);
  target->SetStringKey("app_icon_url",
                       form.app_icon_url.possibly_invalid_spec());
  target->SetStringKey("submission_event", ToString(form.submission_event));
  target->SetBoolKey("only_for_fallback", form.only_for_fallback);
  target->SetBoolKey("is_gaia_with_skip_save_password_form",
                     form.form_data.is_gaia_with_skip_save_password_form);
  target->SetBoolKey("is_new_password_reliable", form.is_new_password_reliable);
  target->SetStringKey("in_store", ToString(form.in_store));

  std::vector<std::string> hashes;
  hashes.reserve(form.moving_blocked_for_list.size());
  for (const auto& gaia_id_hash : form.moving_blocked_for_list) {
    hashes.push_back(gaia_id_hash.ToBase64());
  }

  target->SetStringKey("moving_blocked_for_list",
                       base::JoinString(hashes, ", "));

  std::vector<base::Value> password_issues;
  password_issues.reserve(form.password_issues.size());
  for (const auto& issue : form.password_issues) {
    base::Value issue_value(base::Value::Type::DICTIONARY);
    issue_value.SetStringKey("insecurity_type", ToString(issue.first));
    issue_value.SetKey("create_time",
                       base::TimeToValue(issue.second.create_time));
    issue_value.SetBoolKey("is_muted",
                           static_cast<bool>(issue.second.is_muted));
    password_issues.push_back(std::move(issue_value));
  }

  target->SetKey("password_issues ", base::Value(password_issues));

  std::vector<base::Value> password_notes;
  password_notes.reserve(form.notes.size());
  for (const auto& note : form.notes) {
    base::Value note_value(base::Value::Type::DICTIONARY);
    note_value.SetStringKey("unique_display_name", note.unique_display_name);
    note_value.SetStringKey("value", note.value);
    note_value.SetKey("date_created", base::TimeToValue(note.date_created));
    note_value.SetBoolKey("hide_by_default", note.hide_by_default);
    password_notes.push_back(std::move(note_value));
  }
  target->SetKey("notes", base::Value(password_notes));

  target->SetStringKey("previously_associated_sync_account_email",
                       form.previously_associated_sync_account_email);
}

}  // namespace

InsecurityMetadata::InsecurityMetadata() = default;
InsecurityMetadata::InsecurityMetadata(base::Time create_time, IsMuted is_muted)
    : create_time(create_time), is_muted(is_muted) {}
InsecurityMetadata::InsecurityMetadata(const InsecurityMetadata& rhs) = default;
InsecurityMetadata::~InsecurityMetadata() = default;

bool operator==(const InsecurityMetadata& lhs, const InsecurityMetadata& rhs) {
  return lhs.create_time == rhs.create_time && *lhs.is_muted == *rhs.is_muted;
}

PasswordNote::PasswordNote() = default;

PasswordNote::PasswordNote(std::u16string value, base::Time date_created)
    : value(std::move(value)), date_created(std::move(date_created)) {}

PasswordNote::PasswordNote(std::u16string unique_display_name,
                           std::u16string value,
                           base::Time date_created,
                           bool hide_by_default)
    : unique_display_name(std::move(unique_display_name)),
      value(std::move(value)),
      date_created(date_created),
      hide_by_default(hide_by_default) {}

PasswordNote::PasswordNote(const PasswordNote& rhs) = default;

PasswordNote::PasswordNote(PasswordNote&& rhs) = default;

PasswordNote& PasswordNote::operator=(const PasswordNote& rhs) = default;

PasswordNote& PasswordNote::operator=(PasswordNote&& rhs) = default;

PasswordNote::~PasswordNote() = default;

bool operator==(const PasswordNote& lhs, const PasswordNote& rhs) {
  return lhs.unique_display_name == rhs.unique_display_name &&
         lhs.value == rhs.value && lhs.date_created == rhs.date_created &&
         lhs.hide_by_default == rhs.hide_by_default;
}

bool operator!=(const PasswordNote& lhs, const PasswordNote& rhs) {
  return !(lhs == rhs);
}

PasswordForm::PasswordForm() = default;

PasswordForm::PasswordForm(const PasswordForm& other) = default;

PasswordForm::PasswordForm(PasswordForm&& other) = default;

PasswordForm::~PasswordForm() = default;

PasswordForm& PasswordForm::operator=(const PasswordForm& form) = default;

PasswordForm& PasswordForm::operator=(PasswordForm&& form) = default;

bool PasswordForm::IsLikelySignupForm() const {
  return HasNewPasswordElement() && HasUsernameElement() &&
         !HasPasswordElement();
}

bool PasswordForm::IsLikelyChangePasswordForm() const {
  return HasNewPasswordElement() &&
         (!HasUsernameElement() || HasPasswordElement());
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

bool PasswordForm::IsInsecureCredential(InsecureType insecure_type) const {
  return password_issues.find(insecure_type) != password_issues.end();
}

bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right) {
  return PasswordFormUniqueKey(left) == PasswordFormUniqueKey(right);
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
         lhs.confirmation_password_element ==
             rhs.confirmation_password_element &&
         lhs.confirmation_password_element_renderer_id ==
             rhs.confirmation_password_element_renderer_id &&
         lhs.new_password_value == rhs.new_password_value &&
         lhs.date_created == rhs.date_created &&
         lhs.date_last_used == rhs.date_last_used &&
         lhs.date_password_modified == rhs.date_password_modified &&
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
         lhs.moving_blocked_for_list == rhs.moving_blocked_for_list &&
         lhs.password_issues == rhs.password_issues && lhs.notes == rhs.notes &&
         lhs.previously_associated_sync_account_email ==
             rhs.previously_associated_sync_account_email;
}

bool operator!=(const PasswordForm& lhs, const PasswordForm& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, PasswordForm::Scheme scheme) {
  return os << ToString(scheme);
}

std::ostream& operator<<(std::ostream& os, const PasswordForm& form) {
  base::Value form_json(base::Value::Type::DICTIONARY);
  PasswordFormToJSON(form, &form_json);

  // Serialize the default PasswordForm, and remove values from the result that
  // are equal to this to make the results more concise.
  base::Value default_form_json(base::Value::Type::DICTIONARY);
  PasswordFormToJSON(PasswordForm(), &default_form_json);
  for (auto it_default_key_values : default_form_json.DictItems()) {
    const base::Value* actual_value =
        form_json.FindKey(it_default_key_values.first);
    if (actual_value != nullptr &&
        it_default_key_values.second == *actual_value) {
      form_json.RemoveKey(it_default_key_values.first);
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
