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
extern const base::Feature kAutofillAddressEnhancementVotes;
extern const base::Feature kAutofillAllowDuplicateFormSubmissions;
extern const base::Feature kAutofillAllowHtmlTypeCountryCodesWithFullNames;
extern const base::Feature kAutofillAllowNonHttpActivation;
extern const base::Feature kAutofillAlwaysFillAddresses;
extern const base::Feature
    kAutofillApplyNegativePatternsForFieldTypeDetectionHeuristics;
extern const base::Feature kAutofillCacheQueryResponses;
extern const base::Feature kAutofillCreateDataForTest;
extern const base::Feature kAutofillEnableAccountWalletStorage;
extern const base::Feature kAutofillEnableAugmentedPhoneCountryCode;
extern const base::Feature kAutofillEnableHideSuggestionsUI;
extern const base::Feature kAutofillEnableSupportForMoreStructureInNames;
extern const base::Feature kAutofillEnableSupportForMoreStructureInAddresses;
extern const base::Feature kAutofillEnableSupportForMergingSubsetNames;
extern const base::Feature kAutofillEnableSupportForHouseNumbers;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForHeuristics;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForQuery;
extern const base::Feature kAutofillEnforceMinRequiredFieldsForUpload;
extern const base::Feature kAutofillExtractAllDatalists;
extern const base::Feature kAutofillFixFillableFieldTypes;
extern const base::Feature kAutofillImportPrefilledCountryAndStateValues;
extern const base::Feature kAutofillKeepInitialFormValuesInCache;
extern const base::Feature kAutofillRetrieveFromCacheWithRendererIds;
extern const base::Feature
    kAutofillRetrieveFromCacheWithFieldSignatureAsFallback;
extern const base::Feature kAutofillKeyboardAccessory;
extern const base::Feature kAutofillLabelAffixRemoval;
extern const base::Feature kAutofillPruneSuggestions;
extern const base::Feature kAutofillMetadataUploads;
extern const base::Feature kAutofillOffNoServerData;
extern const base::Feature kAutofillPreventMixedFormsFilling;
extern const base::Feature kAutofillProbableFormSubmissionInBrowser;
extern const base::Feature kAutofillProfileClientValidation;
extern const base::Feature kAutofillProfileImportFromUnfocusableFields;
extern const base::Feature kAutofillProfileImportFromUnifiedSection;
extern const base::Feature kAutofillProfileServerValidation;
extern const base::Feature kAutofillRestrictUnownedFieldsToFormlessCheckout;
extern const base::Feature kAutofillRichMetadataQueries;
extern const base::Feature kAutofillSaveAndFillVPA;
extern const base::Feature kAutofillSectionUponRedundantNameInfo;
extern const base::Feature kAutofillServerCommunication;
extern const base::Feature kAutofillShowTypePredictions;
extern const base::Feature kAutofillSkipComparingInferredLabels;
extern const base::Feature kAutofillSkipFillingFieldsWithChangedValues;
extern const base::Feature kAutofillTokenPrefixMatching;
extern const base::Feature kAutofillTouchToFill;
extern const base::Feature kAutofillUploadThrottling;
extern const base::Feature kAutofillUseImprovedLabelDisambiguation;
extern const base::Feature kAutofillUseNewSectioningMethod;
extern const base::Feature kAutofillUsePageLanguageToTranslateCountryNames;
extern const base::Feature kAutofillUseParseCityStateCountryZipCodeInHeuristic;
extern const base::Feature kAutofillUseVariationCountryCode;

#if defined(OS_ANDROID)
extern const base::Feature kAutofillManualFallbackAndroid;
extern const base::Feature kAutofillRefreshStyleAndroid;
#endif  // OS_ANDROID

#if defined(OS_ANDROID) || defined(OS_IOS)
extern const base::Feature kAutofillUseMobileLabelDisambiguation;
extern const char kAutofillUseMobileLabelDisambiguationParameterName[];
extern const char kAutofillUseMobileLabelDisambiguationParameterShowOne[];
extern const char kAutofillUseMobileLabelDisambiguationParameterShowAll[];
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_APPLE)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // defined(OS_APPLE)

#if defined(OS_IOS)
extern const base::Feature kAutofillUseUniqueRendererIDsOnIOS;
#endif  // OS_IOS

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
