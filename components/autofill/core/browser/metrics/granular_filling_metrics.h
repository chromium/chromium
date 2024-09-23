// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_

#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"

namespace autofill::autofill_metrics {

// These values are persisted to UMA logs. Entries should not be renumbered
// and numeric values should never be reused. This is a subset of field
// types that can be chosen when the user fills a specific field using one of
// the field by field filling suggestions. Update this enum when a new
// `FieldType` is included in the available field by field filling field
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
  kCity = 15,
  kCompany = 16,
  kMaxValue = kCompany
};

// This metric is only relevant for granular filling, i.e. when the edit dialog
// is opened from the Autofill popup.
void LogEditAddressProfileDialogClosed(bool user_saved_changes);

// This metric is only relevant for granular filling, i.e. when the delete
// dialog is opened from the Autofill popup.
void LogDeleteAddressProfileFromExtendedMenu(bool user_accepted_delete);

// Logs the `FillingMethod` chosen by the user.
// `filling_product` defines what type of filling the user chose, for example
// address or payment. `triggering_field_type_matches_filling_product` defines
// whether the `filling_product` chosen matches the triggering field type. For
// example, if an user chose to fill their address profile into an unclassified
// field, triggering_field_type_matches_filling_product will be false.
void LogFillingMethodUsed(FillingMethod filling_method,
                          FillingProduct filling_product,
                          bool triggering_field_type_matches_filling_product);

// Logs the `AutofillFieldByFieldFillingTypes` that corresponds
// to the `field_type` chosen by the user when accepting a field-by-field
// filling suggestions.
// `filling_product` defines what type of filling the user chose, for example
// address or payment. `triggering_field_type_matches_filling_product` defines
// whether the `filling_product` chosen matches the triggering field type. For
// example, if an user chose to fill their address profile into an unclassified
// field, triggering_field_type_matches_filling_product will be false.
void LogFieldByFieldFillingFieldUsed(
    FieldType field_type_used,
    FillingProduct filling_product,
    bool triggering_field_type_matches_filling_product);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_GRANULAR_FILLING_METRICS_H_
