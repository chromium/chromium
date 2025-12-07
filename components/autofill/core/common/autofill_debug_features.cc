// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_debug_features.h"

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace autofill::features::debug {

// When enabled, the user will be considered to be opted-in to Autofill AI by
// default. Used for development purposes.
BASE_FEATURE(kAutofillAiForceOptIn, base::FEATURE_DISABLED_BY_DEFAULT);

// Testing tool that collects metrics during a run of the captured site tests
// and dumps the collected metrics into a specified output directory.
// For each test, a file named {test-name}.txt is created. It contains all the
// collected metrics in the following format.
// histogram-name-1
// bucket value
// ...
// histogram-name-2
// ...
// The set of metrics can be restricted using
// `kAutofillCapturedSiteTestsMetricsScraperMetricNames`.
// It is helpful in conjunction with `tools/captured_sites/metrics-scraper.py`.
BASE_FEATURE(kAutofillCapturedSiteTestsMetricsScraper,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Name of the directory to write the results into.
BASE_FEATURE_PARAM(std::string,
                   kAutofillCapturedSiteTestsMetricsScraperOutputDir,
                   &kAutofillCapturedSiteTestsMetricsScraper,
                   "output_dir",
                   "/tmp/");
// A regex matching the histogram names that should be dumped. If not specified,
// the metrics of all histograms dumped.
BASE_FEATURE_PARAM(std::string,
                   kAutofillCapturedSiteTestsMetricsScraperHistogramRegex,
                   &kAutofillCapturedSiteTestsMetricsScraper,
                   "histogram_regex",
                   "");

// If enabled, Captured Site Tests will use 'AutofillFlow' utility to trigger
// the autofill action. This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillCapturedSiteTestsUseAutofillFlow,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill will not apply updates to address profiles based on data
// extracted from submitted forms. This feature is mostly for debugging and
// testing purposes and is not supposed to be launched.
BASE_FEATURE(kAutofillDisableProfileUpdates, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, Autofill will not apply silent updates to the structure of
// addresses and names. This feature is mostly for debugging and testing
// purposes and is not supposed to be launched.
BASE_FEATURE(kAutofillDisableSilentProfileUpdates,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for disabling suppressing suggestions based on the strike
// database.
BASE_FEATURE(kAutofillDisableSuggestionStrikeDatabase,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables logging the content of chrome://autofill-internals to the terminal.
BASE_FEATURE(kAutofillLogToTerminal, base::FEATURE_DISABLED_BY_DEFAULT);

// Allows passing a set of overrides for Autofill server predictions.
// Example command line to override server predictions manually:
// chrome --enable-features=AutofillOverridePredictions:spec/1_2_4-7_8_9
// This creates two manual overrides that supersede server predictions as
// follows:
// * The server prediction for the field with signature 2 in the form with
//   signature 1 is overridden to be 4 (NAME_MIDDLE).
// * The server prediction for the field with signature 8 in the form with
//   signature 7 is overridden to be 9 (EMAIL_ADDRESS).
//
// See
// components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h
// for more examples and details on how to specify overrides.
BASE_FEATURE(kAutofillOverridePredictions, base::FEATURE_DISABLED_BY_DEFAULT);

// The override specification in string form.
// See `OverrideFormat::kSpec` for details.
BASE_FEATURE_PARAM(std::string,
                   kAutofillOverridePredictionsSpecification,
                   &kAutofillOverridePredictions,
                   "spec",
                   "");

// The override specification in Base64-encoded JSON.
// See `OverrideFormat::kJson` for details.
BASE_FEATURE_PARAM(std::string,
                   kAutofillOverridePredictionsJson,
                   &kAutofillOverridePredictions,
                   "json",
                   "");

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
BASE_FEATURE(kAutofillServerCommunication, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
BASE_FEATURE(kAutofillShowTypePredictions, base::FEATURE_DISABLED_BY_DEFAULT);
// This variation controls whether the verbose version of the feature is used.
// In this version more information is attached to the respective DOM element,
// such as aria labels and descriptions and select element options values and
// texts.
BASE_FEATURE_PARAM(bool,
                   kAutofillShowTypePredictionsVerboseParam,
                   &kAutofillShowTypePredictions,
                   "verbose",
                   false);

// This variation controls whether the autofill information of the element
// is shown as 'title' of the form field elements. If this parameter is on,
// the title attribute will be overwritten with autofill information.
// By default this is disabled to avoid data collection corruption.
BASE_FEATURE_PARAM(bool,
                   kAutofillShowTypePredictionsAsTitleParam,
                   &kAutofillShowTypePredictions,
                   "as-title",
                   false);

// If enabled, forces the deduplication pipeline to run on every startup,
// bypassing the 'once per milestone' limit.
BASE_FEATURE(kAutofillSkipDeduplicationRequirements,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, ensures that the "autofill-information" attribute only contains a
// single FieldType in "overall type: <FieldTypes>". For example,
// "overall type: NAME_FULL, USERNAME" becomes "overall type: NAME_FULL" if the
// feature is enabled.
// TODO(crbug.com/435354393): Migrate the infrastructure to union types and
// remove this feature.
BASE_FEATURE(kAutofillUnionTypesSingleTypeInAutofillInformation,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Autofill upload throttling limits uploading a form to the Autofill server
// more than once over a `kAutofillUploadThrottlingPeriodInDays` period.
// This feature is for testing purposes and is not supposed
// to be launched.
BASE_FEATURE(kAutofillUploadThrottling, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing DOM Node ID of elements.
BASE_FEATURE(kShowDomNodeIDs, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace autofill::features::debug
