// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/search/app_service_app_result.h"
#include "chrome/browser/ui/app_list/search/ranking/ranking_item_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/chromeos/devicetype_utils.h"

namespace app_list {

namespace {

using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;
using ::ash::string_matching::TokenizedStringMatch;

constexpr double kEps = 1e-5;

// The minimum capacity we reserve in the Apps container which will be filled
// with extensions and ARC apps, to avoid successive reallocation.
constexpr size_t kMinimumReservedAppsContainerCapacity = 60U;

// Parameters for FuzzyTokenizedStringMatch.
constexpr bool kUseWeightedRatio = false;
constexpr double kRelevanceThreshold = 0.32;

// Default recommended apps in descending order of priority.
constexpr const char* const ranked_default_app_ids[] = {
    web_app::kOsSettingsAppId, web_app::kHelpAppId, arc::kPlayStoreAppId,
    web_app::kCanvasAppId, web_app::kCameraAppId};

// Flag to enable/disable diacritics stripping
constexpr bool kStripDiacritics = true;

// A selection of apps are designated as default recommended apps, and these are
// ranked in a priority order. Determine the rank of the app corresponding to
// |app_id|.
//
// Returns:
//    The priority rank 0, 1, ... if the app is a default app.
//    -1 if the app is not a default app.
int GetDefaultAppRank(const std::string app_id) {
  for (size_t i = 0; i < std::size(ranked_default_app_ids); ++i) {
    if (app_id == ranked_default_app_ids[i])
      return i;
  }
  return -1;
}

// Adds |app_result| to |results| only in case no duplicate apps were already
// added. Duplicate means the same app but for different domain, Chrome and
// Android.
void MaybeAddResult(SearchProvider::Results* results,
                    std::unique_ptr<AppResult> app_result,
                    std::set<std::string>* seen_or_filtered_apps) {
  if (seen_or_filtered_apps->count(app_result->app_id()))
    return;

  seen_or_filtered_apps->insert(app_result->app_id());

  std::unordered_set<std::string> duplicate_app_ids;
  if (!extensions::util::GetEquivalentInstalledArcApps(
          app_result->profile(), app_result->app_id(), &duplicate_app_ids)) {
    results->emplace_back(std::move(app_result));
    return;
  }

  for (const auto& duplicate_app_id : duplicate_app_ids) {
    if (seen_or_filtered_apps->count(duplicate_app_id))
      return;
  }

  results->emplace_back(std::move(app_result));

  // Add duplicate ids in order to filter them if they appear down the
  // list.
  seen_or_filtered_apps->insert(duplicate_app_ids.begin(),
                                duplicate_app_ids.end());
}

// Linearly maps |score| to the range [min, max].
// |score| is assumed to be within [0.0, 1.0]; if it's greater than 1.0
// then max is returned; if it's less than 0.0, then min is returned.
double ReRange(const double score, const double min, const double max) {
  if (score >= 1.0)
    return max;
  if (score <= 0.0)
    return min;

  return min + score * (max - min);
}

// Checks if current locale is non Latin locales.
bool IsNonLatinLocale(base::StringPiece locale) {
  // A set of of non Latin locales. This set is used to select appropriate
  // algorithm for app search.
  static constexpr char kNonLatinLocales[][6] = {
      "am", "ar", "be", "bg", "bn",    "el",    "fa",   "gu", "hi",
      "hy", "iw", "ja", "ka", "kk",    "km",    "kn",   "ko", "ky",
      "lo", "mk", "ml", "mn", "mr",    "my",    "pa",   "ru", "sr",
      "ta", "te", "th", "uk", "zh-CN", "zh-HK", "zh-TW"};
  return base::Contains(kNonLatinLocales, locale);
}

}  // namespace

class AppSearchProvider::App {
 public:
  App(AppSearchProvider::DataSource* data_source,
      const std::string& id,
      const std::string& name,
      const base::Time& last_launch_time,
      const base::Time& install_time,
      bool installed_internally)
      : data_source_(data_source),
        id_(id),
        name_(base::UTF8ToUTF16(name)),
        last_launch_time_(last_launch_time),
        install_time_(install_time),
        installed_internally_(installed_internally) {}

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  ~App() = default;

