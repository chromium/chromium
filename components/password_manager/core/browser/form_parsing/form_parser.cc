// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FieldPropertiesFlags;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::string16;

namespace password_manager {

namespace {

constexpr char kAutocompleteUsername[] = "username";
constexpr char kAutocompleteCurrentPassword[] = "current-password";
constexpr char kAutocompleteNewPassword[] = "new-password";
constexpr char kAutocompleteCreditCardPrefix[] = "cc-";

// The susbset of autocomplete flags related to passwords.
enum class AutocompleteFlag {
  kNone,
  kUsername,
  kCurrentPassword,
  kNewPassword,
  // Represents the whole family of cc-* flags.
  kCreditCard
};

// The autocomplete attribute has one of the following structures:
//   [section-*] [shipping|billing] [type_hint] field_type
//   on | off | false
// (see
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#autofilling-form-controls%3A-the-autocomplete-attribute).
// For password forms, only the field_type is relevant. So parsing the attribute
// amounts to just taking the last token.  If that token is one of "username",
// "current-password" or "new-password", this returns an appropriate enum value.
// If the token starts with a "cc-" prefix, this returns kCreditCard.
// Otherwise, returns kNone.
AutocompleteFlag ExtractAutocompleteFlag(const std::string& attribute) {
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(attribute, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty())
    return AutocompleteFlag::kNone;

  const base::StringPiece& field_type = tokens.back();
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteUsername))
    return AutocompleteFlag::kUsername;
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteCurrentPassword))
    return AutocompleteFlag::kCurrentPassword;
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteNewPassword))
    return AutocompleteFlag::kNewPassword;

  if (base::StartsWith(field_type, kAutocompleteCreditCardPrefix,
                       base::CompareCase::SENSITIVE))
    return AutocompleteFlag::kCreditCard;

  return AutocompleteFlag::kNone;
}

// How likely is user interaction for a given field?
// Note: higher numeric values should match higher likeliness to allow using the
// standard operator< for comparison of likeliness.
enum class Interactability {
  // When the field is invisible.
  kUnlikely = 0,
  // When the field is visible/focusable.
  kPossible = 1,
  // When the user actually typed into the field before.
  kCertain = 2,
};

// A wrapper around FormFieldData, carrying some additional data used during
// parsing.
struct ProcessedField {
  // This points to the wrapped FormFieldData.
  const FormFieldData* field;

  // The flag derived from field->autocomplete_attribute.
  AutocompleteFlag autocomplete_flag = AutocompleteFlag::kNone;

  // True iff field->form_control_type == "password".
  bool is_password = false;

  Interactability interactability = Interactability::kUnlikely;
};

// Returns true if the |str| contains words related to CVC fields.
bool StringMatchesCVC(const base::string16& str) {
  static const base::NoDestructor<base::string16> kCardCvcReCached(
      base::UTF8ToUTF16(autofill::kCardCvcRe));

  return autofill::MatchesPattern(str, *kCardCvcReCached);
}

// TODO(crbug.com/860700): Remove once server-side provides hints for CVC
// fields.
// Returns true if the |field|'s name or id hint at the field being a CVC field.
bool IsFieldCVC(const FormFieldData& field) {
  return StringMatchesCVC(field.name) || StringMatchesCVC(field.id);
}

// Returns true iff |processed_field| matches the |interactability_bar|. That is
// when either:
// (1) |processed_field.interactability| is not less than |interactability_bar|,
//     or
// (2) |interactability_bar| is |kCertain|, and |processed_field| was
// autofilled. The second clause helps to handle the case when both Chrome and
// the user contribute to filling a form:
//
// <form>
//   <input type="password" autocomplete="current-password" id="Chrome">
//   <input type="password" autocomplete="new-password" id="user">
// </form>
//
// In the example above, imagine that Chrome filled the field with id=Chrome,
// and the user typed the new password in field with id=user. Then the parser
// should identify that id=Chrome is the current password and id=user is the new
// password. Without clause (2), Chrome would ignore id=Chrome.
bool MatchesInteractability(const ProcessedField& processed_field,
                            Interactability interactability_bar) {
  return (processed_field.interactability >= interactability_bar) ||
         (interactability_bar == Interactability::kCertain &&
          (processed_field.field->properties_mask &
           FieldPropertiesFlags::AUTOFILLED));
}

