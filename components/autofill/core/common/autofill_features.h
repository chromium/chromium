// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

class PrefService;

namespace base {
struct Feature;
}

namespace autofill {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillAllowNonHttpActivation;
extern const base::Feature kAutofillAddressNormalizer;
extern const base::Feature kAutofillAlwaysFillAddresses;
extern const base::Feature kAutofillCacheQueryResponses;
extern const base::Feature kAutofillCreateDataForTest;
extern const base::Feature kAutofillCreditCardAblationExperiment;
extern const base::Feature kAutofillCreditCardAssist;
extern const base::Feature kAutofillCreditCardLocalCardMigration;
extern const base::Feature kAutofillDeleteDisusedAddresses;
extern const base::Feature kAutofillDeleteDisusedCreditCards;
extern const base::Feature kAutofillDownstreamUseGooglePayBrandingOniOS;
extern const base::Feature kAutofillDynamicForms;
extern const base::Feature kAutofillEnableAccountWalletStorage;
extern const base::Feature kAutofillEnableCompanyName;
extern const base::Feature kAutofillEnableIFrameSupportOniOS;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForQuery;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForUpload;
extern const base::Feature kAutofillExpandedPopupViews;
extern const base::Feature kAutofillGetPaymentsIdentityFromSync;
extern const base::Feature kAutofillLocalCardMigrationShowFeedback;
extern const base::Feature kAutofillManualFallback;
extern const base::Feature kAutofillManualFallbackPhaseTwo;
extern const base::Feature kAutofillPreferServerNamePredictions;
extern const base::Feature kAutofillNoLocalSaveOnUploadSuccess;
extern const base::Feature kAutofillOverrideWithRaterConsensus;
extern const base::Feature kAutofillPrefilledFields;
extern const base::Feature kAutofillRationalizeFieldTypePredictions;
extern const base::Feature kAutofillRationalizeRepeatedServerPredictions;
extern const base::Feature kAutofillRestrictUnownedFieldsToFormlessCheckout;
extern const base::Feature kAutofillSaveCardDialogUnlabeledExpirationDate;
extern const base::Feature kAutofillSaveCardSignInAfterLocalSave;
extern const base::Feature kAutofillSaveCreditCardUsesStrikeSystem;
extern const base::Feature kAutofillSaveOnProbablySubmitted;
extern const base::Feature kAutofillScanCardholderName;
extern const base::Feature kAutofillSendExperimentIdsInPaymentsRPCs;
extern const base::Feature kAutofillSendOnlyCountryInGetUploadDetails;
extern const base::Feature kAutofillServerCommunication;
extern const base::Feature kAutofillShowAllSuggestionsOnPrefilledForms;
extern const base::Feature kAutofillShowAutocompleteConsoleWarnings;
extern const base::Feature kAutofillShowTypePredictions;
extern const base::Feature kAutofillSkipComparingInferredLabels;
extern const base::Feature kAutofillSuggestInvalidProfileData;
extern const base::Feature kAutofillSuppressDisusedAddresses;
extern const base::Feature kAutofillSuppressDisusedCreditCards;
extern const base::Feature kAutofillUploadThrottling;
extern const base::Feature kAutofillUpstream;
extern const base::Feature kAutofillUpstreamAllowAllEmailDomains;
extern const base::Feature kAutofillUpstreamAlwaysRequestCardholderName;
extern const base::Feature kAutofillUpstreamBlankCardholderNameField;
extern const base::Feature kAutofillUpstreamDisallowElo;
extern const base::Feature kAutofillUpstreamDisallowJcb;
extern const base::Feature kAutofillUpstreamEditableCardholderName;
extern const base::Feature kAutofillUpstreamUseGooglePayBrandingOnMobile;
extern const base::Feature kAutofillUsePaymentsCustomerData;
extern const base::Feature kAutomaticPasswordGeneration;
extern const base::Feature kSingleClickAutofill;
extern const char kAutofillCreditCardLastUsedDateShowExpirationDateKey[];
extern const char kAutofillLocalCardMigrationCloseButtonDelay[];
extern const char kAutofillCreditCardLocalCardMigrationParameterName[];
extern const char kAutofillUpstreamMaxMinutesSinceAutofillProfileUseKey[];
extern const char kCreditCardSigninPromoImpressionLimitParamKey[];
extern const char
    kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage[];

#if defined(OS_ANDROID)
extern const base::Feature kAutofillManualFallbackAndroid;
extern const base::Feature kAutofillRefreshStyleAndroid;
#endif  // OS_ANDROID

// Returns whether the Autofill credit card assist infobar should be shown.
bool IsAutofillCreditCardAssistEnabled();

// Enum for local card migration experimental flag states.
enum class LocalCardMigrationExperimentalFlag {
  // Local card migration disabled.
  kMigrationDisabled,
  // Only migrate local cards when user submits form.
  kMigrationWithoutSettingsPage,
  // Migrate both on submitted form and from settings page.
  kMigrationIncludeSettingsPage,
};

// Returns kMigrationDisabled if no experimental behavior is enabled for
// kAutofillCreditCardLocalCardMigration; Return kMigrationIncludeSettingsPage
// if user enables the local card migration and does not exclude the settings
// page. Return kMigrationWithoutSettingsPage if user chooses to exclude the
// settings page migration.
LocalCardMigrationExperimentalFlag GetLocalCardMigrationExperimentalFlag();

// For testing purposes; not to be launched.  When enabled, Chrome Upstream
// always requests that the user enters/confirms cardholder name in the
// offer-to-save dialog, regardless of if it was present or if the user is a
// Google Payments customer.  Note that this will override the detected
// cardholder name, if one was found.
bool IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled();

// For experimental purposes; not to be made available in chrome://flags. When
// enabled and Chrome Upstream requests the cardholder name in the offer-to-save
// dialog, the field will be blank instead of being prefilled with the name from
// the user's Google Account.
bool IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled();

// Returns whether the experiment is enabled where Chrome Upstream can request
// the user to enter/confirm cardholder name in the offer-to-save bubble if it
// was not detected or was conflicting during the checkout flow and the user is
// NOT a Google Payments customer.
bool IsAutofillUpstreamEditableCardholderNameExperimentEnabled();

#if defined(OS_MACOSX)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // defined(OS_MACOSX)

// Returns whether the UI for passwords in manual fallback is enabled.
bool IsPasswordManualFallbackEnabled();

// Returns whether the UI for addresses and credit cards in manual fallback is
// enabled.
bool IsAutofillManualFallbackEnabled();

// Returns true if the native Views implementation of the Desktop dropdown
// should be used. This will also be true if the kExperimentalUi flag is true,
// which forces a bunch of forthcoming UI changes on.
bool ShouldUseNativeViews();

// Returns true if expiration dates on the save card dialog should be
// unlabeled, i.e. not preceded by "Exp."
bool IsAutofillSaveCardDialogUnlabeledExpirationDateEnabled();

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