  struct CompareByLastActivityTimeAndThenAppId {
    bool operator()(const std::unique_ptr<App>& app1,
                    const std::unique_ptr<App>& app2) {
      // Sort decreasing by last activity time, then increasing by App ID.
      base::Time t1 = app1->GetLastActivityTime();
      base::Time t2 = app2->GetLastActivityTime();
      if (t1 != t2)
        return t1 > t2;
      return app1->id_ < app2->id_;
    }
  };

  TokenizedString* GetTokenizedIndexedName() {
    // Tokenizing a string is expensive. Don't pay the price for it at
    // construction of every App, but rather, only when needed (i.e. when the
    // query is not empty and cache the result.
    if (!tokenized_indexed_name_)
      tokenized_indexed_name_ = std::make_unique<TokenizedString>(name_);
    return tokenized_indexed_name_.get();
  }

  base::Time GetLastActivityTime() const {
    if (!last_launch_time_.is_null())
      return last_launch_time_;
    if (!installed_internally_)
      return install_time_;
    return base::Time();
  }

  bool MatchSearchableText(const TokenizedString& query, bool use_exact_match) {
    if (searchable_text_.empty())
      return false;
    if (tokenized_indexed_searchable_text_.empty()) {
      for (const std::u16string& curr_text : searchable_text_) {
        tokenized_indexed_searchable_text_.push_back(
            std::make_unique<TokenizedString>(curr_text));
      }
    }
    if (use_exact_match) {
      TokenizedStringMatch match;
      for (auto& curr_text : tokenized_indexed_searchable_text_) {
        if (match.Calculate(query, *curr_text) > relevance_threshold())
          return true;
      }
    } else {
      FuzzyTokenizedStringMatch match;
      for (auto& curr_text : tokenized_indexed_searchable_text_) {
        if (match.Relevance(query, *curr_text, kUseWeightedRatio,
                            kStripDiacritics) >=
            std::max(kRelevanceThreshold, relevance_threshold())) {
          return true;
        }
      }
    }
    return false;
  }

  AppSearchProvider::DataSource* data_source() { return data_source_; }
  const std::string& id() const { return id_; }
  const std::u16string& name() const { return name_; }
  const base::Time& last_launch_time() const { return last_launch_time_; }
  const base::Time& install_time() const { return install_time_; }

  bool recommendable() const { return recommendable_; }
  void set_recommendable(bool recommendable) { recommendable_ = recommendable; }

  bool searchable() const { return searchable_; }
  void set_searchable(bool searchable) { searchable_ = searchable; }

  const std::vector<std::u16string>& searchable_text() const {
    return searchable_text_;
  }
  void AddSearchableText(const std::u16string& searchable_text) {
    DCHECK(tokenized_indexed_searchable_text_.empty());
    searchable_text_.push_back(searchable_text);
  }

  // Relevance must exceed the threshold to appear as a search result. Exact
  // matches are always surfaced.
  double relevance_threshold() const { return relevance_threshold_; }
  void set_relevance_threshold(double threshold) {
    relevance_threshold_ = threshold;
  }

  bool installed_internally() const { return installed_internally_; }

 private:
  AppSearchProvider::DataSource* data_source_;
  std::unique_ptr<TokenizedString> tokenized_indexed_name_;
  std::vector<std::unique_ptr<TokenizedString>>
      tokenized_indexed_searchable_text_;
  const std::string id_;
  const std::u16string name_;
  const base::Time last_launch_time_;
  const base::Time install_time_;
  bool recommendable_ = true;
  bool searchable_ = true;
  std::vector<std::u16string> searchable_text_;
  double relevance_threshold_ = 0.0;
  // Set to true in case app was installed internally, by sync, policy or as a
  // default app.
  const bool installed_internally_;
};

class AppSearchProvider::DataSource {
 public:
  DataSource(Profile* profile, AppSearchProvider* owner)
      : profile_(profile), owner_(owner) {}

  DataSource(const DataSource&) = delete;
  DataSource& operator=(const DataSource&) = delete;

  virtual ~DataSource() {}

  virtual void AddApps(Apps* apps) = 0;

  virtual std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) = 0;

  virtual void ViewClosing() {}

 protected:
  Profile* profile() { return profile_; }
  AppSearchProvider* owner() { return owner_; }

 private:
  // Unowned pointers.
  Profile* profile_;
  AppSearchProvider* owner_;
};

