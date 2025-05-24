// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"

namespace autofill::autofill_metrics {

namespace {
// Returns the histogram suffix for the passed
// `SaveCreditCardPromptOverlayType`.
std::string_view SaveCreditCardPromptOverlayTypeToMetricSuffix(
    SaveCreditCardPromptOverlayType type) {
  switch (type) {
    case SaveCreditCardPromptOverlayType::kBanner:
      return ".Banner";
    case SaveCreditCardPromptOverlayType::kBottomSheet:
      return ".BottomSheet";
    case SaveCreditCardPromptOverlayType::kModal:
      return ".Modal";
  }
  NOTREACHED();
}

// Returns the histogram suffix stating if fix flow
// is required or not. If required then returns which fix flow is requested.
std::string_view SaveCreditCardPromptFixFlowSuffix(
    bool request_cardholder_name,
    bool request_expiration_date) {
  if (request_cardholder_name && request_expiration_date) {
    return ".RequestingCardHolderNameAndExpiryDate";
  } else if (request_cardholder_name) {
    return ".RequestingCardHolderName";
  } else if (request_expiration_date) {
    return ".RequestingExpiryDate";
  }
  return ".NoFixFlow";
}

}  // namespace

void LogSaveCreditCardPromptResultIOS(
    SaveCreditCardPromptResultIOS metric,
    bool is_uploading,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions& options,
    SaveCreditCardPromptOverlayType overlay_type) {
  auto is_num_strikes_in_range = [](int strikes) {
    return strikes >= 0 && strikes <= 2;
  };

  // To avoid emitting an arbitrary number of histograms, limit `num_strikes` to
  // [0, 2], matching the save card's current maximum allowed strikes.
  if (!options.num_strikes.has_value() ||
      !is_num_strikes_in_range(options.num_strikes.value())) {
    return;
  }

  std::string_view destination = is_uploading ? ".Server" : ".Local";

  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveCreditCardPromptResult.IOS", destination,
                    SaveCreditCardPromptOverlayTypeToMetricSuffix(overlay_type),
                    ".NumStrikes.",
                    base::NumberToString(options.num_strikes.value()),
                    SaveCreditCardPromptFixFlowSuffix(
                        options.should_request_name_from_user,
                        options.should_request_expiration_date_from_user)}),
      metric);
}

}  // namespace autofill::autofill_metrics