// A helper struct that is used to capture significant fields to be used for
// the construction of a PasswordForm.
struct SignificantFields {
  const FormFieldData* username = nullptr;
  const FormFieldData* password = nullptr;
  const FormFieldData* new_password = nullptr;
  const FormFieldData* confirmation_password = nullptr;

  // Returns true if some password field is present. This is the minimal
  // requirement for a successful creation of a PasswordForm is present.
  bool HasPasswords() const {
    DCHECK(!confirmation_password || new_password)
        << "There is no password to confirm if there is no new password field.";
    return password || new_password;
  }
};

// Returns the first element of |fields| which has the specified
// |unique_renderer_id|, or null if there is no such element.
const FormFieldData* FindFieldWithUniqueRendererId(
    const std::vector<ProcessedField>& processed_fields,
    uint32_t unique_renderer_id) {
  for (const ProcessedField& processed_field : processed_fields) {
    if (processed_field.field->unique_renderer_id == unique_renderer_id)
      return processed_field.field;
  }
  return nullptr;
}

// Tries to parse |processed_fields| based on server |predictions|.
std::unique_ptr<SignificantFields> ParseUsingPredictions(
    const std::vector<ProcessedField>& processed_fields,
    const FormPredictions& predictions) {
  auto result = std::make_unique<SignificantFields>();
  // Note: The code does not check whether there is at most 1 username, 1
  // current password and at most 2 new passwords. It is assumed that server
  // side predictions are sane.
  for (const auto& prediction : predictions) {
    switch (DeriveFromServerFieldType(prediction.second.type)) {
      case CredentialFieldType::kUsername:
        result->username =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kCurrentPassword:
        result->password =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kNewPassword:
        result->new_password =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kConfirmationPassword:
        result->confirmation_password =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kNone:
        break;
    }
  }
  // If the server suggests there is a confirmation field but no new password,
  // something went wrong. Sanitize the result.
  if (result->confirmation_password && !result->new_password)
    result->confirmation_password = nullptr;

  return result->HasPasswords() ? std::move(result) : nullptr;
}

// Tries to parse |processed_fields| based on autocomplete attributes.
// Assumption on the usage autocomplete attributes:
// 1. Not more than 1 field with autocomplete=username.
// 2. Not more than 1 field with autocomplete=current-password.
// 3. Not more than 2 fields with autocomplete=new-password.
// 4. Only password fields have "*-password" attribute and only non-password
//    fields have the "username" attribute.
// Are these assumptions violated, or is there no password with an autocomplete
// attribute, parsing is unsuccessful. Returns nullptr if parsing is
// unsuccessful.
std::unique_ptr<SignificantFields> ParseUsingAutocomplete(
    const std::vector<ProcessedField>& processed_fields) {
  auto result = std::make_unique<SignificantFields>();
  for (const ProcessedField& processed_field : processed_fields) {
    switch (processed_field.autocomplete_flag) {
      case AutocompleteFlag::kUsername:
        if (processed_field.is_password || result->username)
          return nullptr;
        result->username = processed_field.field;
        break;
      case AutocompleteFlag::kCurrentPassword:
        if (!processed_field.is_password || result->password)
          return nullptr;
        result->password = processed_field.field;
        break;
      case AutocompleteFlag::kNewPassword:
        if (!processed_field.is_password)
          return nullptr;
        // The first field with autocomplete=new-password is considered to be
        // new_password and the second is confirmation_password.
        if (!result->new_password)
          result->new_password = processed_field.field;
        else if (!result->confirmation_password)
          result->confirmation_password = processed_field.field;
        else
          return nullptr;
        break;
      case AutocompleteFlag::kCreditCard:
        NOTREACHED();
        break;
      case AutocompleteFlag::kNone:
        break;
    }
  }

  return result->HasPasswords() ? std::move(result) : nullptr;
}

