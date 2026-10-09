// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_settings_data_provider.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_split_metrics.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/ui_utils.h"
#include "components/search_engines/util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/android/template_url_android.h"
#include "components/search_engines/search_engines_switches.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/search_engines/android/jni_headers/PrepopulatedAndRecentlyVisitedTemplateURLs_jni.h"
#include "components/search_engines/android/jni_headers/SearchEngineSettingsDataProvider_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace search_engines {
namespace {
enum class SearchEngineListSurface {
  kDsePicker,
  kFullList,
};

void RecordSearchEngineCountInSettingsHistogram(SearchEngineListSurface surface,
                                                size_t count) {
  static int kMax = 50;
  switch (surface) {
    case SearchEngineListSurface::kDsePicker:
      base::UmaHistogramExactLinear(
          search_engines::kSearchEngineCountInSettingsDsePickerHistogram, count,
          kMax);
      break;
    case SearchEngineListSurface::kFullList:
      base::UmaHistogramExactLinear(
          search_engines::kSearchEngineCountInSettingsFullListHistogram, count,
          kMax);
      break;
  }
}
}  // namespace

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

PrepopulatedAndRecentlyVisitedTemplateUrls::
    PrepopulatedAndRecentlyVisitedTemplateUrls() = default;
PrepopulatedAndRecentlyVisitedTemplateUrls::
    PrepopulatedAndRecentlyVisitedTemplateUrls(
        const PrepopulatedAndRecentlyVisitedTemplateUrls& other) = default;
PrepopulatedAndRecentlyVisitedTemplateUrls&
PrepopulatedAndRecentlyVisitedTemplateUrls::operator=(
    const PrepopulatedAndRecentlyVisitedTemplateUrls& other) = default;
PrepopulatedAndRecentlyVisitedTemplateUrls::
    PrepopulatedAndRecentlyVisitedTemplateUrls(
        PrepopulatedAndRecentlyVisitedTemplateUrls&& other) = default;
PrepopulatedAndRecentlyVisitedTemplateUrls&
PrepopulatedAndRecentlyVisitedTemplateUrls::operator=(
    PrepopulatedAndRecentlyVisitedTemplateUrls&& other) = default;
PrepopulatedAndRecentlyVisitedTemplateUrls::
    ~PrepopulatedAndRecentlyVisitedTemplateUrls() = default;

DefaultSearchEnginePickerData::DefaultSearchEnginePickerData() = default;
DefaultSearchEnginePickerData::DefaultSearchEnginePickerData(
    const DefaultSearchEnginePickerData& other) = default;
DefaultSearchEnginePickerData& DefaultSearchEnginePickerData::operator=(
    const DefaultSearchEnginePickerData& other) = default;
DefaultSearchEnginePickerData::DefaultSearchEnginePickerData(
    DefaultSearchEnginePickerData&& other) = default;
DefaultSearchEnginePickerData& DefaultSearchEnginePickerData::operator=(
    DefaultSearchEnginePickerData&& other) = default;
DefaultSearchEnginePickerData::~DefaultSearchEnginePickerData() = default;

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

PrepopulatedAndRecentlyVisitedTemplateUrls SearchEngineSettingsDataProvider::
    GetPrepopulatedAndRecentlyVisitedTemplateURLs() const {
  PrepopulatedAndRecentlyVisitedTemplateUrls data;

  for (TemplateURL* url : template_url_service_->GetTemplateURLs()) {
    if (template_url_service_->HiddenFromLists(url)) {
      continue;
    }

    if (template_url_service_->ShowInDefaultList(url)) {
      data.prepopulated_urls.push_back(url);
      continue;
    }

    const bool is_starter_pack =
        url->starter_pack_id() !=
        template_url_starter_pack_data::StarterPackId::kNone;
    const bool is_extension = url->type() == TemplateURL::OMNIBOX_API_EXTENSION;

    if (is_starter_pack || is_extension) {
      continue;
    }

    data.recently_visited_urls.push_back(url);
  }

  std::ranges::sort(
      data.prepopulated_urls,
      ::internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
          prepopulate_data_resolver_->GetPrepopulatedEngines()));
  ::internal::SortAndFilterRecentlyVisitedURLs(data.recently_visited_urls);

  return data;
}

