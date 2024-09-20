// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form.h"

#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace password_manager {

namespace {

std::string ToString(PasswordForm::Store in_store) {
  // It is possible that both flags are set for password forms in best matches.
  if (in_store == (PasswordForm::Store::kProfileStore |
                   PasswordForm::Store::kAccountStore)) {
    return "Account and Profile Store";
  }
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
    case PasswordForm::Type::kImported:
      return "Imported";
    case PasswordForm::Type::kReceivedViaSharing:
      return "ReceivedViaSharing";
  }

  // In old clients type might contain non-enum values and their mapping is
  // unknown.
  return "Unknown";
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

  NOTREACHED_IN_MIGRATION();
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

std::u16string AlternativeElementVectorToString(
    const AlternativeElementVector& value_element_pairs) {
  std::vector<std::u16string> pairs(value_element_pairs.size());
  base::ranges::transform(
      value_element_pairs, pairs.begin(),
      [](const AlternativeElement& p) { return p.value + u"+" + p.name; });
  return base::JoinString(pairs, u", ");
}

// Serializes a PasswordForm to a JSON object. Used only for logging in tests.
void PasswordFormToJSON(const PasswordForm& form, base::Value::Dict& target) {
  target.Set("primary_key",
             form.primary_key.has_value()
                 ? base::NumberToString(form.primary_key.value().value())
                 : "PRIMARY KEY IS MISSING");
  target.Set("scheme", ToString(form.scheme));
  target.Set("signon_realm", form.signon_realm);
  target.Set("match_type", form.match_type.has_value()
                               ? base::NumberToString(
                                     static_cast<int>(form.match_type.value()))
                               : "MATCH TYPE IS MISSING");
  target.Set("url", form.url.possibly_invalid_spec());
  target.Set("action", form.action.possibly_invalid_spec());
  target.Set("submit_element", form.submit_element);
  target.Set("username_element", form.username_element);
  target.Set("username_element_renderer_id",
             base::NumberToString(form.username_element_renderer_id.value()));
  target.Set("username_value", form.username_value);
  target.Set("password_element", form.password_element);
  target.Set("password_value", form.password_value);
  target.Set("password_element_renderer_id",
             base::NumberToString(form.password_element_renderer_id.value()));
  target.Set("new_password_element", form.new_password_element);
  target.Set(
      "new_password_element_renderer_id",
      base::NumberToString(form.new_password_element_renderer_id.value()));
  target.Set("new_password_value", form.new_password_value);
  target.Set("confirmation_password_element",
             form.confirmation_password_element);
  target.Set("confirmation_password_element_renderer_id",
             base::NumberToString(
                 form.confirmation_password_element_renderer_id.value()));
  target.Set("all_alternative_usernames",
             AlternativeElementVectorToString(form.all_alternative_usernames));
  target.Set("all_alternative_passwords",
             AlternativeElementVectorToString(form.all_alternative_passwords));
  target.Set("blocked_by_user", form.blocked_by_user);
  target.Set("date_last_used", form.date_last_used.InSecondsFSinceUnixEpoch());
  target.Set("date_password_modified",
             form.date_password_modified.InSecondsFSinceUnixEpoch());
  target.Set("date_created", form.date_created.InSecondsFSinceUnixEpoch());
  target.Set("type", ToString(form.type));
  target.Set("times_used_in_html_form", form.times_used_in_html_form);
  target.Set("form_data", ToString(form.form_data));
  target.Set("generation_upload_status",
             ToString(form.generation_upload_status));
  target.Set("display_name", form.display_name);
  target.Set("icon_url", form.icon_url.possibly_invalid_spec());
  target.Set("federation_origin", form.federation_origin.Serialize());
  target.Set("skip_next_zero_click", form.skip_zero_click);
  target.Set("was_parsed_using_autofill_predictions",
             form.was_parsed_using_autofill_predictions);
  target.Set("affiliated_web_realm", form.affiliated_web_realm);
  target.Set("app_display_name", form.app_display_name);
  target.Set("app_icon_url", form.app_icon_url.possibly_invalid_spec());
  target.Set("submission_event", ToString(form.submission_event));
  target.Set("only_for_fallback", form.only_for_fallback);
  target.Set("is_gaia_with_skip_save_password_form",
             form.form_data.is_gaia_with_skip_save_password_form());
  target.Set("in_store", ToString(form.in_store));
  target.Set("server_side_classification_successful",
             form.server_side_classification_successful);
  target.Set("username_may_use_prefilled_placeholder",
             form.username_may_use_prefilled_placeholder);
  target.Set("form_has_autofilled_value", form.form_has_autofilled_value);
  target.Set("keychain_identifier", form.keychain_identifier);
  target.Set("accepts_webauthn_credentials", form.accepts_webauthn_credentials);

  std::vector<std::string> hashes;
  hashes.reserve(form.moving_blocked_for_list.size());
  for (const auto& gaia_id_hash : form.moving_blocked_for_list) {
    hashes.push_back(gaia_id_hash.ToBase64());
  }

  target.Set("moving_blocked_for_list", base::JoinString(hashes, ", "));

  base::Value::List password_issues;
  password_issues.reserve(form.password_issues.size());
  for (const auto& issue : form.password_issues) {
    base::Value::Dict issue_value;
    issue_value.Set("insecurity_type", ToString(issue.first));
    issue_value.Set("create_time", base::TimeToValue(issue.second.create_time));
    issue_value.Set("is_muted", static_cast<bool>(issue.second.is_muted));
    password_issues.Append(std::move(issue_value));
  }

  target.Set("password_issues ", std::move(password_issues));

  base::Value::List password_notes;
  password_notes.reserve(form.notes.size());
  for (const auto& note : form.notes) {
    base::Value::Dict note_dict;
    note_dict.Set("unique_display_name", note.unique_display_name);
    note_dict.Set("value", note.value);
    note_dict.Set("date_created", base::TimeToValue(note.date_created));
    note_dict.Set("hide_by_default", note.hide_by_default);
    password_notes.Append(std::move(note_dict));
  }
  target.Set("notes", std::move(password_notes));

  target.Set("previously_associated_sync_account_email",
             form.previously_associated_sync_account_email);

  target.Set("sender_email", form.sender_email);
  target.Set("sender_name", form.sender_name);
  target.Set("sender_profile_image_url",
             form.sender_profile_image_url.possibly_invalid_spec());
  target.Set("date_received", base::TimeToValue(form.date_received));
  target.Set("sharing_notification_displayed",
             form.sharing_notification_displayed);
}

}  // namespace

