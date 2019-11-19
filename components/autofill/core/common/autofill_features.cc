// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace features {

// Controls whether the AddressNormalizer is supplied. If available, it may be
// used to normalize address and will incur fetching rules from the server.
const base::Feature kAutofillAddressNormalizer{
    "AutofillAddressNormalizer", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether autofill activates on non-HTTP(S) pages. Useful for
// automated with data URLS in cases where it's too difficult to use the
// embedded test server. Generally avoid using.
const base::Feature kAutofillAllowNonHttpActivation{
    "AutofillAllowNonHttpActivation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillAlwaysFillAddresses{
    "AlwaysFillAddresses", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls the use of GET (instead of POST) to fetch cacheable autofill query
// responses.
const base::Feature kAutofillCacheQueryResponses{
    "AutofillCacheQueryResponses", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillCreateDataForTest{
    "AutofillCreateDataForTest", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardAssist{
    "AutofillCreditCardAssist", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we download server credit cards to the ephemeral
// account-based storage when sync the transport is enabled.
const base::Feature kAutofillEnableAccountWalletStorage{
    "AutofillEnableAccountWalletStorage", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we use COMPANY as part of Autofill
const base::Feature kAutofillEnableCompanyName{
    "AutofillEnableCompanyName", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// heuristic field type prediction is run for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics{
    "AutofillEnforceMinRequiredFieldsForHeuristics",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// crowd-sourced field type predictions are queried for a form.
const base::Feature kAutofillEnforceMinRequiredFieldsForQuery{
    "AutofillEnforceMinRequiredFieldsForQuery",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a minimum number of fields is required before
// field type votes are uploaded to the crowd-sourcing server.
const base::Feature kAutofillEnforceMinRequiredFieldsForUpload{
    "AutofillEnforceMinRequiredFieldsForUpload",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, gets payment identity from sync service instead of
// identity manager.
const base::Feature kAutofillGetPaymentsIdentityFromSync{
    "AutofillGetPaymentsIdentityFromSync", base::FEATURE_ENABLED_BY_DEFAULT};

// When enabled, autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
const base::Feature kAutofillKeyboardAccessory{
    "AutofillKeyboardAccessory", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillPruneSuggestions{
    "AutofillPruneSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillMetadataUploads{"AutofillMetadataUploads",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillOffNoServerData{"AutofillOffNoServerData",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, autofill server will override field types with rater
// consensus data before returning to client.
const base::Feature kAutofillOverrideWithRaterConsensus{
    "AutofillOverrideWithRaterConsensus", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillPreferServerNamePredictions{
    "AutofillPreferServerNamePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillProfileClientValidation{
    "AutofillProfileClientValidation", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill uses server-side validation to ensure that fields
// with invalid data are not suggested.
const base::Feature kAutofillProfileServerValidation{
    "AutofillProfileServerValidation", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether autofill rejects using non-verified company names that are
// in the format of a birthyear.
const base::Feature kAutofillRejectCompanyBirthyear{
    "AutofillRejectCompanyBirthyear", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether autofill rejects using non-verified company names that are
// social titles (e.g., "Mrs.") in some languages.
const base::Feature kAutofillRejectCompanySocialTitle{
    "AutofillRejectCompanySocialTitle", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether or not a group of fields not enclosed in a form can be
// considered a form. If this is enabled, unowned fields will only constitute
// a form if there are signals to suggest that this might a checkout page.
const base::Feature kAutofillRestrictUnownedFieldsToFormlessCheckout{
    "AutofillRestrictUnownedFieldsToFormlessCheckout",
    base::FEATURE_DISABLED_BY_DEFAULT};

// On Canary and Dev channels only, this feature flag instructs chrome to send
// rich form/field metadata with queries. This will trigger the use of richer
// field-type predictions model on the server, for testing/evaluation of those
// models prior to a client-push.
const base::Feature kAutofillRichMetadataQueries{
    "AutofillRichMetadataQueries", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether UPI/VPA values will be saved and filled into payment forms.
const base::Feature kAutofillSaveAndFillVPA{"AutofillSaveAndFillVPA",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillSaveOnProbablySubmitted{
    "AutofillSaveOnProbablySubmitted", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
const base::Feature kAutofillServerCommunication{
    "AutofillServerCommunication", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether autofill suggestions are filtered by field values previously
// filled by website.
const base::Feature kAutofillShowAllSuggestionsOnPrefilledForms{
    "AutofillShowAllSuggestionsOnPrefilledForms",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether we show warnings in the Dev console for misused autocomplete
// types.
const base::Feature kAutofillShowAutocompleteConsoleWarnings{
    "AutofillShowAutocompleteConsoleWarnings",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
const base::Feature kAutofillShowTypePredictions{
    "AutofillShowTypePredictions", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether inferred label is considered for comparing in
// FormFieldData.SimilarFieldAs.
const base::Feature kAutofillSkipComparingInferredLabels{
    "AutofillSkipComparingInferredLabels", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Autofill should search prefixes of all words/tokens when
// filtering profiles, or only on prefixes of the whole string.
const base::Feature kAutofillTokenPrefixMatching{
    "AutofillTokenPrefixMatching", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the touch to fill feature for Android.
const base::Feature kAutofillTouchToFill = {"TouchToFillAndroid",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUploadThrottling{"AutofillUploadThrottling",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to use the API or use the legacy server.
const base::Feature kAutofillUseApi{"AutofillUseApi",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether suggestions' labels use the improved label disambiguation
// format.
const base::Feature kAutofillUseImprovedLabelDisambiguation{
    "AutofillUseImprovedLabelDisambiguation",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Server predictions for CVC fields are used if the feature is enabled.
const base::Feature kAutofillUseServerCVCPrediction{
    "AutofillUseServerCVCPrediction", base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Controls whether the Autofill manual fallback for Addresses and Payments is
// present on Android.
const base::Feature kAutofillManualFallbackAndroid{
    "AutofillManualFallbackAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to use modernized style for the Autofill dropdown.
const base::Feature kAutofillRefreshStyleAndroid{
    "AutofillRefreshStyleAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

#endif  // OS_ANDROID

#if defined(OS_ANDROID) || defined(OS_IOS)
const base::Feature kAutofillUseMobileLabelDisambiguation{
    "AutofillUseMobileLabelDisambiguation", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAutofillUseMobileLabelDisambiguationParameterName[] = "variant";
const char kAutofillUseMobileLabelDisambiguationParameterShowAll[] = "show-all";
const char kAutofillUseMobileLabelDisambiguationParameterShowOne[] = "show-one";
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

bool IsAutofillCreditCardAssistEnabled() {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(kAutofillCreditCardAssist);
#endif
}

}  // namespace features
}  // namespace autofill
