// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/core/browser/keyboard_accessory_metrics_logger.h"

#import <UIKit/UIKit.h>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"

namespace autofill {

namespace {

void Log(KeyboardAccessoryMetricsLogger::ButtonMetric metric) {
  DCHECK_LT(metric, KeyboardAccessoryMetricsLogger::NUM_BUTTON_METRICS);
  if (UIAccessibilityIsVoiceOverRunning()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Autofill.KeyboardAccessoryButtonsIOS_ScreenReaderOn", metric,
        KeyboardAccessoryMetricsLogger::NUM_BUTTON_METRICS);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Autofill.KeyboardAccessoryButtonsIOS_ScreenReaderOff", metric,
        KeyboardAccessoryMetricsLogger::NUM_BUTTON_METRICS);
  }
}

}  // namespace

KeyboardAccessoryMetricsLogger::KeyboardAccessoryMetricsLogger()
    : has_logged_close_button_(false),
      has_logged_next_button_(false),
      has_logged_previous_button_(false) {}

// static
void KeyboardAccessoryMetricsLogger::OnFormsLoaded() {
  Log(FORMS_LOADED);
}

// static
void KeyboardAccessoryMetricsLogger::OnFormSubmitted() {
  Log(SUBMITTED_FORM);
}

void KeyboardAccessoryMetricsLogger::OnCloseButtonPressed() {
  Log(CLOSE_BUTTON_PRESSED);
  if (!has_logged_close_button_) {
    has_logged_close_button_ = true;
    Log(CLOSE_BUTTON_PRESSED_ONCE);
  }
}

void KeyboardAccessoryMetricsLogger::OnNextButtonPressed() {
  Log(NEXT_BUTTON_PRESSED);
  if (!has_logged_next_button_) {
    has_logged_next_button_ = true;
    Log(NEXT_BUTTON_PRESSED_ONCE);
  }
}

void KeyboardAccessoryMetricsLogger::OnPreviousButtonPressed() {
  Log(PREVIOUS_BUTTON_PRESSED);
  if (!has_logged_previous_button_) {
    has_logged_previous_button_ = true;
    Log(PREVIOUS_BUTTON_PRESSED_ONCE);
  }
}

}  // namespace autofill
