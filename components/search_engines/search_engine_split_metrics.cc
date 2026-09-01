// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_split_metrics.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/types/to_address.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace search_engines {

namespace {

enum class Timing {
  kProfileLoad,
  kSettingsPageLoad,
};

template <typename Range>
void RecordSearchEngineSplitMetricsInternal(
    const Range& template_urls,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    metrics::ProfileMetricsService& profile_metrics_service,
    Timing timing) {
  if (default_search_provider) {
    if (std::optional<OseSplitType> dse_type = InspectYahooJapanEngineType(
            *default_search_provider, search_terms_data);
        dse_type.has_value()) {
      profile_metrics_service.UmaHistogramEnumeration(
          timing == Timing::kProfileLoad
              ? "Search.OseSplitYahooJapan.DseTypeOnProfileLoad"
              : "Search.OseSplitYahooJapan.DseTypeOnSettingsPageLoad",
          *dse_type);
    }
  }

  int yahoo_count = 0;
  for (const auto& turl : template_urls) {
    const TemplateURL& deref_turl = CHECK_DEREF(base::to_address(turl));
    if (std::optional<OseSplitEngineState> state = InspectYahooJapanEngineState(
            deref_turl, default_search_provider, search_terms_data);
        state.has_value()) {
      yahoo_count++;
      profile_metrics_service.UmaHistogramEnumeration(
          timing == Timing::kProfileLoad
              ? "Search.OseSplitYahooJapan.EngineStateOnProfileLoad"
              : "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad",
          *state);
    }
  }

  profile_metrics_service.UmaHistogramCounts100(
      timing == Timing::kProfileLoad
          ? "Search.OseSplitYahooJapan.CountOnProfileLoad"
          : "Search.OseSplitYahooJapan.CountOnSettingsPageLoad",
      yahoo_count);
}

}  // namespace

std::optional<OseSplitType> InspectYahooJapanEngineType(
    const TemplateURL& turl,
    const SearchTermsData& search_terms_data) {
  if (turl.GetEngineType(search_terms_data) != SEARCH_ENGINE_YAHOO_JP) {
    return std::nullopt;
  }

  int prepopulate_id = turl.prepopulate_id();
  if (prepopulate_id == TemplateURLPrepopulateData::yahoo_jp.id) {
    return OseSplitType::kLegacy;
  }
  if (prepopulate_id == TemplateURLPrepopulateData::yahoo_jp_next.id) {
    return OseSplitType::kNew;
  }
  if (prepopulate_id == 0) {
    return OseSplitType::kCustom;
  }
  return OseSplitType::kUnknown;
}

std::optional<OseSplitEngineState> InspectYahooJapanEngineState(
    const TemplateURL& turl,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data) {
  std::optional<OseSplitType> type =
      InspectYahooJapanEngineType(turl, search_terms_data);
  if (!type.has_value()) {
    return std::nullopt;
  }

  bool is_dse = (default_search_provider == &turl);
  bool is_customized = !turl.safe_for_autoreplace();
  bool is_device_choice = turl.CreatedByRegulatoryProgram();

  switch (*type) {
    case OseSplitType::kLegacy:
      if (is_device_choice) {
        return is_dse ? OseSplitEngineState::kLegacyDseDeviceChoice
                      : OseSplitEngineState::kLegacyNotDseDeviceChoice;
      }
      if (is_customized) {
        return is_dse ? OseSplitEngineState::kLegacyDseCustomized
                      : OseSplitEngineState::kLegacyNotDseCustomized;
      }
      return is_dse ? OseSplitEngineState::kLegacyDse
                    : OseSplitEngineState::kLegacyNotDse;

    case OseSplitType::kNew:
      if (is_device_choice) {
        return is_dse ? OseSplitEngineState::kNewDseDeviceChoice
                      : OseSplitEngineState::kNewNotDseDeviceChoice;
      }
      if (is_customized) {
        return is_dse ? OseSplitEngineState::kNewDseCustomized
                      : OseSplitEngineState::kNewNotDseCustomized;
      }
      return is_dse ? OseSplitEngineState::kNewDse
                    : OseSplitEngineState::kNewNotDse;

    case OseSplitType::kCustom:
      return is_dse ? OseSplitEngineState::kCustomDse
                    : OseSplitEngineState::kCustomNotDse;

    case OseSplitType::kUnknown:
      return OseSplitEngineState::kUnknown;
  }
}

void RecordSearchEngineSplitProfileLoadMetrics(
    TemplateURL::OwnedTemplateURLVectorSpan template_urls,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    metrics::ProfileMetricsService& profile_metrics_service) {
  RecordSearchEngineSplitMetricsInternal(
      template_urls, default_search_provider, search_terms_data,
      profile_metrics_service, Timing::kProfileLoad);
}

void RecordSearchEngineSplitSettingsPageLoadMetrics(
    TemplateURL::TemplateURLVectorSpan template_urls,
    const TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    metrics::ProfileMetricsService& profile_metrics_service) {
  RecordSearchEngineSplitMetricsInternal(
      template_urls, default_search_provider, search_terms_data,
      profile_metrics_service, Timing::kSettingsPageLoad);
}

}  // namespace search_engines