namespace {

class AppServiceDataSource : public AppSearchProvider::DataSource,
                             public apps::AppRegistryCache::Observer {
 public:
  AppServiceDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner),
        proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)),
        icon_cache_(proxy_,
                    apps::IconCache::GarbageCollectionPolicy::kExplicit) {
    Observe(&proxy_->AppRegistryCache());

    sync_sessions::SessionSyncService* service =
        SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
    if (!service)
      return;
    // base::Unretained() is safe below because the subscription itself is a
    // class member field and handles destruction well.
    foreign_session_updated_subscription_ =
        service->SubscribeToForeignSessionsChanged(base::BindRepeating(
            &AppSearchProvider::RefreshAppsAndUpdateResultsDeferred,
            base::Unretained(owner)));
  }

  AppServiceDataSource(const AppServiceDataSource&) = delete;
  AppServiceDataSource& operator=(const AppServiceDataSource&) = delete;

  ~AppServiceDataSource() override = default;

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps_vector) override {
    proxy_->AppRegistryCache().ForEachApp([this, apps_vector](
                                              const apps::AppUpdate& update) {
      if (!apps_util::IsInstalled(update.Readiness()) ||
          (!update.ShowInSearch().value_or(false) &&
           !(update.Recommendable().value_or(false) &&
             update.AppType() == apps::AppType::kBuiltIn))) {
        return;
      }

      if (!std::strcmp(update.AppId().c_str(),
                       ash::kInternalAppIdContinueReading)) {
        // Don't show continue reading results in the recommended apps.
        return;
      }

      // TODO(crbug.com/826982): add the "can load in incognito" concept to
      // the App Service and use it here, similar to ExtensionDataSource.

      const std::string name = app_list_features::IsCategoricalSearchEnabled()
                                   ? update.Name()
                                   : update.ShortName();
      apps_vector->emplace_back(std::make_unique<AppSearchProvider::App>(
          this, update.AppId(), name, update.LastLaunchTime(),
          update.InstallTime(), update.InstalledInternally()));
      apps_vector->back()->set_recommendable(
          update.Recommendable().value_or(false) &&
          !update.Paused().value_or(false) &&
          update.Readiness() != apps::Readiness::kDisabledByPolicy);
      apps_vector->back()->set_searchable(update.Searchable().value_or(false));

      for (const std::string& term : update.AdditionalSearchTerms()) {
        apps_vector->back()->AddSearchableText(base::UTF8ToUTF16(term));
      }
    });
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) override {
    return std::make_unique<AppServiceAppResult>(
        profile(), app_id, list_controller, is_recommended, &icon_cache_);
  }

 private:
  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override {
    if (!apps_util::IsInstalled(update.Readiness()) ||
        update.IconKeyChanged()) {
      icon_cache_.RemoveIcon(update.AppType(), update.AppId());
    }

    if (update.Readiness() == apps::Readiness::kReady) {
      owner()->RefreshAppsAndUpdateResultsDeferred();
    } else {
      owner()->RefreshAppsAndUpdateResults();
    }
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    Observe(nullptr);
  }

  apps::AppServiceProxy* const proxy_;

  // The AppServiceDataSource seems like one (but not the only) good place to
  // add an App Service icon caching wrapper, because (1) the AppSearchProvider
  // destroys and creates multiple search results in a short period of time,
  // while the user is typing, so will clearly benefit from a cache, and (2)
  // there is an obvious point in time when the cache can be emptied: the user
  // will obviously stop typing (so stop triggering LoadIcon requests) when the
  // search box view closes.
  //
  // There are reasons to have more than one icon caching layer. See the
  // comments for the apps::IconCache::GarbageCollectionPolicy enum.
  apps::IconCache icon_cache_;

  base::CallbackListSubscription foreign_session_updated_subscription_;
};

}  // namespace

AppSearchProvider::AppSearchProvider(Profile* profile,
                                     AppListControllerDelegate* list_controller,
                                     base::Clock* clock,
                                     AppListModelUpdater* model_updater)
    : list_controller_(list_controller),
      model_updater_(model_updater),
      clock_(clock) {
  data_sources_.emplace_back(
      std::make_unique<AppServiceDataSource>(profile, this));
}

AppSearchProvider::~AppSearchProvider() = default;

