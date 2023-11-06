// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_

namespace autofill::autofill_metrics {

// Represents the filling method chosen by the user.
enum AutofillFillingMethodMetric {
  // User chose to fill the whole form. Either from the main suggestion or from
  // the extended menu `PopupItemId::kFillEverything`.
  kFullForm = 0,
  // User chose to fill all name fields.
  kGroupFillingName = 1,
  // User chose to fill all address fields.
  kGroupFillingAddress = 2,
  // User chose to fill all email fields.
  kGroupFillingEmail = 3,
  // User chose to fill all phone number fields.
  kGroupFillingPhoneNumber = 4,
  // User chose to fill a specific field.
  kFieldByFieldFilling = 5,
  kMaxValue = kFieldByFieldFilling
};

// This metric is only relevant for granular filling, i.e. when the edit dialog
// is opened from the Autofill popup.
void LogEditAddressProfileDialogClosed(bool user_saved_changes);

// This metric is only relevant for granular filling, i.e. when the delete
// dialog is opened from the Autofill popup.
void LogDeleteAddressProfileDialogClosed(bool user_accepted_delete);

// Logs the `AutofillFillingMethodMetric` chosen by the user.
void LogFillingMethodUsed(AutofillFillingMethodMetric filling_method);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
