// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DEBUG_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DEBUG_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

// The features in this namespace contains are not meant to be rolled out. They
// are only intended for manual debugging and testing purposes.
namespace autofill::features::debug {

// All features in alphabetical order.

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAiForceOptIn);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCapturedSiteTestsMetricsScraper);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kAutofillCapturedSiteTestsMetricsScraperOutputDir);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE_PARAM(
    std::string,
    kAutofillCapturedSiteTestsMetricsScraperHistogramRegex);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCapturedSiteTestsUseAutofillFlow);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableProfileUpdates);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableSilentProfileUpdates);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableSuggestionStrikeDatabase);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillLogToTerminal);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillOverridePredictions);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE_PARAM(std::string,
                           kAutofillOverridePredictionsSpecification);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE_PARAM(std::string, kAutofillOverridePredictionsJson);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillServerCommunication);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillShowTypePredictions);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE_PARAM(bool, kAutofillShowTypePredictionsVerboseParam);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE_PARAM(bool, kAutofillShowTypePredictionsAsTitleParam);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSkipDeduplicationRequirements);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUnionTypesSingleTypeInAutofillInformation);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillUploadThrottling);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kShowDomNodeIDs);

}  // namespace autofill::features::debug

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_DEBUG_FEATURES_H_
