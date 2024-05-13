// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_FORM_EVENT_LOGGER_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_FORM_EVENT_LOGGER_H_

#include <string>

namespace autofill {

// Logs autofill funnel and key metrics for weblayer.
class AndroidFormEventLogger {
 public:
  explicit AndroidFormEventLogger(const std::string& form_type_name);
  virtual ~AndroidFormEventLogger();

  void OnDidParseForm();
  void OnDidInteractWithAutofillableForm();
  void OnDidFillSuggestion();
  void OnWillSubmitForm();
  void OnTypedIntoNonFilledField();
  void OnEditedAutofilledField();

 private:
  void RecordFunnelAndKeyMetrics();

  std::string form_type_name_;

  bool has_parsed_form_{false};
  bool has_logged_interacted_{false};
  bool has_logged_suggestion_filled_{false};
  bool has_logged_will_submit_{false};
  bool has_logged_typed_into_non_filled_field_{false};
  bool has_logged_edited_autofilled_field_{false};
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_FORM_EVENT_LOGGER_H_