void AppSearchProvider::Start(const std::u16string& query) {
  ClearResultsSilently();
  query_ = query;
  query_start_time_ = base::TimeTicks::Now();
  // We only need to record app search latency for queries started by user.
  // TODO(crbug.com/1258415): Is this needed?
  record_query_uma_ = true;
  // Refresh list of apps to ensure we have the latest launch time information.
  if (apps_.empty()) {
    RefreshAppsAndUpdateResults();
  } else {
    UpdateResults();
  }
}

void AppSearchProvider::StartZeroState() {
  ClearResultsSilently();
  query_.clear();
  query_start_time_ = base::TimeTicks::Now();
  record_query_uma_ = true;
  RefreshAppsAndUpdateResults();
}

void AppSearchProvider::ViewClosing() {
  // Clear search results asynchronously to keep the search results and prevent
  // the search results, e.g. AppServiceContextMenu, from being deleted when
  // executing their functions.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppSearchProvider::ClearResultsSilently,
                                weak_ptr_factory_.GetWeakPtr()));
  for (auto& data_source : data_sources_)
    data_source->ViewClosing();
}

ash::AppListSearchResultType AppSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kInstalledApp;
}

bool AppSearchProvider::ShouldBlockZeroState() const {
  return true;
}

void AppSearchProvider::RefreshAppsAndUpdateResults() {
  // Clear any pending requests if any.
  refresh_apps_factory_.InvalidateWeakPtrs();

  apps_.clear();
  apps_.reserve(kMinimumReservedAppsContainerCapacity);
  for (auto& data_source : data_sources_)
    data_source->AddApps(&apps_);
  UpdateResults();
}

void AppSearchProvider::RefreshAppsAndUpdateResultsDeferred() {
  // Check if request is pending.
  if (refresh_apps_factory_.HasWeakPtrs())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppSearchProvider::RefreshAppsAndUpdateResults,
                                refresh_apps_factory_.GetWeakPtr()));
}

void AppSearchProvider::UpdateRecommendedResults(
    const base::flat_map<std::string, uint16_t>& id_to_app_list_index) {
  SearchProvider::Results new_results;
  std::set<std::string> seen_or_filtered_tile_apps;
  std::set<std::string> seen_or_filtered_chip_apps;
  const uint16_t apps_size = apps_.size();
  new_results.reserve(apps_size);

  for (auto& app : apps_) {
    // Skip apps which cannot be shown as a suggested app.
    if (!app->recommendable())
      continue;

    std::u16string title = app->name();
    std::unique_ptr<AppResult> result =
        app->data_source()->CreateResult(app->id(), list_controller_, true);
    result->SetTitle(title);

    const int default_rank = GetDefaultAppRank(app->id());
    const auto find_in_app_list = id_to_app_list_index.find(app->id());
    const base::Time time = app->GetLastActivityTime();

    // Set app->relevance based on the following criteria. Scores are set within
    // the range [0, 0.66], allowing the SearchResultRanker some headroom to set
    // higher rankings without having to re-range these scores.
    if (!time.is_null()) {
      // Case 1: if it has last activity time or install time, set the relevance
      // in [0.34, 0.66] based on the time.
      result->UpdateFromLastLaunchedOrInstalledTime(clock_->Now(), time);
      result->set_relevance(ReRange(result->relevance(), 0.34, 0.66));
    } else if (default_rank != -1) {
      // Case 2: if it's a default recommended app, set the relevance in (0.33,
      // 0.34) based on a hard-coded ordering.
      const double relevance = 0.34 - (kEps * (default_rank + 1));
      DCHECK(0.33 < relevance && relevance < 0.34);
      result->set_relevance(relevance);
    } else if (find_in_app_list != id_to_app_list_index.end()) {
      // Case 3: if it's in the app_list_index, set the relevance in [0.1, 0.33]
      result->set_relevance(
          ReRange(1.0 / (1.0 + find_in_app_list->second), 0.1, 0.33));
    } else {
      // Case 4: otherwise set the relevance as 0.0;
      result->set_relevance(0.0);
    }

    if (ash::features::IsProductivityLauncherEnabled()) {
      // For ProductivityLauncher, zero-state suggestions for apps are displayed
      // in a separate "recent apps" section.
      result->SetDisplayType(ChromeSearchResult::DisplayType::kRecentApps);
    } else {
      // In the old launcher, create a second result to the display in the
      // launcher chips, that is otherwise identical to |result|.
      //
      // TODO(crbug.com/1258415): This can be removed once the productivity
      // launcher is launched.
      std::unique_ptr<AppResult> chip_result =
          app->data_source()->CreateResult(app->id(), list_controller_, true);
      chip_result->SetMetadata(result->CloneMetadata());
      chip_result->SetDisplayType(ChromeSearchResult::DisplayType::kChip);
      chip_result->set_relevance(result->relevance());
      MaybeAddResult(&new_results, std::move(chip_result),
                     &seen_or_filtered_chip_apps);
    }

    MaybeAddResult(&new_results, std::move(result),
                   &seen_or_filtered_tile_apps);
  }
  PublishQueriedResultsOrRecommendation(false, &new_results);
}

