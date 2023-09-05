// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"

#include <stdint.h>

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

struct AutocompleteParsing {
  AutocompleteFlag flag = AutocompleteFlag::kNone;
  bool accepts_webauthn_credentials = false;
};

// The autocomplete attribute has one of the following structures:
//   [section-*] [shipping|billing] [type_hint] field_type [webauthn]
//   on | off | false
// (see
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#autofilling-form-controls%3A-the-autocomplete-attribute).
// For password forms, only the field_type is relevant. So parsing the attribute
// amounts to just taking the last token.  If that token is one of "username",
// "current-password", "new-password", "one-time-code", this returns an
// appropriate enum value. If the token starts with a "cc-" prefix, this returns
// kCreditCardField. Otherwise, returns kNone. If the webauthn token is present,
// this sets accepts_webauthn_credentials to true.
AutocompleteParsing ParseAutocomplete(const std::string& attribute) {
  AutocompleteParsing result;
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(attribute, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty())
    return result;

  if (base::EqualsCaseInsensitiveASCII(tokens.back(),
                                       constants::kAutocompleteWebAuthn)) {
    result.accepts_webauthn_credentials = true;
    tokens.pop_back();
  }

  if (tokens.empty())
    return result;

  const base::StringPiece& field_type = tokens.back();
  if (base::EqualsCaseInsensitiveASCII(field_type,
                                       constants::kAutocompleteUsername)) {
    result.flag = AutocompleteFlag::kUsername;
  } else if (base::EqualsCaseInsensitiveASCII(
                 field_type, constants::kAutocompleteCurrentPassword)) {
    result.flag = AutocompleteFlag::kCurrentPassword;
  } else if (base::EqualsCaseInsensitiveASCII(
                 field_type, constants::kAutocompleteNewPassword)) {
    result.flag = AutocompleteFlag::kNewPassword;
  } else if (base::EqualsCaseInsensitiveASCII(
                 field_type, constants::kAutocompleteOneTimePassword)) {
    result.flag = AutocompleteFlag::kOneTimeCode;
  } else if (base::StartsWith(field_type,
                              constants::kAutocompleteCreditCardPrefix,
                              base::CompareCase::SENSITIVE)) {
    result.flag = AutocompleteFlag::kCreditCardField;
  }
  return result;
}

// Returns true if the |str| contains words related to CVC fields.
bool StringMatchesCVC(const std::u16string& str) {
  return autofill::MatchesRegex<autofill::kCardCvcRe>(str);
}

// Returns true if the |str| contains words related to SSN fields.
bool StringMatchesSSN(const std::u16string& str) {
  return autofill::MatchesRegex<autofill::kSocialSecurityRe>(str);
}

// Returns true if the |str| contains words related to one time password fields.
bool StringMatchesOTP(const std::u16string& str) {
  return autofill::MatchesRegex<autofill::kOneTimePwdRe>(str);
}

// Returns true if the |str| consists of one repeated non alphanumeric symbol.
// This is likely a result of website modifying the value, and such value should
// not be saved.
bool StringMatchesHiddenValue(const std::u16string& str) {
  return autofill::MatchesRegex<autofill::kHiddenValueRe>(str);
}

// Return true if the |field| is suspected to be a credit card field based on
// server side predictions, autocomplete attribute, and keywords in field's id
// and name.
bool IsCreditCardField(const ProcessedField& field) {
  return field.server_hints_credit_card_field ||
         field.autocomplete_flag == AutocompleteFlag::kCreditCardField ||
         StringMatchesCVC(field.field->name_attribute) ||
         StringMatchesCVC(field.field->id_attribute);
}

// Returns true if the |field| is suspected to be not a password field
// (including CVC fields). The suspicion is based on server-side provided hints
// and on checking the field's id and name for hinting towards a CVC code,
// Social Security Number or one-time password.
bool IsNotPasswordField(const ProcessedField& field,
                        bool* otp_field_detected_with_regex = nullptr) {
  if (IsCreditCardField(field) || field.server_hints_not_password ||
      field.autocomplete_flag == AutocompleteFlag::kOneTimeCode ||
      StringMatchesSSN(field.field->name_attribute) ||
      StringMatchesSSN(field.field->id_attribute)) {
    return true;
  }
  bool is_otp_field = StringMatchesOTP(field.field->name_attribute) ||
                      StringMatchesOTP(field.field->id_attribute);
  if (otp_field_detected_with_regex) {
    *otp_field_detected_with_regex |= is_otp_field;
  }
  return is_otp_field;
}

