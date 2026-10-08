// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_SETTINGS_DATA_PROVIDER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_SETTINGS_DATA_PROVIDER_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_starter_pack_data.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#endif

class TemplateURLService;

namespace metrics {
class ProfileMetricsService;
}

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace TemplateURLPrepopulateData {
class Resolver;
}

namespace search_engines {

// Container for categorized search engine metadata. It groups TemplateURLs
// into specific buckets based on their type (e.g., prepopulated, starter
// pack, or custom) and their active state. This structure is primarily used
// to pass organized lists to the WebUI settings page.
struct CategorizedTemplateUrls {
  CategorizedTemplateUrls();
  CategorizedTemplateUrls(const CategorizedTemplateUrls& other);
  CategorizedTemplateUrls& operator=(const CategorizedTemplateUrls& other);
  CategorizedTemplateUrls(CategorizedTemplateUrls&& other);
  CategorizedTemplateUrls& operator=(CategorizedTemplateUrls&& other);
  ~CategorizedTemplateUrls();

  // All prepopulated engines retrieved from `GetPrepopulatedEngines()`, and
  // custom shortcuts that are currently active. This always includes the
  // current default search engine.
  TemplateURL::TemplateURLVector active_site_shortcuts;
  // Custom shortcuts that are currently inactive.
  TemplateURL::TemplateURLVector inactive_site_shortcuts;
  // Shortcuts with a starter pack id and extensions that are currently
  // active.
  TemplateURL::TemplateURLVector active_feature_shortcuts;
  // Shortcuts with a starter pack id and extensions that are currently
  // inactive.
  TemplateURL::TemplateURLVector inactive_feature_shortcuts;
};

// Container for search engines split into prepopulated and recently visited
// lists, used primarily by mobile settings screens.
// TODO(crbug.com/568358837): Handle data differences with
// `DefaultSearchEnginePickerData` and merge into it.
struct PrepopulatedAndRecentlyVisitedTemplateUrls {
  PrepopulatedAndRecentlyVisitedTemplateUrls();
  PrepopulatedAndRecentlyVisitedTemplateUrls(
      const PrepopulatedAndRecentlyVisitedTemplateUrls& other);
  PrepopulatedAndRecentlyVisitedTemplateUrls& operator=(
      const PrepopulatedAndRecentlyVisitedTemplateUrls& other);
  PrepopulatedAndRecentlyVisitedTemplateUrls(
      PrepopulatedAndRecentlyVisitedTemplateUrls&& other);
  PrepopulatedAndRecentlyVisitedTemplateUrls& operator=(
      PrepopulatedAndRecentlyVisitedTemplateUrls&& other);
  ~PrepopulatedAndRecentlyVisitedTemplateUrls();

  // All prepopulated engines retrieved from `GetPrepopulatedEngines()`. This
  // always includes the current default search engine.
  TemplateURL::TemplateURLVector prepopulated_urls;
  // A limited number of recently visited URLs, defined and sorted through
  // `SortAndFilterRecentlyVisitedURLs()`.
  TemplateURL::TemplateURLVector recently_visited_urls;
};

// Container for search engines displayed in the default search engine picker
// dialog in settings.
struct DefaultSearchEnginePickerData {
  DefaultSearchEnginePickerData();
  DefaultSearchEnginePickerData(const DefaultSearchEnginePickerData& other);
  DefaultSearchEnginePickerData& operator=(
      const DefaultSearchEnginePickerData& other);
  DefaultSearchEnginePickerData(DefaultSearchEnginePickerData&& other);
  DefaultSearchEnginePickerData& operator=(
      DefaultSearchEnginePickerData&& other);
  ~DefaultSearchEnginePickerData();

  // Primary engines shown in the main radio group (regional prepopulated
  // engines, policy-created default search providers, and the current default
  // search engine). This always includes the current default search engine.
  //
  // TODO(crbug.com/567524787): Attach a reason to each entry explaining why it
  // is shown (e.g. regional prepopulated, non-regional prepopulated, policy
  // recommended/enforced, current DSE), computed in
  // `GetDefaultSearchEnginePickerData()`.
  TemplateURL::TemplateURLVector primary;