void AppSearchProvider::UpdateQueriedResults() {
  SearchProvider::Results new_results;
  std::set<std::string> seen_or_filtered_apps;
  const size_t apps_size = apps_.size();
  new_results.reserve(apps_size);

  const TokenizedString query_terms(query_);
  const bool use_exact_match =
      app_list_features::IsExactMatchForNonLatinLocaleEnabled() &&
      IsNonLatinLocale(base::i18n::GetConfiguredLocale());

  for (auto& app : apps_) {
    if (!app->searchable())
      continue;

    TokenizedString* indexed_name = app->GetTokenizedIndexedName();
    if (use_exact_match) {
      TokenizedStringMatch match;
      double relevance = match.Calculate(query_terms, *indexed_name);

      // N.B. Exact matches should be shown even if the threshold isn't reached,
      // e.g. due to a localized name being particularly short.
      const bool keep = relevance > app->relevance_threshold() ||
                        app->MatchSearchableText(query_terms, use_exact_match);

      if (!keep)
        continue;

      std::unique_ptr<AppResult> result =
          app->data_source()->CreateResult(app->id(), list_controller_, false);

      // Update result from match.
      result->SetTitle(indexed_name->text());
      result->SetTitleTags(CalculateTags(query_, indexed_name->text()));
      result->set_relevance(relevance);

      MaybeAddResult(&new_results, std::move(result), &seen_or_filtered_apps);
    } else {
      FuzzyTokenizedStringMatch match;
      const double relevance = match.Relevance(
          query_terms, *indexed_name, kUseWeightedRatio, kStripDiacritics);
      if (relevance >= kRelevanceThreshold ||
          app->MatchSearchableText(query_terms, use_exact_match)) {
        std::unique_ptr<AppResult> result = app->data_source()->CreateResult(
            app->id(), list_controller_, false);

        // Update result from match.
        result->SetTitle(indexed_name->text());
        result->SetTitleTags(CalculateTags(query_, indexed_name->text()));
        result->set_relevance(relevance);

        MaybeAddResult(&new_results, std::move(result), &seen_or_filtered_apps);
      }
    }
  }
  PublishQueriedResultsOrRecommendation(true, &new_results);
}

void AppSearchProvider::PublishQueriedResultsOrRecommendation(
    bool is_queried_search,
    Results* new_results) {
  MaybeRecordQueryLatencyHistogram(is_queried_search);
  SwapResults(new_results);
  update_results_factory_.InvalidateWeakPtrs();
}

void AppSearchProvider::MaybeRecordQueryLatencyHistogram(
    bool is_queried_search) {
  // Record the query latency only if search provider is queried by user
  // initiating a search or getting zero state suggestions.
  if (!record_query_uma_)
    return;

  if (is_queried_search) {
    UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.QueryTime",
                        base::TimeTicks::Now() - query_start_time_);
  } else {
    UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.ZeroStateLatency",
                        base::TimeTicks::Now() - query_start_time_);
  }
  record_query_uma_ = false;
}

void AppSearchProvider::UpdateResults() {
  const bool show_recommendations = query_.empty();

  // Presort app based on last activity time in order to be able to remove
  // duplicates from results. We break ties by App ID, which is arbitrary, but
  // deterministic.
  std::sort(apps_.begin(), apps_.end(),
            App::CompareByLastActivityTimeAndThenAppId());

  if (show_recommendations) {
    // Get the map of app ids to their position in the app list, and then
    // update results.
    model_updater_->GetIdToAppListIndexMap(
        base::BindOnce(&AppSearchProvider::UpdateRecommendedResults,
                       update_results_factory_.GetWeakPtr()));
  } else {
    UpdateQueriedResults();
  }
}

}  // namespace app_list
