// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::autofill_metrics {

namespace {

AutofillFieldByFieldFillingTypes GetFieldByFieldFillingType(
    ServerFieldType field_type) {
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
    default:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

void LogEditAddressProfileDialogClosed(bool user_saved_changes) {
  base::UmaHistogramBoolean("Autofill.ExtendedMenu.EditAddress",
                            user_saved_changes);
}

void LogDeleteAddressProfileDialogClosed(bool user_accepted_delete) {
  base::UmaHistogramBoolean("Autofill.ExtendedMenu.DeleteAddress",
                            user_accepted_delete);
}

void LogFillingMethodUsed(AutofillFillingMethodMetric filling_method) {
  CHECK_LE(filling_method, AutofillFillingMethodMetric::kMaxValue);
  base::UmaHistogramEnumeration("Autofill.FillingMethodUsed", filling_method);
}

void LogFieldByFieldFillingFieldUsed(ServerFieldType field_type) {
  base::UmaHistogramEnumeration("Autofill.FieldByFieldFilling.FieldTypeUsed",
                                GetFieldByFieldFillingType(field_type),
                                AutofillFieldByFieldFillingTypes::kMaxValue);
}

}  // namespace autofill::autofill_metrics
