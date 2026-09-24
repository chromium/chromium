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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/android/template_url_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/search_engines/android/jni_headers/SearchEngineSettingsDataProvider_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

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

std::vector<const TemplateURL*>
SearchEngineSettingsDataProvider::GetTemplateUrlsByCategory(
    TemplateUrlCategory category,
    template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids)
    const {
  std::vector<const TemplateURL*> result;

  for (TemplateURL* turl : template_url_service_->GetTemplateURLs()) {
    if (disabled_starter_pack_ids.Has(turl->starter_pack_id())) {
      continue;
    }

    bool is_default = template_url_service_->ShowInDefaultList(turl);
    bool is_extension = turl->type() == TemplateURL::OMNIBOX_API_EXTENSION;
    bool is_active = template_url_service_->ShowInActivesList(turl);
    bool is_hidden = template_url_service_->HiddenFromLists(turl);

    switch (category) {
      case TemplateUrlCategory::kDefault:
        if (is_default) {
          result.push_back(turl);
        }
        break;
      case TemplateUrlCategory::kActiveSiteSearch:
        if (!is_default && !is_hidden && !is_extension && is_active) {
          result.push_back(turl);
        }
        break;
      case TemplateUrlCategory::kInactiveSiteSearch:
        if (!is_default && !is_hidden && !is_extension && !is_active) {
          result.push_back(turl);
        }
        break;
      case TemplateUrlCategory::kExtension:
        if (!is_default && !is_hidden && is_extension) {
          result.push_back(turl);
        }
        break;
    }
  }

  if (category == TemplateUrlCategory::kActiveSiteSearch ||
      category == TemplateUrlCategory::kInactiveSiteSearch) {
    std::ranges::sort(
        result, ::internal::OrderTemplateUrlsByManagedAndAlphabetically());
  }

  return result;
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

#if BUILDFLAG(IS_ANDROID)
void SearchEngineSettingsDataProvider::Destroy(JNIEnv* env) {
  delete this;
}

// static
template_url_starter_pack_data::StarterPackIdSet
SearchEngineSettingsDataProvider::GetDisabledStarterPackIdsForAndroid() {
  template_url_starter_pack_data::StarterPackIdSet disabled_ids;
  if (!omnibox_feature_configs::ContextualSearch::Get().starter_pack_page) {
    disabled_ids.Put(template_url_starter_pack_data::StarterPackId::kPage);
  }
  // TODO(crbug.com/512766345): Add profile check for aimode and gemini.
  if (!base::FeatureList::IsEnabled(omnibox::kStarterPackExpansion)) {
    disabled_ids.Put(template_url_starter_pack_data::StarterPackId::kGemini);
  }
  disabled_ids.Put(template_url_starter_pack_data::StarterPackId::kBookmarks);
  return disabled_ids;
}

std::vector<const TemplateURL*>
SearchEngineSettingsDataProvider::GetTemplateUrlsByCategory(
    JNIEnv* env,
    TemplateUrlCategory category) const {
  CHECK(category >= TemplateUrlCategory::kDefault &&
        category <= TemplateUrlCategory::kExtension);
  return GetTemplateUrlsByCategory(category,
                                   GetDisabledStarterPackIdsForAndroid());
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace search_engines

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(SearchEngineSettingsDataProvider)
#endif
