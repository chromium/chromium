// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
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
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
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

  // True if field->form_control_type == "password".
  bool is_password = false;

  // True if field is predicted to be a password.
  bool is_predicted_as_password = false;

  // True if the server predicts that this field is not a password field.
  bool server_hints_not_password = false;

  // True if the server predicts that this field is not a username field.
  bool server_hints_not_username = false;

  Interactability interactability = Interactability::kUnlikely;
};

// Returns true if the |str| contains words related to CVC fields.
bool StringMatchesCVC(const base::string16& str) {
  static const base::NoDestructor<base::string16> kCardCvcReCached(
      base::UTF8ToUTF16(autofill::kCardCvcRe));

  return autofill::MatchesPattern(str, *kCardCvcReCached);
}

// TODO(crbug.com/860700): Remove name and attribute checking once server-side
// provides hints for CVC.
// Returns true if the |field| is suspected to be not the password field.
// The suspicion is based on server-side provided hints and on checking the
// field's id and name for hinting towards a CVC code.
bool IsNotPasswordField(const ProcessedField& field) {
  return field.server_hints_not_password ||
         StringMatchesCVC(field.field->name_attribute) ||
         StringMatchesCVC(field.field->id_attribute) ||
         field.autocomplete_flag == AutocompleteFlag::kCreditCard;
}

// Returns true if the |field| is suspected to be not the username field.
bool IsNotUsernameField(const ProcessedField& field) {
  return field.server_hints_not_username;
}

// Checks if the Finch experiment for offering password generation for
// server-predicted clear-text fields is enabled.
bool IsPasswordGenerationForClearTextFieldsEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::KEnablePasswordGenerationForClearTextFields);
}

