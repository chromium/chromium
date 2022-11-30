// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_controller_metrics_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill {

namespace {
static const char kPrefix[] = "Autofill.CreditCardMessage";
}

void LogAutofillCreditCardMessageMetrics(
    MessageMetrics metric,
    bool is_uploading,
    AutofillClient::SaveCreditCardOptions options) {
  std::string destination = is_uploading ? ".Server" : ".Local";
  base::UmaHistogramEnumeration(base::StrCat({kPrefix, destination}), metric);
  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration(
        base::StrCat({kPrefix, destination, ".RequestingCardholderName"}),
        metric);
  }

  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration(
        base::StrCat({kPrefix, destination, ".RequestingExpirationDate"}),
        metric);
  }

  if (options.from_dynamic_change_form) {
    base::UmaHistogramEnumeration(
        base::StrCat({kPrefix, destination, ".FromDynamicChangeForm"}), metric);
  }

  if (options.has_non_focusable_field) {
    base::UmaHistogramEnumeration(
        base::StrCat({kPrefix, destination, ".FromNonFocusableForm"}), metric);
  }

  if (options.has_multiple_legal_lines) {
    base::UmaHistogramEnumeration(
        base::StrCat({kPrefix, destination, ".WithMultipleLegalLines"}),
        metric);
  }
}

void LogAutofillCreditCardMessageDialogPromptMetrics(
    MessageDialogPromptMetrics metric,
    AutofillClient::SaveCreditCardOptions options,
    bool is_link_clicked) {
  std::string histogram = base::StrCat({kPrefix, ".DialogPrompt"});
  if (options.should_request_expiration_date_from_user) {
    histogram = base::StrCat({histogram, ".RequestingExpirationDate"});
  } else if (options.should_request_name_from_user) {
    histogram = base::StrCat({histogram, ".RequestingCardholderName"});
  } else {
    histogram = base::StrCat({histogram, ".ConfirmInfo"});
  }

  base::UmaHistogramEnumeration(histogram, metric);
  if (is_link_clicked) {
    base::UmaHistogramEnumeration(base::StrCat({histogram, ".DidClickLinks"}),
                                  metric);
  }
}

}  // namespace autofill