// Returns only relevant password fields from |processed_fields|. Namely, if
// |mode| == SAVING return only non-empty fields (for saving empty fields are
// useless). This ignores all passwords with Interactability below
// |best_interactability| and also fields with names which sound like CVC
// fields. Stores the iterator to the first relevant password in
// |first_relevant_password|. |readonly_status| will be updated according to the
// processing of the parsed fields; it must not be null.
std::vector<const FormFieldData*> GetRelevantPasswords(
    const std::vector<ProcessedField>& processed_fields,
    FormDataParser::Mode mode,
    Interactability best_interactability,
    std::vector<ProcessedField>::const_iterator* first_relevant_password,
    FormDataParser::ReadonlyPasswordFields* readonly_status) {
  DCHECK(first_relevant_password);
  *first_relevant_password = processed_fields.end();
  std::vector<const FormFieldData*> result;
  result.reserve(processed_fields.size());
  DCHECK(readonly_status);

  const bool consider_only_non_empty = mode == FormDataParser::Mode::kSaving;

  // These two counters are used to determine the ReadonlyPassowrdFields value
  // corresponding to this form.
  size_t all_passwords_seen = 0;
  size_t ignored_readonly = 0;
  for (auto it = processed_fields.begin(); it != processed_fields.end(); ++it) {
    const ProcessedField& processed_field = *it;
    if (!processed_field.is_password)
      continue;
    ++all_passwords_seen;
    if (!MatchesInteractability(processed_field, best_interactability))
      continue;
    if (consider_only_non_empty && processed_field.field->value.empty())
      continue;
    // Readonly fields can be an indication that filling is useless (e.g., the
    // page might use a virtual keyboard). However, if the field was readonly
    // only temporarily, that makes it still interesting for saving. The fact
    // that a user typed or Chrome filled into that field in the past is an
    // indicator that the readonly was only temporary.
    if (processed_field.field->is_readonly &&
        !(processed_field.field->properties_mask &
          (FieldPropertiesFlags::USER_TYPED |
           FieldPropertiesFlags::AUTOFILLED))) {
      ++ignored_readonly;
      continue;
    }
    if (IsFieldCVC(*processed_field.field))
      continue;
    if (*first_relevant_password == processed_fields.end())
      *first_relevant_password = it;
    result.push_back(processed_field.field);
  }

  DCHECK_NE(0u, all_passwords_seen);
  DCHECK_LE(ignored_readonly, all_passwords_seen);
  if (ignored_readonly == 0)
    *readonly_status = FormDataParser::ReadonlyPasswordFields::kNoneIgnored;
  else if (ignored_readonly < all_passwords_seen)
    *readonly_status = FormDataParser::ReadonlyPasswordFields::kSomeIgnored;
  else
    *readonly_status = FormDataParser::ReadonlyPasswordFields::kAllIgnored;

  return result;
}

