// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_PARSER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"
#include "url/gurl.h"

namespace autofill {
struct FormData;
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

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
    kCount
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

  ~FormDataParser();

  void set_predictions(FormPredictions predictions) {
    predictions_ = std::move(predictions);
  }

  const base::Optional<FormPredictions>& predictions() { return predictions_; }

  ReadonlyPasswordFields readonly_status() { return readonly_status_; }

  // Parse DOM information |form_data| into Password Manager's form
  // representation PasswordForm. Return nullptr when parsing is unsuccessful.
  std::unique_ptr<autofill::PasswordForm> Parse(
      const autofill::FormData& form_data,
      Mode mode);

 private:
  // Predictions are an optional source of server-side information about field
  // types.
  base::Optional<FormPredictions> predictions_;

  // Records whether readonly password fields were seen during the last call to
  // Parse().
  ReadonlyPasswordFields readonly_status_ =
      ReadonlyPasswordFields::kNoHeuristics;

  DISALLOW_COPY_AND_ASSIGN(FormDataParser);
};

// Returns the value of PasswordForm::signon_realm for an HTML form with the
// origin |url|.
std::string GetSignonRealm(const GURL& url);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_IOS_FORM_PARSER_H_
