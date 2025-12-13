// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"

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

// Returns the histogram suffix stating if the save destination is `Upload` or
// `Local`.
std::string SaveCvcPromptSaveDestinationSuffix(std::string_view base_name,
                                               bool is_uploading) {
  return base::StrCat({base_name, is_uploading ? ".Upload" : ".Local"});
}

// Returns the histogram suffix for the given card save type.
std::string_view GetSuffixForSaveType(
    const payments::PaymentsAutofillClient::SaveCreditCardOptions& options) {
  switch (options.card_save_type) {
    case payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc:
      return ".SavingWithCvc";
    case payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly:
      // The kCardSaveOnly metric does not have a suffix, to preserve
      // continuity with data logged prior to the introduction of CVC saving
      // features.
      return "";
    case payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly:
      // This flow is now logged via LogSaveCvcPromptOfferedIOS.
      NOTREACHED();
  }
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

  // Determine the metric suffix based on the card save type.
  const std::string_view save_type_suffix = GetSuffixForSaveType(options);

  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveCreditCardPromptResult.IOS", destination,
                    SaveCreditCardPromptOverlayTypeToMetricSuffix(overlay_type),
                    ".NumStrikes.",
                    base::NumberToString(options.num_strikes.value()),
                    SaveCreditCardPromptFixFlowSuffix(
                        options.should_request_name_from_user,
                        options.should_request_expiration_date_from_user),
                    save_type_suffix}),
      metric);
}

void LogSaveCvcPromptOfferedIOS(bool is_uploading) {
  base::UmaHistogramEnumeration(
      SaveCvcPromptSaveDestinationSuffix("Autofill.SaveCvcPromptOffer.IOS",
                                         is_uploading),
      SaveCardPromptOffer::kShown);
}

void LogSaveCvcPromptResultIOS(
    SaveCvcPromptResultIOS metric,
    bool is_uploading,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions& options) {
  base::UmaHistogramEnumeration(
      SaveCvcPromptSaveDestinationSuffix("Autofill.SaveCvcPromptResult.IOS",
                                         is_uploading),
      metric);
}

void LogSaveCreditCardPromptOfferMetricIos(
    SaveCardPromptOffer metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options,
    SaveCreditCardPromptOverlayType overlay_type) {
  std::string_view destination = is_upload_save ? ".Server" : ".Local";
  std::string base_histogram_name = base::StrCat(
      {"Autofill.SaveCreditCardPromptOffer.IOS", destination,
       SaveCreditCardPromptOverlayTypeToMetricSuffix(overlay_type)});

  // Determine the metric suffix based on the card save type.
  const std::string_view save_type_suffix = [&]() {
    switch (save_credit_card_options.card_save_type) {
      case payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc:
        return ".SavingWithCvc";
      case payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly:
        // The kCardSaveOnly metric does not have a suffix, to preserve
        // continuity with data logged prior to the introduction of CVC saving
        // features.
        return "";
      case payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly:
        // This flow is now logged via LogSaveCvcPromptOfferedIOS.
        break;
    }
    NOTREACHED();
  }();

  base::UmaHistogramEnumeration(
      base::StrCat({base_histogram_name, save_type_suffix}), metric);

  auto is_num_strikes_in_range = [](int strikes) {
    return strikes >= 0 && strikes <= 2;
  };

  // To avoid emitting an arbitrary number of histograms, limit `num_strikes` to
  // [0, 2], matching the save card's current maximum allowed strikes.
  if (!save_credit_card_options.num_strikes ||
      !is_num_strikes_in_range(*(save_credit_card_options.num_strikes))) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat(
          {base_histogram_name, ".NumStrikes.",
           base::NumberToString(save_credit_card_options.num_strikes.value()),
           SaveCreditCardPromptFixFlowSuffix(
               save_credit_card_options.should_request_name_from_user,
               save_credit_card_options
                   .should_request_expiration_date_from_user),
           save_type_suffix}),
      metric);
}

}  // namespace autofill::autofill_metrics