// Returns true if the |field| is suspected to be not the username field.
bool IsNotUsernameField(const ProcessedField& field) {
  return field.server_hints_not_username;
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
           FieldPropertiesFlags::kAutofilled));
}

bool DoesStringContainOnlyDigits(const std::u16string& s) {
  return base::ranges::all_of(s, &base::IsAsciiDigit<char16_t>);
}

// Heuristics to determine that a string is very unlikely to be a username.
bool IsProbablyNotUsername(const std::u16string& s) {
  return s.empty() || (s.size() < 3 && DoesStringContainOnlyDigits(s));
}

// Returns |user_input| if it is not empty, |value| otherwise.
const std::u16string& GetFieldValue(const FormFieldData& field) {
  return field.user_input.empty() ? field.value : field.user_input;
}

// A helper struct that is used to capture significant fields to be used for
// the construction of a PasswordForm.
struct SignificantFields {
  raw_ptr<const FormFieldData> username = nullptr;
  raw_ptr<const FormFieldData> password = nullptr;
  raw_ptr<const FormFieldData> new_password = nullptr;
  raw_ptr<const FormFieldData> confirmation_password = nullptr;
  // True if the information about fields could only be derived after relaxing
  // some constraints. The resulting PasswordForm should only be used for
  // fallback UI.
  bool is_fallback = false;

  // True iff the new password field was found with server hints or autocomplete
  // attributes.
  bool is_new_password_reliable = false;

  // True if the current form has only username, but no passwords.
  bool is_single_username = false;

  // True if the current form accepts webauthn crendentials from an active
  // webauthn request.
  bool accepts_webauthn_credentials = false;

  // Returns true if some password field is present. This is the minimal
  // requirement for a successful creation of a PasswordForm is present.
  bool HasPasswords() const {
    DCHECK(!confirmation_password || new_password)
        << "There is no password to confirm if there is no new password field.";
    return password || new_password;
  }

  // Returns whether a new password fields without a corresponding confirmation
  // password field was found.
  bool MissesConfirmationPassword() const {
    return new_password && !confirmation_password;
  }

  void ClearAllPasswordFields() {
    password = nullptr;
    new_password = nullptr;
    confirmation_password = nullptr;
  }

  // Detects different password fields from |passwords| and sets |password|,
  // |new_password|, |confirmation_password| if found.
  void LocateSpecificPasswords(
      const std::vector<const FormFieldData*>& passwords) {
    DCHECK(!password);
    DCHECK(!new_password);
    DCHECK(!confirmation_password);

    switch (passwords.size()) {
      case 1:
        password = passwords[0];
        break;
      case 2:
        if (!passwords[0]->value.empty() &&
            passwords[0]->value == passwords[1]->value) {
          // Two identical non-empty passwords: assume we are seeing a new
          // password with a confirmation. This can be either a sign-up form or
          // a password change form that does not ask for the old password.
          new_password = passwords[0];
          confirmation_password = passwords[1];
        } else {
          // Assume first is old password, second is new (no choice but to
          // guess). If the passwords are both empty, it is impossible to tell
          // if they are the old and the new one, or the new one and its
          // confirmation. In that case Chrome errs on the side of filling and
          // classifies them as old & new to allow filling of change password
          // forms.
          password = passwords[0];
          new_password = passwords[1];
        }
        break;
      default:
        // If there are more than 3 passwords it is not very clear what this
        // form it is. Consider only the first 3 passwords in such case as a
        // best-effort solution.
        if (!passwords[0]->value.empty() &&
            passwords[0]->value == passwords[1]->value &&
            passwords[0]->value == passwords[2]->value) {
          // All passwords are the same. Assume that the first field is the
          // current password.
          password = passwords[0];
        } else if (passwords[1]->value == passwords[2]->value) {
          // New password is the duplicated one, and comes second; or empty form
          // with at least 3 password fields.
          password = passwords[0];
          new_password = passwords[1];
          confirmation_password = passwords[2];
        } else if (passwords[0]->value == passwords[1]->value) {
          // It is strange that the new password comes first, but trust more
          // which fields are duplicated than the ordering of fields. Assume
          // that any password fields after the new password contain sensitive
          // information that isn't actually a password (security hint, SSN,
          // etc.)
          new_password = passwords[0];
          confirmation_password = passwords[1];
        } else {
          // Three different passwords, or first and last match with middle
          // different. No idea which is which. Let's save the first password.
          // Password selection in a prompt will allow to correct the choice.
          password = passwords[0];
        }
    }
  }
};