// Returns true iff |field_type| is one of password types.
bool IsPasswordPrediction(const CredentialFieldType field_type) {
  switch (field_type) {
    case CredentialFieldType::kUsername:
    case CredentialFieldType::kSingleUsername:
    case CredentialFieldType::kNone:
      return false;
    case CredentialFieldType::kCurrentPassword:
    case CredentialFieldType::kNewPassword:
    case CredentialFieldType::kConfirmationPassword:
      return true;
  }
  NOTREACHED();
  return false;
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

bool DoesStringContainOnlyDigits(const base::string16& s) {
  return std::all_of(s.begin(), s.end(), &base::IsAsciiDigit<base::char16>);
}

// Heuristics to determine that a string is very unlikely to be a username.
bool IsProbablyNotUsername(const base::string16& s) {
  return s.empty() || (s.size() < 3 && DoesStringContainOnlyDigits(s));
}

// Returns |typed_value| if it is not empty, |value| otherwise.
base::string16 GetFieldValue(const FormFieldData& field) {
  return field.typed_value.empty() ? field.value : field.typed_value;
}

// A helper struct that is used to capture significant fields to be used for
// the construction of a PasswordForm.
struct SignificantFields {
  const FormFieldData* username = nullptr;
  const FormFieldData* password = nullptr;
  const FormFieldData* new_password = nullptr;
  const FormFieldData* confirmation_password = nullptr;
  // True if the information about fields could only be derived after relaxing
  // some constraints. The resulting PasswordForm should only be used for
  // fallback UI.
  bool is_fallback = false;

  // True iff the new password field was found with server hints or autocomplete
  // attributes.
  bool is_new_password_reliable = false;

  // True if the current form has only username, but no passwords.
  bool is_single_username = false;

  // Returns true if some password field is present. This is the minimal
  // requirement for a successful creation of a PasswordForm is present.
  bool HasPasswords() const {
    DCHECK(!confirmation_password || new_password)
        << "There is no password to confirm if there is no new password field.";
    return password || new_password;
  }

  void ClearAllPasswordFields() {
    password = nullptr;
    new_password = nullptr;
    confirmation_password = nullptr;
  }
};

// Returns true if |field| is in |significant_fields|.
bool IsFieldInSignificantFields(const SignificantFields& significant_fields,
                                const FormFieldData* field) {
  return significant_fields.username == field ||
         significant_fields.password == field ||
         significant_fields.new_password == field ||
         significant_fields.confirmation_password == field;
}

bool DoesPredictionCorrespondToField(
    const FormFieldData& field,
    const PasswordFieldPrediction& prediction) {
#if defined(OS_IOS)
  return field.unique_id == prediction.unique_id;
#else
  return field.unique_renderer_id == prediction.renderer_id;
#endif
}

// Returns the first element of |fields| which corresponds to |prediction|, or
// null if there is no such element.
ProcessedField* FindField(std::vector<ProcessedField>* processed_fields,
                          const PasswordFieldPrediction& prediction) {
  for (ProcessedField& processed_field : *processed_fields) {
    if (DoesPredictionCorrespondToField(*processed_field.field, prediction))
      return &processed_field;
  }
  return nullptr;
}

// Tries to parse |processed_fields| based on server |predictions|. Uses |mode|
// to decide which of two username hints are relevant, if present.
void ParseUsingPredictions(std::vector<ProcessedField>* processed_fields,
                           const FormPredictions& predictions,
                           FormDataParser::Mode mode,
                           SignificantFields* result) {
  // Following the design from https://goo.gl/Mc2KRe, this code will attempt to
  // understand the special case when there are two usernames hinted by the
  // server. In that case, they are considered the sign-in and sign-up
  // usernames, in the order in which the (only) current password and the first
  // new-password come. If there is another amount of usernames, 0 or 2+ current
  // password fields or no new password field, then the abort switch below is
  // set and simply the first field of each kind is used.
  bool prevent_handling_two_usernames = false;  // the abort switch
  // Whether the first username is for sign-in.
  bool sign_in_username_first = true;
  // First username is stored in |result->username|.
  const FormFieldData* second_username = nullptr;

  for (const PasswordFieldPrediction& prediction : predictions.fields) {
    ProcessedField* processed_field = nullptr;

    CredentialFieldType field_type = DeriveFromServerFieldType(prediction.type);
    bool is_password_prediction = IsPasswordPrediction(field_type);
    if (mode == FormDataParser::Mode::kSaving && is_password_prediction) {
      // TODO(crbug.com/913965): Consider server predictions for password fields
      // in SAVING mode when the server predictions become complete.
      continue;
    }
    switch (field_type) {
      case CredentialFieldType::kUsername:
        if (!result->username) {
          processed_field = FindField(processed_fields, prediction);
          if (processed_field)
            result->username = processed_field->field;
        } else if (!second_username) {
          processed_field = FindField(processed_fields, prediction);
          if (processed_field)
            second_username = processed_field->field;
        } else {
          prevent_handling_two_usernames = true;
        }
        break;
      case CredentialFieldType::kSingleUsername:
        processed_field = FindField(processed_fields, prediction);
        if (processed_field) {
          result->username = processed_field->field;
          result->is_single_username = true;
          result->ClearAllPasswordFields();
          return;
        }
        break;
      case CredentialFieldType::kCurrentPassword:
        if (result->password) {
          prevent_handling_two_usernames = true;
        } else {
          processed_field = FindField(processed_fields, prediction);
          if (processed_field) {
            if (!processed_field->is_password)
              continue;
            result->password = processed_field->field;
          }
        }
        break;
      case CredentialFieldType::kNewPassword:
        // If any (and thus the first) new password comes before the current
        // password, the first username is understood as sign-up, not sign-in.
        if (!result->password)
          sign_in_username_first = false;

        // If multiple hints for new-password fields are given (e.g., because
        // of more fields having the same signature), the first one should be
        // marked as new-password. That way the generation can be offered
        // before the user has thought of and typed their new password
        // elsewhere. See https://crbug.com/902700 for more details.
        if (!result->new_password) {
          processed_field = FindField(processed_fields, prediction);
          if (processed_field) {
            if (!IsPasswordGenerationForClearTextFieldsEnabled() &&
                !processed_field->is_password) {
              continue;
            }
            result->new_password = processed_field->field;
            processed_field->is_predicted_as_password = true;
          }
        }
        break;
      case CredentialFieldType::kConfirmationPassword:
        processed_field = FindField(processed_fields, prediction);
        if (processed_field) {
          if (!IsPasswordGenerationForClearTextFieldsEnabled() &&
              !processed_field->is_password) {
            continue;
          }
          result->confirmation_password = processed_field->field;
          processed_field->is_predicted_as_password = true;
        }
        break;
      case CredentialFieldType::kNone:
        break;
    }
  }

  if (!result->new_password || !result->password)
    prevent_handling_two_usernames = true;

  if (!prevent_handling_two_usernames && second_username) {
    // Now that there are two usernames, |sign_in_username_first| determines
    // which is sign-in and which sign-up.
    const FormFieldData* sign_in = result->username;
    const FormFieldData* sign_up = second_username;
    if (!sign_in_username_first)
      std::swap(sign_in, sign_up);
    // For filling, the sign-in username is relevant, because Chrome should not
    // fill where the credentials first need to be created. For saving, the
    // sign-up username is relevant: if both have values, then the sign-up one
    // was not filled and hence was typed by the user.
    result->username =
        mode == FormDataParser::Mode::kSaving ? sign_up : sign_in;
  }

  // If the server suggests there is a confirmation field but no new password,
  // something went wrong. Sanitize the result.
  if (result->confirmation_password && !result->new_password)
    result->confirmation_password = nullptr;

  // For the use of basic heuristics, also mark CVC fields and NOT_PASSWORD
  // fields as such.
  for (const PasswordFieldPrediction& prediction : predictions.fields) {
    ProcessedField* current_field = FindField(processed_fields, prediction);
    if (!current_field)
      continue;
    if (prediction.type == autofill::CREDIT_CARD_VERIFICATION_CODE ||
        prediction.type == autofill::NOT_PASSWORD) {
      current_field->server_hints_not_password = true;
    } else if (prediction.type == autofill::NOT_USERNAME) {
      current_field->server_hints_not_username = true;
    }
  }
}

// Looks for autocomplete attributes in |processed_fields| and saves predictions
// to |result|. Assumption on the usage autocomplete attributes:
// 1. Not more than 1 field with autocomplete=username.
// 2. Not more than 1 field with autocomplete=current-password.
// 3. Not more than 2 fields with autocomplete=new-password.
// 4. Only password fields have "*-password" attribute and only non-password
//    fields have the "username" attribute.
// If any assumption is violated, the autocomplete attribute is ignored.
void ParseUsingAutocomplete(const std::vector<ProcessedField>& processed_fields,
                            SignificantFields* result) {
  bool new_password_found_by_server = result->new_password;
  const FormFieldData* field_marked_as_username = nullptr;
  int username_fields_found = 0;
  for (const ProcessedField& processed_field : processed_fields) {
    if (IsFieldInSignificantFields(*result, processed_field.field)) {
      // Skip this field because it was already chosen in previous steps.
      continue;
    }
    switch (processed_field.autocomplete_flag) {
      case AutocompleteFlag::kUsername:
        if (processed_field.is_password || result->username ||
            processed_field.server_hints_not_username)
          continue;
        username_fields_found++;
        field_marked_as_username = processed_field.field;
        break;
      case AutocompleteFlag::kCurrentPassword:
        if (!processed_field.is_password || result->password ||
            processed_field.server_hints_not_password)
          continue;
        result->password = processed_field.field;
        break;
      case AutocompleteFlag::kNewPassword:
        if (!processed_field.is_password || new_password_found_by_server ||
            processed_field.server_hints_not_password)
          continue;
        // The first field with autocomplete=new-password is considered to be
        // new_password and the second is confirmation_password.
        if (!result->new_password)
          result->new_password = processed_field.field;
        else if (!result->confirmation_password)
          result->confirmation_password = processed_field.field;
        break;
      case AutocompleteFlag::kCreditCard:
      case AutocompleteFlag::kNone:
        break;
    }
  }
  if (!result->username && username_fields_found == 1)
    result->username = field_marked_as_username;
}

// This computes the "likely" condition from the design https://goo.gl/ERvoEN .
// The |field| is likely to be a password if it is not a CVC field, not
// readonly, etc. |*ignored_readonly| is incremented specifically if this
// function returns false because of the |field| being readonly.
bool IsLikelyPassword(const ProcessedField& field, size_t* ignored_readonly) {
  // Readonly fields can be an indication that filling is useless (e.g., the
  // page might use a virtual keyboard). However, if the field was readonly
  // only temporarily, that makes it still interesting for saving. The fact
  // that a user typed or Chrome filled into that field in the past is an
  // indicator that the readonly was only temporary.
  if (field.field->is_readonly &&
      !(field.field->properties_mask & (FieldPropertiesFlags::USER_TYPED |
                                        FieldPropertiesFlags::AUTOFILLED))) {
    ++*ignored_readonly;
    return false;
  }
  return !IsNotPasswordField(field);
}

// Filters the available passwords from |processed_fields| using these rules:
// (1) Passwords with Interactability below |best_interactability| are removed.
// (2) If |mode| == |kSaving|, passwords with empty values are removed.
// (3) Passwords for which IsLikelyPassword returns false are removed.
// If applying rules (1)-(3) results in a non-empty vector of password fields,
// that vector is returned. Otherwise, only rules (1) and (2) are applied and
// the result returned (even if it is empty).
// Neither of the following output parameters may be null:
// |readonly_status| will be updated according to the processing of the parsed
// fields.
// |is_fallback| is set to true if the filtering rule (3) was not used to
// obtain the result.
std::vector<const FormFieldData*> GetRelevantPasswords(
    const std::vector<ProcessedField>& processed_fields,
    FormDataParser::Mode mode,
    Interactability best_interactability,
    FormDataParser::ReadonlyPasswordFields* readonly_status,
    bool* is_fallback) {
  DCHECK(readonly_status);
  DCHECK(is_fallback);

  // Step 0: filter out all non-password fields.
  std::vector<const ProcessedField*> passwords;
  passwords.reserve(processed_fields.size());
  for (const ProcessedField& processed_field : processed_fields) {
    if (processed_field.is_password)
      passwords.push_back(&processed_field);
  }
  if (passwords.empty())
    return std::vector<const FormFieldData*>();

  // These two counters are used to determine the ReadonlyPasswordFields value
  // corresponding to this form.
  const size_t all_passwords_seen = passwords.size();
  size_t ignored_readonly = 0;

  // Step 1: apply filter criterion (1).
  base::EraseIf(
      passwords, [best_interactability](const ProcessedField* processed_field) {
        return !MatchesInteractability(*processed_field, best_interactability);
      });

  if (mode == FormDataParser::Mode::kSaving) {
    // Step 2: apply filter criterion (2).
    base::EraseIf(passwords, [](const ProcessedField* processed_field) {
      return processed_field->field->value.empty();
    });
  }

  // Step 3: apply filter criterion (3). Keep the current content of
  // |passwords| though, in case it is needed for fallback.
  std::vector<const ProcessedField*> filtered;
  filtered.reserve(passwords.size());
  std::copy_if(passwords.begin(), passwords.end(), std::back_inserter(filtered),
               [&ignored_readonly](const ProcessedField* processed_field) {
                 return IsLikelyPassword(*processed_field, &ignored_readonly);
               });
  // Compute the readonly statistic for metrics.
  DCHECK_LE(ignored_readonly, all_passwords_seen);
  if (ignored_readonly == 0)
    *readonly_status = FormDataParser::ReadonlyPasswordFields::kNoneIgnored;
  else if (ignored_readonly < all_passwords_seen)
    *readonly_status = FormDataParser::ReadonlyPasswordFields::kSomeIgnored;
  else
    *readonly_status = FormDataParser::ReadonlyPasswordFields::kAllIgnored;

  // Ensure that |filtered| contains what needs to be returned...
  if (filtered.empty()) {
    filtered = std::move(passwords);
    *is_fallback = true;
  }

  // ...and strip ProcessedFields down to FormFieldData.
  std::vector<const FormFieldData*> result;
  result.reserve(filtered.size());
  for (const ProcessedField* processed_field : filtered)
    result.push_back(processed_field->field);

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
    Interactability best_interactability,
    bool is_fallback) {
  DCHECK(first_relevant_password != processed_fields.end());

  // For saving filter out empty fields and fields with values which are not
  // username.
  const bool is_saving = mode == FormDataParser::Mode::kSaving;

  // Search through the text input fields preceding |first_relevant_password|
  // and find the closest one focusable and the closest one in general.

  const FormFieldData* focusable_username = nullptr;
  const FormFieldData* username = nullptr;

  // Do reverse search to find the closest candidates preceding the password.
  for (auto it = std::make_reverse_iterator(first_relevant_password);
       it != processed_fields.rend(); ++it) {
    if (it->is_password || it->is_predicted_as_password)
      continue;
    if (!MatchesInteractability(*it, best_interactability))
      continue;
    if (is_saving && IsProbablyNotUsername(it->field->value))
      continue;
    if (!is_fallback && IsNotPasswordField(*it))
      continue;
    if (!is_fallback && IsNotUsernameField(*it)) {
      continue;
    }
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
  return field ? field->unique_renderer_id
               : FormFieldData::kNotSetFormControlRendererId;
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
      if (processed_field.is_password && !IsNotPasswordField(processed_field))
        password_max = std::max(password_max, processed_field.interactability);
    }

    // Try to find password elements (current, new, confirmation) among those
    // with best interactability.
    std::vector<const FormFieldData*> passwords =
        GetRelevantPasswords(processed_fields, mode, password_max,
                             readonly_status, &found_fields->is_fallback);
    if (passwords.empty())
      return;
    LocateSpecificPasswords(passwords, &found_fields->password,
                            &found_fields->new_password,
                            &found_fields->confirmation_password);
    if (!found_fields->HasPasswords())
      return;
    for (auto it = processed_fields.begin(); it != processed_fields.end();
         ++it) {
      if (it->field == passwords[0]) {
        first_relevant_password = it;
        break;
      }
    }
  } else {
    const uint32_t password_ids[] = {
        ExtractUniqueId(found_fields->password),
        ExtractUniqueId(found_fields->new_password),
        ExtractUniqueId(found_fields->confirmation_password)};
    for (auto it = processed_fields.begin(); it != processed_fields.end();
         ++it) {
      if ((it->is_password || it->is_predicted_as_password) &&
          base::Contains(password_ids, it->field->unique_renderer_id)) {
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
    if (!it->is_password && !IsNotPasswordField(*it))
      *username_max = std::max(*username_max, it->interactability);
  }

  found_fields->username = FindUsernameFieldBaseHeuristics(
      processed_fields, first_relevant_password, mode, *username_max,
      found_fields->is_fallback);
  return;
}

// Helper to get the platform specific identifier by which autofill and password
// manager refer to a field. The fuzzing infrastructure doed not run on iOS, so
// the iOS specific parts of PasswordForm are also built on fuzzer enabled
// platforms. See http://crbug.com/896594
string16 GetPlatformSpecificIdentifier(const FormFieldData& field) {
#if defined(OS_IOS)
  return field.unique_id;
#else
  return field.name;
#endif
}

// Set username and password fields in |password_form| based on
// |significant_fields| .
void SetFields(const SignificantFields& significant_fields,
               PasswordForm* password_form) {
#if !defined(OS_IOS)
  password_form->has_renderer_ids = true;
#endif
  if (significant_fields.username) {
    password_form->username_element =
        GetPlatformSpecificIdentifier(*significant_fields.username);
    password_form->username_value = GetFieldValue(*significant_fields.username);
    password_form->username_element_renderer_id =
        significant_fields.username->unique_renderer_id;
  }

  if (significant_fields.password) {
    password_form->password_element =
        GetPlatformSpecificIdentifier(*significant_fields.password);
    password_form->password_value = GetFieldValue(*significant_fields.password);
    password_form->password_element_renderer_id =
        significant_fields.password->unique_renderer_id;
  }

  if (significant_fields.new_password) {
    password_form->new_password_element =
        GetPlatformSpecificIdentifier(*significant_fields.new_password);
    password_form->new_password_value =
        GetFieldValue(*significant_fields.new_password);
    password_form->new_password_element_renderer_id =
        significant_fields.new_password->unique_renderer_id;
  }

  if (significant_fields.confirmation_password) {
    password_form->confirmation_password_element =
        GetPlatformSpecificIdentifier(
            *significant_fields.confirmation_password);
    password_form->confirmation_password_element_renderer_id =
        significant_fields.confirmation_password->unique_renderer_id;
  }
}

// For each relevant field of |fields| computes additional data useful for
// parsing and wraps that in a ProcessedField. Returns the vector of all those
// ProcessedField instances, or an empty vector if there was not a single
// password field. Also, computes the vector of all password values and
// associated element names in |all_possible_passwords|, and similarly for
// usernames and |all_possible_usernames|. If |mode| is |kSaving|, fields with
// empty values are ignored.
std::vector<ProcessedField> ProcessFields(
    const std::vector<FormFieldData>& fields,
    autofill::ValueElementVector* all_possible_passwords,
    autofill::ValueElementVector* all_possible_usernames,
    FormDataParser::Mode mode) {
  DCHECK(all_possible_passwords);
  DCHECK(all_possible_passwords->empty());

  std::vector<ProcessedField> result;
  result.reserve(fields.size());

  // |all_possible_passwords| should only contain each value once.
  // |seen_password_values| ensures that duplicates are ignored.
  std::set<base::StringPiece16> seen_password_values;
  // Similarly for usernames.
  std::set<base::StringPiece16> seen_username_values;

  const bool consider_only_non_empty = mode == FormDataParser::Mode::kSaving;
  for (const FormFieldData& field : fields) {
    if (!field.IsTextInputElement())
      continue;
    if (consider_only_non_empty && field.value.empty())
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

    ProcessedField processed_field = {
        .field = &field, .autocomplete_flag = flag, .is_password = is_password};

    if (field.properties_mask & FieldPropertiesFlags::USER_TYPED)
      processed_field.interactability = Interactability::kCertain;
    else if (field.is_focusable)
      processed_field.interactability = Interactability::kPossible;

    result.push_back(processed_field);
  }

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
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnableOverwritingPlaceholderUsernames))
    return false;

  if (!form_predictions || !significant_fields.username)
    return false;

  uint32_t username_id = significant_fields.username->unique_renderer_id;
  for (const PasswordFieldPrediction& prediction : form_predictions->fields) {
    if (prediction.renderer_id == username_id)
      return prediction.may_use_prefilled_placeholder;
  }
  return false;
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
    const FormData& form_data,
    const SignificantFields& significant_fields,
    autofill::ValueElementVector all_possible_passwords,
    autofill::ValueElementVector all_possible_usernames,
    const base::Optional<FormPredictions>& form_predictions) {
  if (!significant_fields.HasPasswords() &&
      !significant_fields.is_single_username) {
    return nullptr;
  }

  // Create the PasswordForm and set data not related to specific fields.
  auto result = std::make_unique<PasswordForm>();
  result->origin = form_data.url;
  result->signon_realm = GetSignonRealm(form_data.url);
  result->action = form_data.action;
  result->form_data = form_data;
  result->all_possible_passwords = std::move(all_possible_passwords);
  result->all_possible_usernames = std::move(all_possible_usernames);
  result->scheme = PasswordForm::Scheme::kHtml;
  result->preferred = false;
  result->blacklisted_by_user = false;
  result->type = PasswordForm::Type::kManual;
  result->username_may_use_prefilled_placeholder =
      GetMayUsePrefilledPlaceholder(form_predictions, significant_fields);
  result->is_new_password_reliable =
      significant_fields.is_new_password_reliable;
  result->only_for_fallback = significant_fields.is_fallback;
  result->submission_event = form_data.submission_event;

  for (const FormFieldData& field : form_data.fields) {
    if (field.form_control_type == "password" &&
        (field.properties_mask & FieldPropertiesFlags::AUTOFILLED)) {
      result->form_has_autofilled_value = true;
    }
  }

  // Set data related to specific fields.
  SetFields(significant_fields, result.get());
  return result;
}

}  // namespace

