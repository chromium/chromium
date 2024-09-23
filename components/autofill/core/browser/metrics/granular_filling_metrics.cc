// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::autofill_metrics {

namespace {

AutofillFieldByFieldFillingTypes GetFieldByFieldFillingType(
    FieldType field_type) {
  switch (field_type) {
    case NAME_FIRST:
      return AutofillFieldByFieldFillingTypes::kNameFirst;
    case NAME_MIDDLE:
      return AutofillFieldByFieldFillingTypes::kNameMiddle;
    case NAME_LAST:
      return AutofillFieldByFieldFillingTypes::kNameLast;
    case ADDRESS_HOME_LINE1:
      return AutofillFieldByFieldFillingTypes::kAddressHomeLine1;
    case ADDRESS_HOME_LINE2:
      return AutofillFieldByFieldFillingTypes::kAddressHomeLine2;
    case ADDRESS_HOME_ZIP:
      return AutofillFieldByFieldFillingTypes::kAddressHomeZip;
    case PHONE_HOME_WHOLE_NUMBER:
      return AutofillFieldByFieldFillingTypes::kPhoneHomeWholeNumber;
    case EMAIL_ADDRESS:
      return AutofillFieldByFieldFillingTypes::kEmailAddress;
    case ADDRESS_HOME_HOUSE_NUMBER:
      return AutofillFieldByFieldFillingTypes::kAddressHomeHouseNumber;
    case ADDRESS_HOME_STREET_NAME:
      return AutofillFieldByFieldFillingTypes::kAddressHomeStreetName;
    case CREDIT_CARD_NAME_FULL:
      return AutofillFieldByFieldFillingTypes::kCreditCardNameFull;
    case CREDIT_CARD_NUMBER:
      return AutofillFieldByFieldFillingTypes::kCreditCardNumber;
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return AutofillFieldByFieldFillingTypes::kCreditCardExpiryDate;
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return AutofillFieldByFieldFillingTypes::kCreditCardExpiryYear;
    case CREDIT_CARD_EXP_MONTH:
      return AutofillFieldByFieldFillingTypes::kCreditCardExpiryMonth;
    case ADDRESS_HOME_CITY:
      return AutofillFieldByFieldFillingTypes::kCity;
    case COMPANY_NAME:
      return AutofillFieldByFieldFillingTypes::kCompany;
    default:
      NOTREACHED();
  }
}

}  // namespace

void LogEditAddressProfileDialogClosed(bool user_saved_changes) {
  base::UmaHistogramBoolean("Autofill.ExtendedMenu.EditAddress",
                            user_saved_changes);
}

void LogDeleteAddressProfileFromExtendedMenu(bool user_accepted_delete) {
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.ExtendedMenu",
                            user_accepted_delete);
  base::UmaHistogramBoolean("Autofill.ProfileDeleted.Any",
                            user_accepted_delete);
}

void LogFillingMethodUsed(FillingMethod filling_method,
                          FillingProduct filling_product,
                          bool triggering_field_type_matches_filling_product) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.FillingMethodUsed.",
                    FillingProductToString(filling_product),
                    triggering_field_type_matches_filling_product
                        ? ".TriggeringFieldMatchesFillingProduct"
                        : ".TriggeringFieldDoesNotMatchFillingProduct"}),
      filling_method);
}

void LogFieldByFieldFillingFieldUsed(
    FieldType field_type_used,
    FillingProduct filling_product,
    bool triggering_field_type_matches_filling_product) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.FieldByFieldFilling.FieldTypeUsed.",
                    FillingProductToString(filling_product),
                    triggering_field_type_matches_filling_product
                        ? ".TriggeringFieldMatchesFillingProduct"
                        : ".TriggeringFieldDoesNotMatchFillingProduct"}),
      GetFieldByFieldFillingType(field_type_used));
}

}  // namespace autofill::autofill_metrics