// For debugging.
std::ostream& operator<<(std::ostream& os,
                         const SignificantFields& significant_fields) {
  os << u"SignificantFields(";
  if (significant_fields.username) {
    os << u"username=" << significant_fields.username->name;
  }
  if (significant_fields.password) {
    os << " password=" << significant_fields.password->name;
  }
  if (significant_fields.new_password) {
    os << " new_password=" << significant_fields.new_password->name;
  }
  if (significant_fields.confirmation_password) {
    os << " confirmation_password="
       << significant_fields.confirmation_password->name;
  }
  os << ")";
  return os;
}

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
  return field.unique_renderer_id == prediction.renderer_id;
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

// Given a `new_password` field tries to find a matching confirmation_password
// field in `processed_fields` that succeeds `new_password` and has matching
// interactability and value.
// Returns that field or nullptr if no such field could be found.
const FormFieldData* FindConfirmationPasswordField(
    const std::vector<ProcessedField>& processed_fields,
    const FormFieldData& new_password) {
  auto new_password_field = base::ranges::find(processed_fields, &new_password,
                                               &ProcessedField::field);
  if (new_password_field == processed_fields.end())
    return nullptr;

  // Find a processed field following `new_password_field` with matching
  // interactability and value.
  auto MatchesNewPasswordField = [new_password_field](
                                     const ProcessedField& field) {
    return MatchesInteractability(field, new_password_field->interactability) &&
           GetFieldValue(*new_password_field->field) ==
               GetFieldValue(*field.field);
  };
  auto confirmation_password_field =
      std::find_if(std::next(new_password_field), processed_fields.end(),
                   MatchesNewPasswordField);
  return confirmation_password_field != processed_fields.end()
             ? confirmation_password_field->field.get()
             : nullptr;
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
          base::UmaHistogramBoolean(
              "PasswordManager.SingleUsername."
              "ForgotPasswordServerPredictionUsed",
              prediction.type == autofill::SINGLE_USERNAME_FORGOT_PASSWORD);
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
            result->new_password = processed_field->field;
            processed_field->is_predicted_as_password = true;
          }
        }
        break;
      case CredentialFieldType::kConfirmationPassword:
        processed_field = FindField(processed_fields, prediction);
        if (processed_field) {
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
        prediction.type == autofill::CREDIT_CARD_NUMBER) {
      current_field->server_hints_credit_card_field = true;
    } else if (prediction.type == autofill::NOT_PASSWORD ||
               prediction.type == autofill::ONE_TIME_CODE) {
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
                            FormDataParser::Mode mode,
                            SignificantFields* result) {
  bool new_password_found_by_server = result->new_password;
  bool new_password_found_by_autocomplete = false;
  bool should_ignore_new_password_autocomplete = false;
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
            processed_field.server_hints_not_password ||
            processed_field.server_hints_credit_card_field) {
          continue;
        }
        result->password = processed_field.field;
        break;
      case AutocompleteFlag::kNewPassword:
        if (!processed_field.is_password || new_password_found_by_server ||
            processed_field.server_hints_not_password ||
            processed_field.server_hints_credit_card_field ||
            should_ignore_new_password_autocomplete) {
          continue;
        }
        // The first field with autocomplete=new-password is considered to be
        // new_password and the second is confirmation_password.
        if (!result->new_password) {
          result->new_password = processed_field.field;
          new_password_found_by_autocomplete = true;
        } else if (!result->confirmation_password) {
          // Ignore kNewPassword autocomplete feature if fields that have it
          // have different values in saving mode.
          if (mode == FormDataParser::Mode::kSaving &&
              new_password_found_by_autocomplete &&
              GetFieldValue(*result->new_password) !=
                  GetFieldValue(*processed_field.field)) {
            should_ignore_new_password_autocomplete = true;
            result->new_password = nullptr;
            continue;
          }
          result->confirmation_password = processed_field.field;
        }
        break;
      case AutocompleteFlag::kOneTimeCode:
      case AutocompleteFlag::kCreditCardField:
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
      !(field.field->properties_mask & (FieldPropertiesFlags::kUserTyped |
                                        FieldPropertiesFlags::kAutofilled))) {
    ++*ignored_readonly;
    return false;
  }
  return !IsNotPasswordField(field);
}

