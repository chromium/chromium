// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace autofill {
namespace features {

// All features in alphabetical order.
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutocompleteFilterForMeaningfulNames;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillAcrossIframes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAddressEnhancementVotes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAddressProfileSavePrompt;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAutoBlockSaveAddressProfilePrompt;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptExpirationDays;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptStrikeLimit;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAutoBlockUpdateAddressProfilePrompt;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptExpirationDays;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptStrikeLimit;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAddressProfileSavePromptNicknameSupport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAllowDuplicateFormSubmissions;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAllowNonHttpActivation;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillAugmentFormsInRenderer;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillCreateDataForTest;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillDisableFilling;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillDisableAddressImport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableAccountWalletStorage;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableAugmentedPhoneCountryCode;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableDependentLocalityParsing;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableHideSuggestionsUI;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableImportWhenMultiplePhoneNumbers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature
    kAutofillEnableInfoBarAccountIndicationFooterForSingleAccountUsers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature
    kAutofillEnableInfoBarAccountIndicationFooterForSyncUsers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature
    kAutofillEnablePasswordInfoBarAccountIndicationFooter;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForApartmentNumbers;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableLabelPrecedenceForTurkishAddresses;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableProfileDeduplication;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForParsingWithSharedLabels;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForMoreStructureInNames;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForMoreStructureInAddresses;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForMergingSubsetNames;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableSupportForHonorificPrefixes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillExtractAllDatalists;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillFixFillableFieldTypes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillIgnoreAutocompleteForImport;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillServerTypeTakesPrecedence;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRefillWithRendererIds;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillNameSectionsWithRendererIds;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillKeyboardAccessory;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillLabelAffixRemoval;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillPruneSuggestions;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillMetadataUploads;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillParsingPatternsFromRemote;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillParsingPatternsLanguageDetection;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillParsingPatternsNegativeMatching;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillParsingPatternsLanguageDependent;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillPreventMixedFormsFilling;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillProbableFormSubmissionInBrowser;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillProfileClientValidation;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillProfileImportFromUnfocusableFields;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillProfileServerValidation;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRetrieveOverallPredictionsFromCache;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillSaveAndFillVPA;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillSectionUponRedundantNameInfo;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillServerCommunication;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillShowTypePredictions;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillSkipComparingInferredLabels;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillStrictContextualCardNameConditions;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillTokenPrefixMatching;
COMPONENT_EXPORT(AUTOFILL) extern const base::Feature kAutofillUploadThrottling;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseAlternativeStateNameMap;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseImprovedLabelDisambiguation;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseNewSectioningMethod;

COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillEnableAblationStudy;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForAddressesParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForPaymentsParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleParam;

#if defined(OS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillManualFallbackAndroid;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillRefreshStyleAndroid;
#endif  // OS_ANDROID

#if defined(OS_ANDROID) || defined(OS_IOS)
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseMobileLabelDisambiguation;
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterName[];
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterShowOne[];
COMPONENT_EXPORT(AUTOFILL)
extern const char kAutofillUseMobileLabelDisambiguationParameterShowAll[];
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_APPLE)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
COMPONENT_EXPORT(AUTOFILL)
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // defined(OS_APPLE)

#if defined(OS_IOS)
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAutofillUseUniqueRendererIDsOnIOS;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature
    kAutofillEnableNewAddressProfileCreationInSettingsOnIOS;
#endif  // OS_IOS

#if defined(OS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kAndroidAutofillQueryServerFieldTypes;
COMPONENT_EXPORT(AUTOFILL)
extern const base::Feature kWalletRequiresFirstSyncSetupComplete;
#endif

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