// Detects different password fields from |passwords|.
void LocateSpecificPasswords(const std::vector<const FormFieldData*>& passwords,
                             const FormFieldData** current_password,
                             const FormFieldData** new_password,
                             const FormFieldData** confirmation_password) {
  DCHECK(current_password);
  DCHECK(!*current_password);
  DCHECK(new_password);
  DCHECK(!*new_password);
  DCHECK(confirmation_password);
  DCHECK(!*confirmation_password);

  switch (passwords.size()) {
    case 1:
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // If the passwords are both empty, it is impossible to tell if they
        // are the old and the new one, or the new one and its confirmation. In
        // that case Chrome errs on the side of filling and classifies them as
        // old & new to allow filling of change password forms.
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      // If there are more than 3 passwords it is not very clear what this form
      // it is. Consider only the first 3 passwords in such case as a
      // best-effort solution.
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value &&
          passwords[0]->value == passwords[2]->value) {
        // All passwords are the same. Assume that the first field is the
        // current password.
        *current_password = passwords[0];
      } else if (passwords[1]->value == passwords[2]->value) {
        // New password is the duplicated one, and comes second; or empty form
        // with at least 3 password fields.
        *current_password = passwords[0];
        *new_password = passwords[1];
        *confirmation_password = passwords[2];
      } else if (passwords[0]->value == passwords[1]->value) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields. Assume that
        // any password fields after the new password contain sensitive
        // information that isn't actually a password (security hint, SSN, etc.)
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which. Let's save the first password.
        // Password selection in a prompt will allow to correct the choice.
        *current_password = passwords[0];
      }
  }
}

// Tries to find username field among text fields from |processed_fields|
// occurring before |first_relevant_password|. Returns nullptr if the username
// is not found. If |mode| is SAVING, ignores all fields with empty values.
// Ignores all fields with interactability less than |best_interactability|.
const FormFieldData* FindUsernameFieldBaseHeuristics(
    const std::vector<ProcessedField>& processed_fields,
    const std::vector<ProcessedField>::const_iterator& first_relevant_password,
    FormDataParser::Mode mode,
    Interactability best_interactability) {
  DCHECK(first_relevant_password != processed_fields.end());

  // For saving filter out empty fields.
  const bool consider_only_non_empty = mode == FormDataParser::Mode::kSaving;

  // Search through the text input fields preceding |first_relevant_password|
  // and find the closest one focusable and the closest one in general.

  const FormFieldData* focusable_username = nullptr;
  const FormFieldData* username = nullptr;
  // Do reverse search to find the closest candidates preceding the password.
  for (auto it = std::make_reverse_iterator(first_relevant_password);
       it != processed_fields.rend(); ++it) {
    if (it->is_password)
      continue;
    if (!MatchesInteractability(*it, best_interactability))
      continue;
    if (consider_only_non_empty && it->field->value.empty())
      continue;
    if (IsFieldCVC(*it->field))
      continue;
    if (!username)
      username = it->field;
    if (it->field->is_focusable) {
      focusable_username = it->field;
      break;
    }
  }

  return focusable_username ? focusable_username : username;
}

// A helper to return a |field|'s unique_renderer_id or
// kNotSetFormControlRendererId if |field| is null.
uint32_t ExtractUniqueId(const FormFieldData* field) {
  return field ? field->unique_renderer_id : FormFieldData::kNotSetFormControlRendererId;
}