// Filters the available passwords from |processed_fields| using these rules:
// (0): Filter out all input fields which type is not password.
// (1) If |mode| == |kFilling|, credit card fields are ignored even for
// fallback.
// (2) Passwords with Interactability below |best_interactability| are removed.
// (3) If |mode| == |kSaving|, passwords with empty values are removed.
// (4) Passwords for which IsLikelyPassword returns false are removed.
// (5) The field parsed as username is removed.
// If applying filters (0)-(1) results in an empty vector, the vector is
// returned.
// If applying filters (0)-(5) results in a non-empty vector of password fields,
// that vector is returned. Otherwise, roll back to the result after applying
// (0)-(3) and return it (with |is_fallback=true|) even if the result is empty.
// Neither of the following output parameters may be null:
// - |readonly_status| will be updated according to the processing of the parsed
// fields.
// - |is_fallback| is set to true if applying all filters removes all fields and
// the method rolls back to the result after (0)-(3).
std::vector<const FormFieldData*> GetRelevantPasswords(
    const std::vector<ProcessedField>& processed_fields,
    FormDataParser::Mode mode,
    Interactability best_interactability,
    FormDataParser::ReadonlyPasswordFields* readonly_status,
    bool* is_fallback,
    const FormFieldData* username) {
  DCHECK(readonly_status);
  DCHECK(is_fallback);

  // Step 0: filter out all input fields which type is not password.
  std::vector<const ProcessedField*> passwords;
  passwords.reserve(processed_fields.size());
  for (const ProcessedField& processed_field : processed_fields) {
    if (processed_field.is_password)
      passwords.push_back(&processed_field);
  }
  // Step 1: filter out credit card fields (e.g. CVC). In the filling mode,
  // don't keep CC fields even for fallback filling and let non-password
  // Autofill handle these fields. Fallback saving is fine because saving UIs of
  // Autofill and the password manager are not mutually exclusive.
  if (mode == FormDataParser::Mode::kFilling &&
      base::FeatureList::IsEnabled(
          password_manager::features::kDisablePasswordsDropdownForCvcFields)) {
    base::EraseIf(passwords, [](const ProcessedField* processed_field) {
      // TODO(crbug/1425423): This code does not use |StringMatchesCVC| because
      // the underlying regex has a high false positive rate, i.e. matches many
      // real password fields. Reconsider this once the regex becomes better.
      return processed_field->server_hints_credit_card_field ||
             processed_field->autocomplete_flag ==
                 AutocompleteFlag::kCreditCardField;
    });
  }
  if (passwords.empty())
    return std::vector<const FormFieldData*>();

  // These two counters are used to determine the ReadonlyPasswordFields value
  // corresponding to this form.
  const size_t all_passwords_seen = passwords.size();
  size_t ignored_readonly = 0;

  // Step 2: apply filter criterion (2).
  base::EraseIf(
      passwords, [best_interactability](const ProcessedField* processed_field) {
        return !MatchesInteractability(*processed_field, best_interactability);
      });

  if (mode == FormDataParser::Mode::kSaving) {
    // Step 3: apply filter criterion (3).
    base::EraseIf(passwords, [](const ProcessedField* processed_field) {
      return GetFieldValue(*processed_field->field).empty();
    });
  }

  // Step 4: apply filter criterion (4). Keep the current content of
  // |passwords| though, in case it is needed for fallback.
  std::vector<const ProcessedField*> filtered;
  filtered.reserve(passwords.size());
  base::ranges::copy_if(
      passwords, std::back_inserter(filtered),
      [&ignored_readonly](const ProcessedField* processed_field) {
        return IsLikelyPassword(*processed_field, &ignored_readonly);
      });

  // Step 5: remove the field parsed as username, if needed.
  if (username && username->IsPasswordInputElement()) {
    base::EraseIf(filtered, [username](const ProcessedField* processed_field) {
      return processed_field->field->unique_renderer_id ==
             username->unique_renderer_id;
    });
  }

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
    if (is_saving && IsProbablyNotUsername(GetFieldValue(*it->field)))
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
// a null renderer ID if |field| is null.
autofill::FieldRendererId ExtractUniqueId(const FormFieldData* field) {
  return field ? field->unique_renderer_id : autofill::FieldRendererId();
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
    // TODO(crbug.com/1382805): The variable is used only for metrics for the
    // new OTP regex launch. Remove the variable after the launch.
    bool otp_field_detected_with_regex = false;
    for (const ProcessedField& processed_field : processed_fields) {
      if (processed_field.is_password &&
          !IsNotPasswordField(processed_field,
                              &otp_field_detected_with_regex)) {
        password_max = std::max(password_max, processed_field.interactability);
      }
    }
    base::UmaHistogramBoolean("PasswordManager.ParserDetectedOtpFieldWithRegex",
                              otp_field_detected_with_regex);

    // Try to find password elements (current, new, confirmation) among those
    // with best interactability.
    std::vector<const FormFieldData*> passwords = GetRelevantPasswords(
        processed_fields, mode, password_max, readonly_status,
        &found_fields->is_fallback, found_fields->username);
    if (passwords.empty())
      return;
    found_fields->LocateSpecificPasswords(passwords);
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
    const autofill::FieldRendererId password_ids[] = {
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

// Set username and password fields in |password_form| based on
// |significant_fields| .
void SetFields(const SignificantFields& significant_fields,
               PasswordForm* password_form) {
  if (significant_fields.username) {
    password_form->username_element = significant_fields.username->name;
    password_form->username_value = GetFieldValue(*significant_fields.username);
    password_form->username_element_renderer_id =
        significant_fields.username->unique_renderer_id;
  }

  if (significant_fields.password) {
    password_form->password_element = significant_fields.password->name;
    password_form->password_value = GetFieldValue(*significant_fields.password);
    password_form->password_element_renderer_id =
        significant_fields.password->unique_renderer_id;
  }

  if (significant_fields.new_password) {
    password_form->new_password_element = significant_fields.new_password->name;
    password_form->new_password_value =
        GetFieldValue(*significant_fields.new_password);
    password_form->new_password_element_renderer_id =
        significant_fields.new_password->unique_renderer_id;
  }

  if (significant_fields.confirmation_password) {
    DCHECK(significant_fields.new_password)
        << "Lone confirmation field (no new password field)"
        << significant_fields;
    password_form->confirmation_password_element =
        significant_fields.confirmation_password->name;
    password_form->confirmation_password_element_renderer_id =
        significant_fields.confirmation_password->unique_renderer_id;
  }
}

// For each relevant field of |fields| computes additional data useful for
// parsing and wraps that in a ProcessedField. Returns the vector of all those
// ProcessedField instances, or an empty vector if there was not a single
// password field. Also, computes the vector of all password values and
// associated element names in |all_alternative_passwords|, and similarly for
// usernames in |all_alternative_usernames|. If |mode| is |kSaving|, fields with
// empty values are ignored.
std::vector<ProcessedField> ProcessFields(
    const std::vector<FormFieldData>& fields,
    AlternativeElementVector* all_alternative_passwords,
    AlternativeElementVector* all_alternative_usernames,
    FormDataParser::Mode mode) {
  CHECK(all_alternative_passwords);
  CHECK(all_alternative_passwords->empty());

  std::vector<ProcessedField> result;
  result.reserve(fields.size());

  // |all_alternative_passwords| should only contain each value once.
  // |seen_password_values| ensures that duplicates are ignored.
  std::set<base::StringPiece16> seen_password_values;
  // Similarly for usernames.
  std::set<base::StringPiece16> seen_username_values;

  const bool consider_only_non_empty = mode == FormDataParser::Mode::kSaving;
  for (const FormFieldData& field : fields) {
    if (!field.IsTextInputElement())
      continue;

    const std::u16string& field_value = GetFieldValue(field);
    if (consider_only_non_empty &&
        (field_value.empty() || StringMatchesHiddenValue(field_value))) {
      continue;
    }

    const bool is_password = field.form_control_type == "password";

    if (!field_value.empty()) {
      std::set<base::StringPiece16>& seen_values =
          is_password ? seen_password_values : seen_username_values;
      AlternativeElementVector* all_alternative_fields =
          is_password ? all_alternative_passwords : all_alternative_usernames;
      // Only the field name of the first occurrence is added.
      auto insertion = seen_values.insert(field_value);
      if (insertion.second) {
        // There was no such element in |seen_values|.
        all_alternative_fields->emplace_back(
            AlternativeElement::Value(field_value), field.unique_renderer_id,
            AlternativeElement::Name(field.name));
      }
    }

    const AutocompleteParsing autocomplete_parsing =
        ParseAutocomplete(field.autocomplete_attribute);

    ProcessedField processed_field = {
        .field = &field,
        .autocomplete_flag = autocomplete_parsing.flag,
        .is_password = is_password,
        .accepts_webauthn_credentials =
            autocomplete_parsing.accepts_webauthn_credentials};

    if (field.properties_mask & FieldPropertiesFlags::kUserTyped)
      processed_field.interactability = Interactability::kCertain;
    else if (field.is_focusable)
      processed_field.interactability = Interactability::kPossible;

    result.push_back(processed_field);
  }

  return result;
}

// Return true if |significant_fields| has an username field and
// |form_predictions| has |may_use_prefilled_placeholder| == true for the
// username field.
bool GetMayUsePrefilledPlaceholder(
    const absl::optional<FormPredictions>& form_predictions,
    const SignificantFields& significant_fields) {
  if (!form_predictions || !significant_fields.username)
    return false;

  autofill::FieldRendererId username_id =
      significant_fields.username->unique_renderer_id;
  for (const PasswordFieldPrediction& prediction : form_predictions->fields) {
    if (prediction.renderer_id == username_id)
      return prediction.may_use_prefilled_placeholder;
  }
  return false;
}

// Puts together a PasswordForm, the result of the parsing, based on the
// |form_data| description of the form metadata (e.g., action), the already
// parsed information about what are the |significant_fields|, the list
// |all_alternative_passwords| of all non-empty password values which occurred
// in the form and their associated element names, and the list
// |all_alternative_usernames| of all non-empty username values which
// occurred in the form and their associated elements. |form_predictions| is
// used to find fields that may have preffilled placeholders.
std::unique_ptr<PasswordForm> AssemblePasswordForm(
    const FormData& form_data,
    const SignificantFields& significant_fields,
    AlternativeElementVector all_alternative_passwords,
    AlternativeElementVector all_alternative_usernames,
    const absl::optional<FormPredictions>& form_predictions) {
  if (!significant_fields.HasPasswords() &&
      !significant_fields.is_single_username &&
      !significant_fields.accepts_webauthn_credentials) {
    return nullptr;
  }

  // Create the PasswordForm and set data not related to specific fields.
  auto result = std::make_unique<PasswordForm>();
  result->url = form_data.url;
  result->signon_realm = GetSignonRealm(form_data.url);
  result->action = form_data.action;
  result->form_data = form_data;
  result->all_alternative_passwords = std::move(all_alternative_passwords);
  result->all_alternative_usernames = std::move(all_alternative_usernames);
  result->scheme = PasswordForm::Scheme::kHtml;
  result->blocked_by_user = false;
  result->type = PasswordForm::Type::kFormSubmission;
  result->server_side_classification_successful = form_predictions.has_value();
  result->username_may_use_prefilled_placeholder =
      GetMayUsePrefilledPlaceholder(form_predictions, significant_fields);
  result->is_new_password_reliable =
      significant_fields.is_new_password_reliable;
  result->only_for_fallback = significant_fields.is_fallback;
  result->submission_event = form_data.submission_event;
  result->accepts_webauthn_credentials =
      significant_fields.accepts_webauthn_credentials;

  for (const FormFieldData& field : form_data.fields) {
    if (field.form_control_type == "password" &&
        (field.properties_mask & FieldPropertiesFlags::kAutofilled)) {
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
  if (!form_data.url.is_valid())
    return nullptr;

  readonly_status_ = ReadonlyPasswordFields::kNoHeuristics;
  AlternativeElementVector all_alternative_passwords;
  AlternativeElementVector all_alternative_usernames;
  std::vector<ProcessedField> processed_fields =
      ProcessFields(form_data.fields, &all_alternative_passwords,
                    &all_alternative_usernames, mode);

  if (processed_fields.empty())
    return nullptr;

  SignificantFields significant_fields;
  UsernameDetectionMethod method = UsernameDetectionMethod::kNoUsernameDetected;

  // (1) First, try to parse with server predictions.
  if (predictions_) {
    ParseUsingPredictions(&processed_fields, *predictions_, mode,
                          &significant_fields);
    if (significant_fields.username) {
      method = UsernameDetectionMethod::kServerSidePrediction;
    }
  }

  // (2) If that failed, try to parse with autocomplete attributes.
  if (!significant_fields.is_single_username) {
    ParseUsingAutocomplete(processed_fields, mode, &significant_fields);
    if (method == UsernameDetectionMethod::kNoUsernameDetected &&
        significant_fields.username) {
      method = UsernameDetectionMethod::kAutocompleteAttribute;
    }
  }

  // (3) Now try to fill the gaps.
  const bool username_found_before_heuristic = significant_fields.username;
  const bool new_password_found_before_heuristic =
      significant_fields.new_password;

  // Try to parse with base heuristic.
  if (!significant_fields.is_single_username) {
    Interactability username_max = Interactability::kUnlikely;
    ParseUsingBaseHeuristics(processed_fields, mode, &significant_fields,
                             &username_max, &readonly_status_);
    if (method == UsernameDetectionMethod::kNoUsernameDetected &&
        significant_fields.username) {
      method = UsernameDetectionMethod::kBaseHeuristic;
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
        if (method == UsernameDetectionMethod::kNoUsernameDetected ||
            method == UsernameDetectionMethod::kBaseHeuristic) {
          method = UsernameDetectionMethod::kHtmlBasedClassifier;
        }
      }
    }

    // If we're in saving mode and have found a new password but not a
    // confirmation password field, try to infer the confirmation password field
    // by trying to find a field that succeeds the new password field and has
    // matching interactability and value. This should only have an effect if
    // the new password field was found using server predictions or autocomplete
    // attributes. In the case of local heuristics we already made use of the
    // field's value to find a confirmation password field, and thus won't find
    // it now if we didn't find it already.
    if (mode == Mode::kSaving &&
        significant_fields.MissesConfirmationPassword()) {
      significant_fields.confirmation_password = FindConfirmationPasswordField(
          processed_fields, *significant_fields.new_password);
    }
  }

  // If no password is found, check if the form is UFF. For now, only consider
  // the case when username is found using autocomplete attribute.
  if (!significant_fields.HasPasswords() &&
      method == UsernameDetectionMethod::kAutocompleteAttribute) {
    significant_fields.is_single_username = true;
  }

  // Pass the "reliability" information to mark the new-password fields as
  // eligible for automatic password generation. This only makes sense when
  // forms are analysed for filling, because no passwords are generated when the
  // user saves the already entered one.
  significant_fields.is_new_password_reliable =
      mode == Mode::kFilling && significant_fields.new_password &&
      new_password_found_before_heuristic;

  if (mode == Mode::kFilling) {
    for (const auto& field : processed_fields) {
      if (field.accepts_webauthn_credentials) {
        significant_fields.accepts_webauthn_credentials = true;
        break;
      }
    }
  }

  base::UmaHistogramEnumeration("PasswordManager.UsernameDetectionMethod",
                                method);

  return AssemblePasswordForm(
      form_data, significant_fields, std::move(all_alternative_passwords),
      std::move(all_alternative_usernames), predictions_);
}

std::string GetSignonRealm(const GURL& url) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  rep.SetPathStr("");
  return url.ReplaceComponents(rep).spec();
}

const FormFieldData* FindUsernameInPredictions(
    const std::vector<autofill::FieldRendererId>& username_predictions,
    const std::vector<ProcessedField>& processed_fields,
    Interactability username_max) {
  for (autofill::FieldRendererId predicted_id : username_predictions) {
    auto iter = base::ranges::find_if(
        processed_fields, [&](const ProcessedField& processed_field) {
          return processed_field.field->unique_renderer_id == predicted_id &&
                 MatchesInteractability(processed_field, username_max);
        });
    if (iter != processed_fields.end()) {
      return iter->field;
    }
  }
  return nullptr;
}

}  // namespace password_manager