  // TODO(crbug.com/568358837): Populate and use it.
  TemplateURL::TemplateURLVector recently_visited;
};

// Defines the category of template URLs to be displayed in different UI
// sections.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.search_engines
enum class TemplateUrlCategory {
  kDefault = 0,
  kActiveSiteSearch = 1,
  kInactiveSiteSearch = 2,
  kExtension = 3,
};

// Prepares and dispenses TemplateURL data for search engine settings screens,
// and owns the once-per-page-load settings telemetry for those screens.
//
// Instances are created through
// `TemplateURLService::CreateSearchEngineSettingsDataProvider()` and are owned
// by a settings screen (e.g. `SearchEngineAdapter` on Android, or the fragment
// backing the site search page) and destroyed when that screen is dismissed.
// Exactly one provider backs a screen, even when the screen is composed of
// several sub-components, so that the telemetry below is emitted once per page
// load. Multiple provider instances may still exist concurrently when one
// settings screen is opened on top of another, which counts as two page loads.
//
// The dependencies passed to the constructor (`TemplateURLService`,
// `RegionalCapabilitiesService`, etc.) are profile-keyed services whose
// lifetimes exceed that of this provider and the settings UI.
//
// Data extraction methods are const and free of side effects. Telemetry is
// only emitted through `MaybeRecordSettingsPageLoadMetrics()`, which the
// primary UI controller calls once it knows which engines it is actually
// displaying. Callers may call it on every refresh: it records at most once
// per provider instance, so the engine list changing while the page is open
// does not record again. Only a new page load, which constructs a new
// provider, records again.
class SearchEngineSettingsDataProvider {
 public:
  SearchEngineSettingsDataProvider(
      TemplateURLService& template_url_service,
      const TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver,
      regional_capabilities::RegionalCapabilitiesService&
          regional_capabilities_service,
      metrics::ProfileMetricsService& profile_metrics_service);

  SearchEngineSettingsDataProvider(const SearchEngineSettingsDataProvider&) =
      delete;
  SearchEngineSettingsDataProvider& operator=(
      const SearchEngineSettingsDataProvider&) = delete;

  ~SearchEngineSettingsDataProvider();

  // Returns a CategorizedTemplateUrls object containing all TemplateURLs
  // categorized into specific buckets (active/inactive site shortcuts and
  // feature shortcuts).
  //
  // The ordering of `active_site_shortcuts` is specifically handled to ensure
  // that prepopulated regional engines appear first in the order defined by the
  // prepopulate_data_resolver. Enterprise policy search engines (both mandatory
  // and recommended) and user-added (custom) engines are appended to the end of
  // this list and sorted alphabetically.
  //
  // `disabled_starter_pack_ids` contains all `starter_pack_id`s that should not
  // be included in either of the lists.
  CategorizedTemplateUrls GetCategorizedTemplateURLs(
      template_url_starter_pack_data::StarterPackIdSet
          disabled_starter_pack_ids = {}) const;

  // Returns an object containing prepopulated engines and recently visited
  // engines for mobile settings screens.
  PrepopulatedAndRecentlyVisitedTemplateUrls
  GetPrepopulatedAndRecentlyVisitedTemplateURLs() const;

  // Returns the search engines to display in the default search engine picker
  // dialog in settings.
  DefaultSearchEnginePickerData GetDefaultSearchEnginePickerData() const;

  // Returns template URLs filtered by `category` and sorted appropriately
  // (managed first, then alphabetically for site search).
  std::vector<const TemplateURL*> GetTemplateUrlsByCategory(
      TemplateUrlCategory category,
      template_url_starter_pack_data::StarterPackIdSet
          disabled_starter_pack_ids = {}) const;

  // Records the `Search.OseSplitYahooJapan.*` settings page load metrics for
  // `displayed_engines`, which must be the set of engines the settings screen
  // is actually showing the user.
  //
  // Only the first call on a given instance records anything, so callers may
  // call this unconditionally every time they refresh their list. Recording is
  // additionally limited to search engine split regions.
  void MaybeRecordSettingsPageLoadMetrics(
      TemplateURL::TemplateURLVectorSpan displayed_engines);
  void MaybeRecordSettingsPageLoadMetrics(
      std::initializer_list<TemplateURL::TemplateURLVectorSpan>
          displayed_engine_lists);
  void MaybeRecordSettingsPageLoadMetrics(
      const CategorizedTemplateUrls& displayed_engines);

#if BUILDFLAG(IS_ANDROID)
  // Computes the set of disabled starter pack IDs specific to Android.
  static template_url_starter_pack_data::StarterPackIdSet
  GetDisabledStarterPackIdsForAndroid();

  void Destroy(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject>
  GetPrepopulatedAndRecentlyVisitedTemplateURLs(JNIEnv* env) const;

  std::vector<const TemplateURL*> GetTemplateUrlsByCategory(
      JNIEnv* env,
      TemplateUrlCategory category) const;

  void MaybeRecordSettingsPageLoadMetrics(
      JNIEnv* env,
      const base::android::JavaRef<jlongArray>& j_engine_ids);
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  const raw_ref<TemplateURLService> template_url_service_;
  const raw_ref<const TemplateURLPrepopulateData::Resolver>
      prepopulate_data_resolver_;
  const raw_ref<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_service_;
  const raw_ref<metrics::ProfileMetricsService> profile_metrics_service_;

  // Whether `MaybeRecordSettingsPageLoadMetrics()` has already recorded for
  // this instance.
  bool has_recorded_metrics_ = false;
};

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_SETTINGS_DATA_PROVIDER_H_