// Tries to find the username and password fields in |processed_fields| based
// on the structure (how the fields are ordered). If |mode| is SAVING, only
// considers non-empty fields. The |found_fields| is both an input and output
// argument: if some password field and the username are already present, the
// the function exits early. If something is missing, the function tries to
// complete it. The result is stored back in |found_fields|. The best
// interactability for usernames, which depends on position of the found
// passwords as well, is returned through |username_max| to be used in other
// kinds of analysis. If password fields end up being parsed, |readonly_status|
// will be updated according to that processing.
void ParseUsingBaseHeuristics(
    const std::vector<ProcessedField>& processed_fields,
    FormDataParser::Mode mode,
    SignificantFields* found_fields,
    Interactability* username_max,
    FormDataParser::ReadonlyPasswordFields* readonly_status) {
  // If there is both the username and the minimal set of fields to build a
  // PasswordForm, return early -- no more work to do.
  if (found_fields->HasPasswords() && found_fields->username)
    return;

  // Will point to the password included in |found_field| which is first in the
  // order of fields in |processed_fields|.
  auto first_relevant_password = processed_fields.end();

  if (!found_fields->HasPasswords()) {
    // What is the best interactability among passwords?
    Interactability password_max = Interactability::kUnlikely;
    for (const ProcessedField& processed_field : processed_fields) {
      if (processed_field.is_password)
        password_max = std::max(password_max, processed_field.interactability);
    }

    // Try to find password elements (current, new, confirmation) among those
    // with best interactability.
    first_relevant_password = processed_fields.end();
    std::vector<const FormFieldData*> passwords =
        GetRelevantPasswords(processed_fields, mode, password_max,
                             &first_relevant_password, readonly_status);
    if (passwords.empty())
      return;
    LocateSpecificPasswords(passwords, &found_fields->password,
                            &found_fields->new_password,
                            &found_fields->confirmation_password);
    if (!found_fields->HasPasswords())
      return;
  } else {
    const uint32_t password_ids[] = {
        ExtractUniqueId(found_fields->password),
        ExtractUniqueId(found_fields->new_password),
        ExtractUniqueId(found_fields->confirmation_password)};
    for (auto it = processed_fields.begin(); it != processed_fields.end();
         ++it) {
      if (it->is_password &&
          base::ContainsValue(password_ids, it->field->unique_renderer_id)) {
        first_relevant_password = it;
        break;
      }
    }
  }
  DCHECK(first_relevant_password != processed_fields.end());

  if (found_fields->username)
    return;

  // What is the best interactability among text fields preceding the passwords?
  *username_max = Interactability::kUnlikely;
  for (auto it = processed_fields.begin(); it != first_relevant_password;
       ++it) {
    if (!it->is_password)
      *username_max = std::max(*username_max, it->interactability);
  }

  found_fields->username = FindUsernameFieldBaseHeuristics(
      processed_fields, first_relevant_password, mode, *username_max);
  return;
}

string16 GetPlatformSpecificIdentifier(const FormFieldData& field) {
#if defined(OS_IOS)
  return field.id;
#else
  return field.name;
#endif
}

// Set username and password fields in |password_form| based on
// |significant_fields| .
void SetFields(const SignificantFields& significant_fields,
               PasswordForm* password_form) {
  password_form->has_renderer_ids = true;
  if (significant_fields.username) {
    password_form->username_element =
        GetPlatformSpecificIdentifier(*significant_fields.username);
    password_form->username_value = significant_fields.username->value;
    password_form->username_element_renderer_id =
        significant_fields.username->unique_renderer_id;
  }

  if (significant_fields.password) {
    password_form->password_element =
        GetPlatformSpecificIdentifier(*significant_fields.password);
    password_form->password_value = significant_fields.password->value;
    password_form->password_element_renderer_id =
        significant_fields.password->unique_renderer_id;
  }

  if (significant_fields.new_password) {
    password_form->new_password_element =
        GetPlatformSpecificIdentifier(*significant_fields.new_password);
    password_form->new_password_value = significant_fields.new_password->value;
  }

  if (significant_fields.confirmation_password) {
    password_form->confirmation_password_element =
        GetPlatformSpecificIdentifier(
            *significant_fields.confirmation_password);
  }
}