AlternativeElement::AlternativeElement(
    const AlternativeElement::Value& value,
    autofill::FieldRendererId field_renderer_id,
    const AlternativeElement::Name& name)
    : value(value), field_renderer_id(field_renderer_id), name(name) {}

AlternativeElement::AlternativeElement(const AlternativeElement::Value& value)
    : value(value) {}

AlternativeElement::AlternativeElement(const AlternativeElement& rhs) = default;

AlternativeElement::AlternativeElement(AlternativeElement&& rhs) = default;

AlternativeElement& AlternativeElement::operator=(
    const AlternativeElement& rhs) = default;

AlternativeElement& AlternativeElement::operator=(AlternativeElement&& rhs) =
    default;

AlternativeElement::~AlternativeElement() = default;

std::ostream& operator<<(std::ostream& os, const AlternativeElement& element) {
  base::Value::Dict element_json;
  element_json.Set("value", element.value);
  element_json.Set("field_renderer_id",
                   base::NumberToString(element.field_renderer_id.value()));
  element_json.Set("name", element.name);

  std::string element_as_string;
  base::JSONWriter::WriteWithOptions(
      element_json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &element_as_string);
  base::TrimWhitespaceASCII(element_as_string, base::TRIM_ALL,
                            &element_as_string);
  return os << "AlternativeElement(" << element_as_string << ")";
}