FormDataParser::FormDataParser() = default;

FormDataParser::~FormDataParser() = default;

std::unique_ptr<PasswordForm> FormDataParser::Parse(const FormData& form_data,
                                                    Mode mode) {
  if (form_data.fields.size() > kMaxParseableFields)
    return nullptr;

  readonly_status_ = ReadonlyPasswordFields::kNoHeuristics;
  autofill::ValueElementVector all_possible_passwords;
  autofill::ValueElementVector all_possible_usernames;
  std::vector<ProcessedField> processed_fields = ProcessFields(
      form_data.fields, &all_possible_passwords, &all_possible_usernames, mode);

  if (processed_fields.empty())
    return nullptr;

  SignificantFields significant_fields;
  UsernameDetectionMethod username_detection_method =
      UsernameDetectionMethod::kNoUsernameDetected;

  // (1) First, try to parse with server predictions.
  if (predictions_) {
    ParseUsingPredictions(&processed_fields, *predictions_, mode,
                          &significant_fields);
    if (significant_fields.username) {
      username_detection_method =
          UsernameDetectionMethod::kServerSidePrediction;
    }
  }

  // (2) If that failed, try to parse with autocomplete attributes.
  if (!significant_fields.is_single_username) {
    ParseUsingAutocomplete(processed_fields, &significant_fields);
    if (username_detection_method ==
            UsernameDetectionMethod::kNoUsernameDetected &&
        significant_fields.username) {
      username_detection_method =
          UsernameDetectionMethod::kAutocompleteAttribute;
    }
  }

  // Pass the "reliability" information to mark the new-password fields as
  // eligible for automatic password generation. This only makes sense when
  // forms are analysed for filling, because no passwords are generated when the
  // user saves the already entered one.
  if (mode == Mode::kFilling && significant_fields.new_password) {
    significant_fields.is_new_password_reliable = true;
  }

  // (3) Now try to fill the gaps.
  const bool username_found_before_heuristic = significant_fields.username;

  // Try to parse with base heuristic.
  if (!significant_fields.is_single_username) {
    Interactability username_max = Interactability::kUnlikely;
    ParseUsingBaseHeuristics(processed_fields, mode, &significant_fields,
                             &username_max, &readonly_status_);
    if (username_detection_method ==
            UsernameDetectionMethod::kNoUsernameDetected &&
        significant_fields.username) {
      username_detection_method = UsernameDetectionMethod::kBaseHeuristic;
    }

    // Additionally, and based on the best interactability computed by base
    // heuristics, try to improve the username based on the context of the
    // fields, unless the username already came from more reliable types of
    // analysis.
    if (!username_found_before_heuristic) {
      const FormFieldData* username_field_by_context =
          FindUsernameInPredictions(form_data.username_predictions,
                                    processed_fields, username_max);
      if (username_field_by_context &&
          !(mode == FormDataParser::Mode::kSaving &&
            username_field_by_context->value.empty())) {
        significant_fields.username = username_field_by_context;
        if (username_detection_method ==
                UsernameDetectionMethod::kNoUsernameDetected ||
            username_detection_method ==
                UsernameDetectionMethod::kBaseHeuristic) {
          username_detection_method =
              UsernameDetectionMethod::kHtmlBasedClassifier;
        }
      }
    }
  }

  UMA_HISTOGRAM_ENUMERATION("PasswordManager.UsernameDetectionMethod",
                            username_detection_method,
                            UsernameDetectionMethod::kCount);

  return AssemblePasswordForm(form_data, significant_fields,
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