// For each relevant field of |fields| computes additional data useful for
// parsing and wraps that in a ProcessedField. Returns the vector of all those
// ProcessedField instances, or an empty vector if there was not a single
// password field. Also, computes the vector of all password values and
// associated element names in |all_possible_passwords|, and similarly for
// usernames and |all_possible_usernames|.
std::vector<ProcessedField> ProcessFields(
    const std::vector<FormFieldData>& fields,
    autofill::ValueElementVector* all_possible_passwords,
    autofill::ValueElementVector* all_possible_usernames) {
  DCHECK(all_possible_passwords);
  DCHECK(all_possible_passwords->empty());

  std::vector<ProcessedField> result;
  bool password_field_found = false;

  result.reserve(fields.size());

  // |all_possible_passwords| should only contain each value once.
  // |seen_password_values| ensures that duplicates are ignored.
  std::set<base::StringPiece16> seen_password_values;
  // Similarly for usernames.
  std::set<base::StringPiece16> seen_username_values;

  for (const FormFieldData& field : fields) {
    if (!field.IsTextInputElement())
      continue;

    const bool is_password = field.form_control_type == "password";
    if (!field.value.empty()) {
      std::set<base::StringPiece16>& seen_values =
          is_password ? seen_password_values : seen_username_values;
      autofill::ValueElementVector* all_possible_fields =
          is_password ? all_possible_passwords : all_possible_usernames;
      // Only the field name of the first occurrence is added.
      auto insertion = seen_values.insert(base::StringPiece16(field.value));
      if (insertion.second) {
        // There was no such element in |seen_values|.
        all_possible_fields->push_back({field.value, field.name});
      }
    }

    const AutocompleteFlag flag =
        ExtractAutocompleteFlag(field.autocomplete_attribute);
    if (flag == AutocompleteFlag::kCreditCard)
      continue;

    ProcessedField processed_field = {
        .field = &field, .autocomplete_flag = flag, .is_password = is_password};

    password_field_found |= is_password;

    if (field.properties_mask & FieldPropertiesFlags::USER_TYPED)
      processed_field.interactability = Interactability::kCertain;
    else if (field.is_focusable)
      processed_field.interactability = Interactability::kPossible;

    result.push_back(processed_field);
  }

  if (!password_field_found)
    result.clear();

  return result;
}

// Find the first element in |username_predictions| (i.e. the most reliable
// prediction) that occurs in |processed_fields| and has interactability level
// at least |username_max|.
const FormFieldData* FindUsernameInPredictions(
    const std::vector<uint32_t>& username_predictions,
    const std::vector<ProcessedField>& processed_fields,
    Interactability username_max) {
  for (uint32_t predicted_id : username_predictions) {
    auto iter = std::find_if(
        processed_fields.begin(), processed_fields.end(),
        [predicted_id, username_max](const ProcessedField& processed_field) {
          return processed_field.field->unique_renderer_id == predicted_id &&
                 MatchesInteractability(processed_field, username_max);
        });
    if (iter != processed_fields.end()) {
      return iter->field;
    }
  }
  return nullptr;
}

// Return true if |significant_fields| has an username field and
// |form_predictions| has |may_use_prefilled_placeholder| == true for the
// username field.
bool GetMayUsePrefilledPlaceholder(
    const base::Optional<FormPredictions>& form_predictions,
    const SignificantFields& significant_fields) {
  if (!form_predictions || !significant_fields.username)
    return false;

  uint32_t username_id = significant_fields.username->unique_renderer_id;
  auto it = form_predictions->find(username_id);
  if (it == form_predictions->end())
    return false;
  return it->second.may_use_prefilled_placeholder;
}

// Puts together a PasswordForm, the result of the parsing, based on the
// |form_data| description of the form metadata (e.g., action), the already
// parsed information about what are the |significant_fields|, the list
// |all_possible_passwords| of all non-empty password values which occurred in
// the form and their associated element names, and the list
// |all_possible_usernames| of all non-empty username values which
// occurred in the form and their associated elements. |form_predictions| is
// used to find fields that may have preffilled placeholders.
std::unique_ptr<PasswordForm> AssemblePasswordForm(
    const autofill::FormData& form_data,
    const SignificantFields& significant_fields,
    autofill::ValueElementVector all_possible_passwords,
    autofill::ValueElementVector all_possible_usernames,
    const base::Optional<FormPredictions>& form_predictions) {
  if (!significant_fields.HasPasswords())
    return nullptr;

  // Create the PasswordForm and set data not related to specific fields.
  auto result = std::make_unique<PasswordForm>();
  result->origin = form_data.origin;
  result->signon_realm = GetSignonRealm(form_data.origin);
  result->action = form_data.action;
  result->form_data = form_data;
  result->all_possible_passwords = std::move(all_possible_passwords);
  // TODO(crbug.com/881346) Rename PasswordForm::other_possible_usernames to
  // all_possible_usernames once the old parser is gone.
  result->other_possible_usernames = std::move(all_possible_usernames);
  result->scheme = PasswordForm::SCHEME_HTML;
  result->preferred = false;
  result->blacklisted_by_user = false;
  result->type = PasswordForm::TYPE_MANUAL;
  result->username_may_use_prefilled_placeholder =
      GetMayUsePrefilledPlaceholder(form_predictions, significant_fields);

  // Set data related to specific fields.
  SetFields(significant_fields, result.get());
  return result;
}

}  // namespace

