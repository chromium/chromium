// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_DATA_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_DATA_PARSER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace password_manager {

struct PasswordForm;

// The subset of autocomplete flags related to passwords.
enum class AutocompleteFlag {
  kNone,
  kUsername,
  kCurrentPassword,
  kNewPassword,
  // Represents the whole family of cc-* flags.
  kCreditCardField,
  kOneTimeCode
};

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

// This needs to be in sync with the histogram enumeration
// UsernameDetectionMethod, because the values are reported in the
// "PasswordManager.UsernameDetectionMethod" histogram. Don't remove or shift
// existing values in the enum, only append and mark as obsolete as needed.
enum class UsernameDetectionMethod {
  kNoUsernameDetected = 0,
  kBaseHeuristic = 1,
  kHtmlBasedClassifier = 2,
  kAutocompleteAttribute = 3,
  kServerSidePrediction = 4,
  kMaxValue = kServerSidePrediction,
};

// A wrapper around FormFieldData, carrying some additional data used during
// parsing.
struct ProcessedField {
  // This points to the wrapped FormFieldData.
  raw_ptr<const autofill::FormFieldData> field;

  // The flag derived from field->autocomplete_attribute.
  AutocompleteFlag autocomplete_flag = AutocompleteFlag::kNone;

  // True if field->form_control_type ==
  // autofill::FormControlType::kInputPassword (this is also true for fields
  // that have been password field at some point of time).
  bool is_password = false;

  // True if field is predicted to be a password.
  bool is_predicted_as_password = false;

  // True if the server predicts that this field is a non-credential field
  // (either credit card related field or `NOT_PASSWORD`, `NOT_USERNAME`
  // override).
  bool server_hints_non_credential_field = false;

  // True if the field accepts WebAuthn credentials, false otherwise.
  bool accepts_webauthn_credentials = false;

  Interactability interactability = Interactability::kUnlikely;
};

// Wrapper around the parsing result.
struct FormParsingResult {
  FormParsingResult();
  FormParsingResult(
      std::unique_ptr<PasswordForm> password_form,
      UsernameDetectionMethod username_detection_method,
      bool is_new_password_reliable,
      std::vector<autofill::FieldRendererId> suggestion_banned_fields,
      const autofill::FormFieldData* manual_generation_enabled_field);
  FormParsingResult(FormParsingResult&& other);
  ~FormParsingResult();

  // Holistic representation of the password form.
  std::unique_ptr<PasswordForm> password_form = nullptr;

  // How the username in the password form was found. Used for comparison
  // between in-form and out of form (Username First Flow) username detection.
  UsernameDetectionMethod username_detection_method =
      UsernameDetectionMethod::kNoUsernameDetected;

  // True iff the new password field was found with server hints or autocomplete
  // attributes.
  // Only set on form parsing for filling. Used as signal for
  // password generation eligibility.
  bool is_new_password_reliable = false;

  // List of fields that should have no Password Manager filling suggestions.
  std::vector<autofill::FieldRendererId> suggestion_banned_fields;

  // If the parsed form has new password server prediction on the text field,
  // the field will be stored here.
  autofill::FieldRendererId manual_generation_enabled_field;
};

// This class takes care of parsing FormData into PasswordForm and managing
// related metadata.
class FormDataParser {
 public:
  // Denotes the intended use of the result (for filling forms vs. saving
  // captured credentials). This influences whether empty fields are ignored.
  enum class Mode { kFilling, kSaving };

  // Records whether password fields with a "readonly" attribute were ignored
  // during form parsing.
  enum class ReadonlyPasswordFields {
    // Local heuristics, which only consider the readonly attribute, were not
    // used for the parsing. This means that either the form was unsuitable for
    // parsing (e.g., no fields at all), or some of the more trusted methods
    // (server hints, autocomplete attributes) succeeded.
    kNoHeuristics = 0,
    //
    // The rest of the values refer to the case when local heuristics were used.
    // In that case there are always some password fields.
    //
    // No password fields with "readonly" ignored but some password fields
    // present.
    kNoneIgnored = 2,
    // At least one password with "readonly" was ignored and at least one other
    // password field was not ignored (whether readonly or not).
    kSomeIgnored = 3,
    // At least one password with "readonly" was ignored and every password
    // field was ignored because of being readonly.
    kAllIgnored = 4,
  };

  // The parser will give up on parsing any FormData with more than
  // |kMaxParseableFields| fields.
  static const size_t kMaxParseableFields = 10000;

  FormDataParser();

  FormDataParser(const FormDataParser&) = delete;
  FormDataParser& operator=(const FormDataParser&) = delete;

  ~FormDataParser();

  void set_predictions(FormPredictions predictions) {
    predictions_ = std::move(predictions);
  }

  void reset_predictions() { predictions_.reset(); }

  const std::optional<FormPredictions>& predictions() { return predictions_; }

  ReadonlyPasswordFields readonly_status() { return readonly_status_; }

  // Parse DOM information |form_data| into Password Manager's form
  // representation `PasswordForm` and parsing related information. Return
  // {nullptr, UsernameDetectionMethod::kNoUsernameDetected, false} when parsing
  // is unsuccessful.
  FormParsingResult ParseAndReturnParsingResult(
      const autofill::FormData& form_data,
      Mode mode,
      const base::flat_set<std::u16string>& stored_usernames);

  // Parse DOM information `form_data` into Password Manager's form
  // representation `PasswordForm`. Return nullptr when parsing is unsuccessful.
  // Wrapper around `ParseAndReturnParsingResult()`.
  std::unique_ptr<PasswordForm> Parse(
      const autofill::FormData& form_data,
      Mode mode,
      const base::flat_set<std::u16string>& stored_usernames);

 private:
  // Predictions are an optional source of server-side information about field
  // types.
  std::optional<FormPredictions> predictions_;

  // Records whether readonly password fields were seen during the last call to
  // Parse().
  ReadonlyPasswordFields readonly_status_ =
      ReadonlyPasswordFields::kNoHeuristics;
};

// Returns the value of PasswordForm::signon_realm for an HTML form with the
// origin |url|.
std::string GetSignonRealm(const GURL& url);

// Find the first element in |username_predictions| (i.e. the most reliable
// prediction) that occurs in |processed_fields| and has interactability level
// at least |username_max|.
const autofill::FormFieldData* FindUsernameInPredictions(
    const std::vector<autofill::FieldRendererId>& username_predictions,
    const std::vector<ProcessedField>& processed_fields,
    Interactability username_max);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_DATA_PARSER_H_
