// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <ostream>
#include <sstream>

#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/submission_source.h"

namespace autofill {

namespace {

// Serializes a PasswordForm to a JSON object. Used only for logging in tests.
void PasswordFormToJSON(const PasswordForm& form,
                        base::DictionaryValue* target) {
  target->SetInteger("scheme", form.scheme);
  target->SetString("signon_realm", form.signon_realm);
  target->SetBoolean("is_public_suffix_match", form.is_public_suffix_match);
  target->SetBoolean("is_affiliation_based_match",
                     form.is_affiliation_based_match);
  target->SetString("origin", form.origin.possibly_invalid_spec());
  target->SetString("action", form.action.possibly_invalid_spec());
  target->SetString("submit_element", form.submit_element);
  target->SetBoolean("has_renderer_ids", form.has_renderer_ids);
  target->SetString("username_element", form.username_element);
  target->SetInteger("username_element_renderer_id",
                     form.username_element_renderer_id);
  target->SetBoolean("username_marked_by_site", form.username_marked_by_site);
  target->SetString("username_value", form.username_value);
  target->SetString("password_elem", form.password_element);
  target->SetString("password_value", form.password_value);
  target->SetString("new_password_element", form.new_password_element);
  target->SetInteger("password_element_renderer_id",
                     form.password_element_renderer_id);
  target->SetString("new_password_value", form.new_password_value);
  target->SetBoolean("new_password_marked_by_site",
                     form.new_password_marked_by_site);
  target->SetString("other_possible_usernames",
                    ValueElementVectorToString(form.other_possible_usernames));
  target->SetString("all_possible_passwords",
                    ValueElementVectorToString(form.all_possible_passwords));
  target->SetBoolean("blacklisted", form.blacklisted_by_user);
  target->SetBoolean("preferred", form.preferred);
  target->SetDouble("date_created", form.date_created.ToDoubleT());
  target->SetDouble("date_synced", form.date_synced.ToDoubleT());
  target->SetInteger("type", form.type);
  target->SetInteger("times_used", form.times_used);
  std::ostringstream form_data_string_stream;
  form_data_string_stream << form.form_data;
  target->SetString("form_data", form_data_string_stream.str());
  target->SetInteger("generation_upload_status", form.generation_upload_status);
  target->SetString("display_name", form.display_name);
  target->SetString("icon_url", form.icon_url.possibly_invalid_spec());
  target->SetString("federation_origin", form.federation_origin.Serialize());
  target->SetBoolean("skip_next_zero_click", form.skip_zero_click);
  target->SetBoolean("was_parsed_using_autofill_predictions",
                     form.was_parsed_using_autofill_predictions);
  target->SetString("affiliated_web_realm", form.affiliated_web_realm);
  target->SetString("app_display_name", form.app_display_name);
  target->SetString("app_icon_url", form.app_icon_url.possibly_invalid_spec());
  std::ostringstream submission_event_string_stream;
  submission_event_string_stream << form.submission_event;
  target->SetString("submission_event", submission_event_string_stream.str());
  target->SetBoolean("only_for_fallback_saving", form.only_for_fallback_saving);
  target->SetBoolean("is_gaia_with_skip_save_password_form",
                     form.is_gaia_with_skip_save_password_form);
}

}  // namespace

PasswordForm::PasswordForm()
    : scheme(SCHEME_HTML),
      username_marked_by_site(false),
      form_has_autofilled_value(false),
      new_password_marked_by_site(false),
      preferred(false),
      blacklisted_by_user(false),
      type(TYPE_MANUAL),
      times_used(0),
      generation_upload_status(NO_SIGNAL_SENT),
      skip_zero_click(false),
      was_parsed_using_autofill_predictions(false),
      is_public_suffix_match(false),
      is_affiliation_based_match(false),
      submission_event(SubmissionIndicatorEvent::NONE),
      only_for_fallback_saving(false),
      is_gaia_with_skip_save_password_form(false) {}

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

bool PasswordForm::operator==(const PasswordForm& form) const {
  return scheme == form.scheme && signon_realm == form.signon_realm &&
         origin == form.origin && action == form.action &&
         submit_element == form.submit_element &&
         has_renderer_ids == form.has_renderer_ids &&
         username_element == form.username_element &&
         username_element_renderer_id == form.username_element_renderer_id &&
         username_marked_by_site == form.username_marked_by_site &&
         username_value == form.username_value &&
         other_possible_usernames == form.other_possible_usernames &&
         all_possible_passwords == form.all_possible_passwords &&
         form_has_autofilled_value == form.form_has_autofilled_value &&
         password_element == form.password_element &&
         password_element_renderer_id == form.password_element_renderer_id &&
         password_value == form.password_value &&
         new_password_element == form.new_password_element &&
         new_password_marked_by_site == form.new_password_marked_by_site &&
         new_password_value == form.new_password_value &&
         preferred == form.preferred && date_created == form.date_created &&
         date_synced == form.date_synced &&
         blacklisted_by_user == form.blacklisted_by_user && type == form.type &&
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
         only_for_fallback_saving == form.only_for_fallback_saving &&
         is_gaia_with_skip_save_password_form ==
             form.is_gaia_with_skip_save_password_form;
}

bool PasswordForm::operator!=(const PasswordForm& form) const {
  return !operator==(form);
}

bool ArePasswordFormUniqueKeyEqual(const PasswordForm& left,
                                   const PasswordForm& right) {
  return (left.signon_realm == right.signon_realm &&
          left.origin == right.origin &&
          left.username_element == right.username_element &&
          left.username_value == right.username_value &&
          left.password_element == right.password_element);
}

bool LessThanUniqueKey::operator()(
    const std::unique_ptr<PasswordForm>& left,
    const std::unique_ptr<PasswordForm>& right) const {
  int result = left->signon_realm.compare(right->signon_realm);
  if (result)
    return result < 0;

  result = left->username_element.compare(right->username_element);
  if (result)
    return result < 0;

  result = left->username_value.compare(right->username_value);
  if (result)
    return result < 0;

  result = left->password_element.compare(right->password_element);
  if (result)
    return result < 0;

  return left->origin < right->origin;
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

PasswordForm::SubmissionIndicatorEvent ToSubmissionIndicatorEvent(
    SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return PasswordForm::SubmissionIndicatorEvent::NONE;
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return PasswordForm::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;
    case SubmissionSource::XHR_SUCCEEDED:
      return PasswordForm::SubmissionIndicatorEvent::XHR_SUCCEEDED;
    case SubmissionSource::FRAME_DETACHED:
      return PasswordForm::SubmissionIndicatorEvent::FRAME_DETACHED;
    case SubmissionSource::DOM_MUTATION_AFTER_XHR:
      return PasswordForm::SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR;
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return PasswordForm::SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  }
  // Unittests exercise this path, so do not put NOTREACHED() here.
  return PasswordForm::SubmissionIndicatorEvent::NONE;
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

std::ostream& operator<<(
    std::ostream& os,
    PasswordForm::SubmissionIndicatorEvent submission_event) {
  switch (submission_event) {
    case PasswordForm::SubmissionIndicatorEvent::HTML_FORM_SUBMISSION:
      os << "HTML_FORM_SUBMISSION";
      break;
    case PasswordForm::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION:
      os << "SAME_DOCUMENT_NAVIGATION";
      break;
    case PasswordForm::SubmissionIndicatorEvent::XHR_SUCCEEDED:
      os << "XHR_SUCCEEDED";
      break;
    case PasswordForm::SubmissionIndicatorEvent::FRAME_DETACHED:
      os << "FRAME_DETACHED";
      break;
    case PasswordForm::SubmissionIndicatorEvent::DOM_MUTATION_AFTER_XHR:
      os << "DOM_MUTATION_AFTER_XHR";
      break;
    case PasswordForm::SubmissionIndicatorEvent::
        PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD:
      os << "PROVISIONALLY_SAVED_FORM_ON_START_PROVISIONAL_LOAD";
      break;
    default:
      os << "NO_SUBMISSION";
      break;
  }
  return os;
}

}  // namespace autofill