FormDataParser::FormDataParser() = default;

FormDataParser::~FormDataParser() = default;

std::unique_ptr<PasswordForm> FormDataParser::Parse(
    const autofill::FormData& form_data,
    Mode mode) {
  readonly_status_ = ReadonlyPasswordFields::kNoHeuristics;
  autofill::ValueElementVector all_possible_passwords;
  autofill::ValueElementVector all_possible_usernames;
  std::vector<ProcessedField> processed_fields = ProcessFields(
      form_data.fields, &all_possible_passwords, &all_possible_usernames);

  if (processed_fields.empty())
    return nullptr;

  std::unique_ptr<SignificantFields> significant_fields;
  UsernameDetectionMethod username_detection_method =
      UsernameDetectionMethod::kNoUsernameDetected;

  // (1) First, try to parse with server predictions.
  if (predictions_) {
    significant_fields = ParseUsingPredictions(processed_fields, *predictions_);
    if (significant_fields && significant_fields->username) {
      username_detection_method =
          UsernameDetectionMethod::kServerSidePrediction;
    }
  }

  // (2) If that failed, try to parse with autocomplete attributes.
  if (!significant_fields) {
    significant_fields = ParseUsingAutocomplete(processed_fields);
    if (significant_fields && significant_fields->username) {
      username_detection_method =
          UsernameDetectionMethod::kAutocompleteAttribute;
    }
  }

  // (3) Now try to fill the gaps.
  if (!significant_fields)
    significant_fields = std::make_unique<SignificantFields>();

  const bool username_found_before_heuristic = significant_fields->username;

  // Try to parse with base heuristic.
  Interactability username_max = Interactability::kUnlikely;
  ParseUsingBaseHeuristics(processed_fields, mode, significant_fields.get(),
                           &username_max, &readonly_status_);
  if (username_detection_method ==
          UsernameDetectionMethod::kNoUsernameDetected &&
      significant_fields && significant_fields->username) {
    username_detection_method = UsernameDetectionMethod::kBaseHeuristic;
  }

  // Additionally, and based on the best interactability computed by base
  // heuristics, try to improve the username based on the context of the
  // fields, unless the username already came from more reliable types of
  // analysis.
  if (!username_found_before_heuristic &&
      base::FeatureList::IsEnabled(
          password_manager::features::kHtmlBasedUsernameDetector)) {
    const FormFieldData* username_field_by_context = FindUsernameInPredictions(
        form_data.username_predictions, processed_fields, username_max);
    if (username_field_by_context &&
        !(mode == FormDataParser::Mode::kSaving &&
          username_field_by_context->value.empty())) {
      significant_fields->username = username_field_by_context;
      if (username_detection_method ==
              UsernameDetectionMethod::kNoUsernameDetected ||
          username_detection_method ==
              UsernameDetectionMethod::kBaseHeuristic) {
        username_detection_method =
            UsernameDetectionMethod::kHtmlBasedClassifier;
      }
    }
  }

  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UsernameDetectionMethod",
                            username_detection_method,
                            UsernameDetectionMethod::kCount);

  return AssemblePasswordForm(form_data, *significant_fields,
                              std::move(all_possible_passwords),
                              std::move(all_possible_usernames), predictions_);
}

std::string GetSignonRealm(const GURL& url) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  rep.SetPathStr(std::string());
  return url.ReplaceComponents(rep).spec();
}

}  // namespace password_manager