InsecurityMetadata::InsecurityMetadata() = default;
InsecurityMetadata::InsecurityMetadata(
    base::Time create_time,
    IsMuted is_muted,
    TriggerBackendNotification trigger_notification_from_backend)
    : create_time(create_time),
      is_muted(is_muted),
      trigger_notification_from_backend(trigger_notification_from_backend) {}
InsecurityMetadata::InsecurityMetadata(const InsecurityMetadata& rhs) = default;
InsecurityMetadata::~InsecurityMetadata() = default;

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

PasswordForm::PasswordForm() = default;

PasswordForm::PasswordForm(const PasswordForm& other) = default;

PasswordForm::PasswordForm(PasswordForm&& other) = default;

PasswordForm::~PasswordForm() = default;

PasswordForm& PasswordForm::operator=(const PasswordForm& form) = default;

PasswordForm& PasswordForm::operator=(PasswordForm&& form) = default;

bool PasswordForm::IsLikelyLoginForm() const {
  return HasUsernameElement() && HasPasswordElement() &&
         !HasNewPasswordElement();
}

bool PasswordForm::IsLikelySignupForm() const {
  return HasNewPasswordElement() && HasUsernameElement() &&
         !HasPasswordElement();
}

bool PasswordForm::IsLikelyChangePasswordForm() const {
  return HasNewPasswordElement() && HasPasswordElement();
}

bool PasswordForm::IsLikelyResetPasswordForm() const {
  return HasNewPasswordElement() && !HasPasswordElement() &&
         !HasUsernameElement();
}

autofill::PasswordFormClassification::Type PasswordForm::GetPasswordFormType()
    const {
  using enum autofill::PasswordFormClassification::Type;
  if (IsLikelyLoginForm()) {
    return kLoginForm;
  } else if (IsLikelySignupForm()) {
    return kSignupForm;
  } else if (IsLikelyChangePasswordForm()) {
    return kChangePasswordForm;
  } else if (IsLikelyResetPasswordForm()) {
    return kResetPasswordForm;
  } else if (IsSingleUsername()) {
    return kSingleUsernameForm;
  }
  return kNoPasswordForm;
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
  return federation_origin.IsValid();
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

std::u16string PasswordForm::GetNoteWithEmptyUniqueDisplayName() const {
  const auto& note_itr = base::ranges::find_if(
      notes, &std::u16string::empty, &PasswordNote::unique_display_name);
  return note_itr != notes.end() ? note_itr->value : std::u16string();
}

void PasswordForm::SetNoteWithEmptyUniqueDisplayName(
    const std::u16string& new_note_value) {
  const auto& note_itr = base::ranges::find_if(
      notes, &std::u16string::empty, &PasswordNote::unique_display_name);
  // if the old note doesn't exist, the note is just created.
  if (note_itr == notes.end()) {
    notes.emplace_back(new_note_value, base::Time::Now());
    return;
  }
  // Note existed, but it was empty.
  if (note_itr->value.empty()) {
    note_itr->value = new_note_value;
    note_itr->date_created = base::Time::Now();
    return;
  }
  note_itr->value = new_note_value;
}

bool ArePasswordFormUniqueKeysEqual(const PasswordForm& left,
                                    const PasswordForm& right) {
  return PasswordFormUniqueKey(left) == PasswordFormUniqueKey(right);
}

std::ostream& operator<<(std::ostream& os, PasswordForm::Scheme scheme) {
  return os << ToString(scheme);
}

std::ostream& operator<<(std::ostream& os, const PasswordForm& form) {
  base::Value::Dict form_json;
  PasswordFormToJSON(form, form_json);

  // Serialize the default PasswordForm, and remove values from the result that
  // are equal to this to make the results more concise.
  base::Value::Dict default_form_json;
  PasswordFormToJSON(PasswordForm(), default_form_json);
  for (auto it_default_key_values : default_form_json) {
    const base::Value* actual_value =
        form_json.Find(it_default_key_values.first);
    if (actual_value != nullptr &&
        it_default_key_values.second == *actual_value) {
      form_json.Remove(it_default_key_values.first);
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
