// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_settings_data_provider.h"

#include <algorithm>

#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/search_engine_split_metrics.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/ui_utils.h"
#include "components/search_engines/util.h"

namespace search_engines {

CategorizedTemplateUrls::CategorizedTemplateUrls() = default;
CategorizedTemplateUrls::CategorizedTemplateUrls(
    const CategorizedTemplateUrls& other) = default;
CategorizedTemplateUrls& CategorizedTemplateUrls::operator=(
    const CategorizedTemplateUrls& other) = default;
CategorizedTemplateUrls::CategorizedTemplateUrls(
    CategorizedTemplateUrls&& other) = default;
CategorizedTemplateUrls& CategorizedTemplateUrls::operator=(
    CategorizedTemplateUrls&& other) = default;
CategorizedTemplateUrls::~CategorizedTemplateUrls() = default;

SearchEngineSettingsDataProvider::SearchEngineSettingsDataProvider(
    TemplateURLService& template_url_service,
    const TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
    regional_capabilities::RegionalCapabilitiesService&
        regional_capabilities_service,
    metrics::ProfileMetricsService& profile_metrics_service)
    : template_url_service_(template_url_service),
      prepopulate_data_resolver_(prepopulate_data_resolver),
      regional_capabilities_service_(regional_capabilities_service),
      profile_metrics_service_(profile_metrics_service) {}

SearchEngineSettingsDataProvider::~SearchEngineSettingsDataProvider() = default;

CategorizedTemplateUrls
SearchEngineSettingsDataProvider::GetCategorizedTemplateURLs(
    template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids)
    const {
  CategorizedTemplateUrls data;

  for (TemplateURL* url : template_url_service_->GetTemplateURLs()) {
    // Exclude those URLs that cannot be enabled or should be hidden.
    if (disabled_starter_pack_ids.Has(url->starter_pack_id()) ||
        template_url_service_->HiddenFromLists(url)) {
      continue;
    }

    const bool is_starter_pack =
        url->starter_pack_id() !=
        template_url_starter_pack_data::StarterPackId::kNone;
    const bool is_extension = url->type() == TemplateURL::OMNIBOX_API_EXTENSION;

    if (template_url_service_->ShowInDefaultList(url)) {
      data.active_site_shortcuts.push_back(url);
    } else if (is_starter_pack || is_extension) {
      if (template_url_service_->ShowInActivesList(url)) {
        data.active_feature_shortcuts.push_back(url);
      } else {
        data.inactive_feature_shortcuts.push_back(url);
      }
    } else {
      if (template_url_service_->ShowInActivesList(url)) {
        data.active_site_shortcuts.push_back(url);
      } else {
        data.inactive_site_shortcuts.push_back(url);
      }
    }
  }

  std::ranges::sort(
      data.active_site_shortcuts,
      ::internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
          prepopulate_data_resolver_->GetPrepopulatedEngines()));
  std::ranges::sort(data.inactive_site_shortcuts,
                    ::internal::OrderTemplateUrlsByManagedAndAlphabetically());

  return data;
}

bool SearchEngineSettingsDataProvider::CanRecordSettingsPageLoadMetrics()
    const {
  return !has_recorded_metrics_ &&
         regional_capabilities_service_->IsSearchEngineSplitRegion();
}

void SearchEngineSettingsDataProvider::MaybeRecordSettingsPageLoadMetrics(
    TemplateURL::TemplateURLVectorSpan displayed_engines) {
  if (!CanRecordSettingsPageLoadMetrics()) {
    return;
  }
  has_recorded_metrics_ = true;

  RecordSearchEngineSplitSettingsPageLoadMetrics(
      displayed_engines, template_url_service_->GetDefaultSearchProvider(),
      template_url_service_->search_terms_data(), *profile_metrics_service_);
}

void SearchEngineSettingsDataProvider::MaybeRecordSettingsPageLoadMetrics(
    const CategorizedTemplateUrls& displayed_engines) {
  if (!CanRecordSettingsPageLoadMetrics()) {
    return;
  }

  TemplateURL::TemplateURLVector all_engines;
  all_engines.reserve(displayed_engines.active_site_shortcuts.size() +
                      displayed_engines.inactive_site_shortcuts.size() +
                      displayed_engines.active_feature_shortcuts.size() +
                      displayed_engines.inactive_feature_shortcuts.size());
  for (const auto* category : {&displayed_engines.active_site_shortcuts,
                               &displayed_engines.inactive_site_shortcuts,
                               &displayed_engines.active_feature_shortcuts,
                               &displayed_engines.inactive_feature_shortcuts}) {
    all_engines.insert(all_engines.end(), category->begin(), category->end());
  }

  MaybeRecordSettingsPageLoadMetrics(all_engines);
}

}  // namespace search_engines