DefaultSearchEnginePickerData
SearchEngineSettingsDataProvider::GetDefaultSearchEnginePickerData() const {
  DefaultSearchEnginePickerData data;
  const TemplateURL* default_search_provider =
      template_url_service_->GetDefaultSearchProvider();

  for (TemplateURL* url : template_url_service_->GetTemplateURLs()) {
    // The current default search engine is always shown.
    if (url == default_search_provider) {
      data.primary.push_back(url);
      continue;
    }

    if (!template_url_service_->ShowInDefaultList(url)) {
      continue;
    }

    // `HiddenFromLists()` is not used here: it would also hide engines whose
    // keyword is claimed by a custom, site search or extension engine, which
    // is irrelevant to the picker since keywords are not displayed. Only hide
    // an engine when the engine winning its keyword is itself shown in the
    // picker (e.g. a policy-provided or regulatory program engine reusing a
    // prepopulated keyword), to avoid showing duplicates.
    const TemplateURL* keyword_winner =
        template_url_service_->GetTemplateURLForKeyword(url->keyword());
    if (keyword_winner && keyword_winner != url &&
        template_url_service_->ShowInDefaultList(keyword_winner)) {
      continue;
    }

    // TODO(crbug.com/567524562): Revisit the logic to ensure non-regional
    // prepopulated engines as also skipped.
    data.primary.push_back(url);
  }

  std::ranges::sort(
      data.primary,
      ::internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically(
          prepopulate_data_resolver_->GetPrepopulatedEngines()));

  RecordSearchEngineCountInSettingsHistogram(
      SearchEngineListSurface::kDsePicker, data.primary.size());

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

void SearchEngineSettingsDataProvider::MaybeRecordSettingsPageLoadMetrics(
    TemplateURL::TemplateURLVectorSpan displayed_engines) {
  if (has_recorded_metrics_) {
    return;
  }
  has_recorded_metrics_ = true;

  RecordSearchEngineCountInSettingsHistogram(
      SearchEngineListSurface::kFullList,
      std::ranges::count_if(displayed_engines, [](const TemplateURL* engine) {
        return engine->prepopulate_id() != 0;
      }));

  if (regional_capabilities_service_->IsSearchEngineSplitRegion()) {
    RecordSearchEngineSplitSettingsPageLoadMetrics(
        displayed_engines, template_url_service_->GetDefaultSearchProvider(),
        template_url_service_->search_terms_data(), *profile_metrics_service_);
  }
}

void SearchEngineSettingsDataProvider::MaybeRecordSettingsPageLoadMetrics(
    std::initializer_list<TemplateURL::TemplateURLVectorSpan>
        displayed_engine_lists) {
  if (has_recorded_metrics_) {
    return;
  }

  size_t total_size = 0;
  for (const auto& list : displayed_engine_lists) {
    total_size += list.size();
  }

  TemplateURL::TemplateURLVector all_engines;
  all_engines.reserve(total_size);
  for (const auto& list : displayed_engine_lists) {
    all_engines.insert(all_engines.end(), list.begin(), list.end());
  }

  MaybeRecordSettingsPageLoadMetrics(all_engines);
}

void SearchEngineSettingsDataProvider::MaybeRecordSettingsPageLoadMetrics(
    const CategorizedTemplateUrls& displayed_engines) {
  MaybeRecordSettingsPageLoadMetrics(
      {displayed_engines.active_site_shortcuts,
       displayed_engines.inactive_site_shortcuts,
       displayed_engines.active_feature_shortcuts,
       displayed_engines.inactive_feature_shortcuts});
}

#if BUILDFLAG(IS_ANDROID)
void SearchEngineSettingsDataProvider::Destroy(JNIEnv* env) {
  delete this;
}

base::android::ScopedJavaLocalRef<jobject>
SearchEngineSettingsDataProvider::GetPrepopulatedAndRecentlyVisitedTemplateURLs(
    JNIEnv* env) const {
  CHECK(base::FeatureList::IsEnabled(switches::kSearchSettingsUpdateV2));
  auto result = GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  std::vector<const TemplateURL*> prepopulated_urls;
  prepopulated_urls.reserve(result.prepopulated_urls.size());
  for (const auto& turl : result.prepopulated_urls) {
    prepopulated_urls.push_back(turl.get());
  }

  std::vector<const TemplateURL*> recently_visited_urls;
  recently_visited_urls.reserve(result.recently_visited_urls.size());
  for (const auto& turl : result.recently_visited_urls) {
    recently_visited_urls.push_back(turl.get());
  }

  return Java_PrepopulatedAndRecentlyVisitedTemplateURLs_create(
      env, prepopulated_urls, recently_visited_urls);
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

void SearchEngineSettingsDataProvider::MaybeRecordSettingsPageLoadMetrics(
    JNIEnv* env,
    const base::android::JavaRef<jlongArray>& j_engine_ids) {
  if (has_recorded_metrics_) {
    return;
  }

  std::vector<int64_t> ids;
  base::android::JavaLongArrayToInt64Vector(env, j_engine_ids, &ids);
  TemplateURL::TemplateURLVector displayed_engines;
  displayed_engines.reserve(ids.size());
  for (int64_t id : ids) {
    if (TemplateURL* turl =
            template_url_service_->GetTemplateURLForId(TemplateURLID(id))) {
      displayed_engines.push_back(turl);
    }
  }
  MaybeRecordSettingsPageLoadMetrics(displayed_engines);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace search_engines

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(SearchEngineSettingsDataProvider)
#endif
