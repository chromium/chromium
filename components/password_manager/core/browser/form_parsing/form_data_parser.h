// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_DATA_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_DATA_PARSER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill {
struct FormData;
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

// A wrapper around FormFieldData, carrying some additional data used during
// parsing.
struct ProcessedField {
  // This points to the wrapped FormFieldData.
  raw_ptr<const autofill::FormFieldData> field;

  // The flag derived from field->autocomplete_attribute.
  AutocompleteFlag autocomplete_flag = AutocompleteFlag::kNone;

  // True if field->form_control_type == "password".
  bool is_password = false;

  // True if field is predicted to be a password.
  bool is_predicted_as_password = false;

  // True if the server predicts that this field is a credit card field (e.g.
  // CVC field).
  bool server_hints_credit_card_field = false;

  // True if the server predicts that this field is not a password field (credit
  // cards fields don't set this field).
  bool server_hints_not_password = false;

  // True if the server predicts that this field is not a username field.
  bool server_hints_not_username = false;

  // True if the field accepts WebAuthn credentials, false otherwise.
  bool accepts_webauthn_credentials = false;

  Interactability interactability = Interactability::kUnlikely;
};

// This class takes care of parsing FormData into PasswordForm and managing
// related metadata.
class FormDataParser {
 public:
  // Denotes the intended use of the result (for filling forms vs. saving
  // captured credentials). This influences whether empty fields are ignored.
  enum class Mode { kFilling, kSaving };

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

  const absl::optional<FormPredictions>& predictions() { return predictions_; }

  ReadonlyPasswordFields readonly_status() { return readonly_status_; }

  // Parse DOM information |form_data| into Password Manager's form
  // representation PasswordForm. Return nullptr when parsing is unsuccessful.
  std::unique_ptr<PasswordForm> Parse(const autofill::FormData& form_data,
                                      Mode mode);

 private:
  // Predictions are an optional source of server-side information about field
  // types.
  absl::optional<FormPredictions> predictions_;

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
