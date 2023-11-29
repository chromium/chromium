// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_

#include "components/autofill/core/browser/field_types.h"

namespace autofill::autofill_metrics {

// Represents the filling method chosen by the user.
enum class AutofillFillingMethodMetric {
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

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused. This is a subset of field
// types that can be chosen when the user fills a specific field using one of
// the field by field filling suggestions. Update this enum when a new
// `ServerFieldType` is included in the available field by field filling field
// type suggestions.
enum class AutofillFieldByFieldFillingTypes {
  kNameFirst = 0,
  kNameMiddle = 1,
  kNameLast = 2,
  kAddressHomeLine1 = 3,
  kAddressHomeLine2 = 4,
  kAddressHomeZip = 5,
  kPhoneHomeWholeNumber = 6,
  kEmailAddress = 7,
  kAddressHomeHouseNumber = 8,
  kAddressHomeStreetName = 9,
  kCreditCardNameFull = 10,
  kCreditCardNumber = 11,
  kCreditCardExpiryDate = 12,
  kCreditCardExpiryYear = 13,
  kCreditCardExpiryMonth = 14,
  kMaxValue = kCreditCardExpiryMonth
};

// This metric is only relevant for granular filling, i.e. when the edit dialog
// is opened from the Autofill popup.
void LogEditAddressProfileDialogClosed(bool user_saved_changes);

// This metric is only relevant for granular filling, i.e. when the delete
// dialog is opened from the Autofill popup.
void LogDeleteAddressProfileDialogClosed(bool user_accepted_delete);

// Logs the `AutofillFillingMethodMetric` chosen by the user.
void LogFillingMethodUsed(AutofillFillingMethodMetric filling_method);

// Logs the `AutofillFieldByFieldFillingTypes` that corresponds
// to the `field_type` chosen by the user when accepting a field-by-field
// filling suggestions.
void LogFieldByFieldFillingFieldUsed(ServerFieldType field_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
