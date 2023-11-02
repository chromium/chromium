// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_KEYBOARD_ACCESSORY_METRICS_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_KEYBOARD_ACCESSORY_METRICS_LOGGER_H_


namespace autofill {

class KeyboardAccessoryMetricsLogger {
 public:
  // Each of these metrics is logged only for potentially autofillable forms,
  // i.e. forms with at least three fields, etc.
  // These are used to derive how often the keyboard accessory buttons are used.
  // For example, (NEXT_BUTTON_PRESSED_ONCE / SUBMITTED_FORM) gives the fraction
  // of submitted forms where the user pressed the "next field" keyboard
  // accessory button to navigate the form.
  enum ButtonMetric {
    // Loaded a page containing forms.
    // Should be logged under exact same conditions as
    // UserHappinessMetric.FORMS_LOADED to make this data as directly
    // comparable as possible with UserHappinessMetric.
    FORMS_LOADED = 0,
    // Submitted a form.
    // Should be logged under same conditions as UserHappiness.SUBMITTED_*, for
    // same reason as FORMS_LOADED above.
    SUBMITTED_FORM,
    // User pressed the "Close" button on the keyboard accessory.
    CLOSE_BUTTON_PRESSED,
    // Same as above, but only logged once per page load.
    CLOSE_BUTTON_PRESSED_ONCE,
    // User pressed the "Next" button on the keyboard accessory.
    NEXT_BUTTON_PRESSED,
    // Same as above, but only logged once per page load.
    NEXT_BUTTON_PRESSED_ONCE,
    // User pressed the "Previous" button on the keyboard accessory.
    PREVIOUS_BUTTON_PRESSED,
    // Same as above, but only logged once per page load.
    PREVIOUS_BUTTON_PRESSED_ONCE,
    NUM_BUTTON_METRICS,
  };

  KeyboardAccessoryMetricsLogger();

  // Called when a page with potentially autofillable forms is loaded,
  // i.e. forms with at least three fields, etc.
  static void OnFormsLoaded();

  // Called when a potentially autofillable form is submitted.
  static void OnFormSubmitted();

  // Called when the user presses the "close keyboard" keyboard accessory
  // button.
  void OnCloseButtonPressed();

  // Called when the user presses the "next field" keyboard accessory button.
  void OnNextButtonPressed();

  // Called when the user presses the "previous field" keyboard accessory
  // button.
  void OnPreviousButtonPressed();

 private:
  bool has_logged_close_button_;
  bool has_logged_next_button_;
  bool has_logged_previous_button_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_KEYBOARD_ACCESSORY_METRICS_LOGGER_H_
