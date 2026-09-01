// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_SPLIT_METRICS_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_SPLIT_METRICS_H_

#include <optional>

#include "components/search_engines/template_url.h"

class SearchTermsData;

namespace metrics {
class ProfileMetricsService;
}  // namespace metrics

namespace search_engines {

// Classification of search engine definitions undergoing an ID split /
// migration (e.g. Yahoo! JAPAN in JP).
//
// LINT.IfChange(OseSplitType)
enum class OseSplitType {
  kUnknown = 0,  // Unrecognized prepopulated ID or unmapped state
  kLegacy = 1,   // Legacy (pre-migration)
  kNew = 2,      // New (post-migration)
  kCustom = 3,   // Custom user-defined or non-prepopulated engine
  kMaxValue = kCustom,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:OseSplitType)

// Detailed configuration state of an individual search engine definition
// undergoing an ID split / migration.
//
// LINT.IfChange(OseSplitEngineState)
enum class OseSplitEngineState {
  kUnknown = 0,

  // Legacy (pre-migration)
  kLegacyDse = 1,
  kLegacyDseCustomized = 2,
  kLegacyNotDse = 3,
  kLegacyNotDseCustomized = 4,

  // New (post-migration)
  kNewDse = 5,
  kNewDseCustomized = 6,
  kNewNotDse = 7,
  kNewNotDseCustomized = 8,

  // Regulatory / Device choice variants
  kLegacyDseDeviceChoice = 9,
  kLegacyNotDseDeviceChoice = 10,
  kNewDseDeviceChoice = 11,
  kNewNotDseDeviceChoice = 12,

  // Custom
  kCustomDse = 13,
  kCustomNotDse = 14,

  kMaxValue = kCustomNotDse,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:OseSplitEngineState)

// Returns the `OseSplitType` for `turl` if it represents Yahoo! JAPAN.
// Returns `std::nullopt` if `turl` is not a Yahoo! JAPAN search engine.
std::optional<OseSplitType> InspectYahooJapanEngineType(
    const TemplateURL& turl,
    const SearchTermsData& search_terms_data);

// Returns the `OseSplitEngineState` for `turl` if it represents Yahoo! JAPAN.
// Returns `std::nullopt` if `turl` is not a Yahoo! JAPAN search engine.
std::optional<OseSplitEngineState> InspectYahooJapanEngineState(
    const TemplateURL& turl,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data);

// Records profile load metrics for users in search engine split regions:
// - Search.OseSplitYahooJapan.DseTypeOnProfileLoad{Profile}
// - Search.OseSplitYahooJapan.CountOnProfileLoad{Profile}
// - Search.OseSplitYahooJapan.EngineStateOnProfileLoad{Profile}
void RecordSearchEngineSplitProfileLoadMetrics(
    TemplateURL::OwnedTemplateURLVectorSpan template_urls,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    metrics::ProfileMetricsService& profile_metrics_service);

// Records settings page load metrics:
// - Search.OseSplitYahooJapan.DseTypeOnSettingsPageLoad{Profile}
// - Search.OseSplitYahooJapan.CountOnSettingsPageLoad{Profile}
// - Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad{Profile}
void RecordSearchEngineSplitSettingsPageLoadMetrics(
    TemplateURL::TemplateURLVectorSpan template_urls,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    metrics::ProfileMetricsService& profile_metrics_service);

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_SPLIT_METRICS_H_
