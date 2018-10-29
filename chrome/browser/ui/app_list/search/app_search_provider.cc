// Copyright 2013 The Chromium Authors. All rights reserved.
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

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/app_list/tokenized_string.h"
#include "ash/public/cpp/app_list/tokenized_string_match.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/extension_app_utils.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/search/arc_app_result.h"
#include "chrome/browser/ui/app_list/search/crostini_app_result.h"
#include "chrome/browser/ui/app_list/search/extension_app_result.h"
#include "chrome/browser/ui/app_list/search/internal_app_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExtensionRegistry;

namespace {

// The minimum capacity we reserve in the Apps container which will be filled
// with extensions and ARC apps, to avoid successive reallocation.
constexpr size_t kMinimumReservedAppsContainerCapacity = 60U;

// Adds |app_result| to |results| only in case no duplicate apps were already
// added. Duplicate means the same app but for different domain, Chrome and
// Android.
void MaybeAddResult(app_list::SearchProvider::Results* results,
                    std::unique_ptr<app_list::AppResult> app_result,
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
float ReRange(const float score, const float min, const float max) {
  if (score >= 1.0f)
    return max;
  if (score <= 0.0f)
    return min;

  return min + score * (max - min);
}

}  // namespace

namespace app_list {

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
  ~App() = default;

  struct CompareByLastActivityTime {
    bool operator()(const std::unique_ptr<App>& app1,
                    const std::unique_ptr<App>& app2) {
      return app1->GetLastActivityTime() > app2->GetLastActivityTime();
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

  bool MatchSearchableText(const TokenizedString& query) {
    if (searchable_text_.empty())
      return false;
    if (!tokenized_indexed_searchable_text_) {
      tokenized_indexed_searchable_text_ =
          std::make_unique<TokenizedString>(searchable_text_);
    }
    return TokenizedStringMatch().Calculate(
        query, *tokenized_indexed_searchable_text_);
  }

  AppSearchProvider::DataSource* data_source() { return data_source_; }
  const std::string& id() const { return id_; }
  const base::string16& name() const { return name_; }
  const base::Time& last_launch_time() const { return last_launch_time_; }
  const base::Time& install_time() const { return install_time_; }

  bool recommendable() const { return recommendable_; }
  void set_recommendable(bool recommendable) { recommendable_ = recommendable; }

  bool searchable() const { return searchable_; }
  void set_searchable(bool searchable) { searchable_ = searchable; }

  const base::string16& searchable_text() const { return searchable_text_; }
  void set_searchable_text(const base::string16& searchable_text) {
    searchable_text_ = searchable_text;
  }

  bool require_exact_match() const { return require_exact_match_; }
  void set_require_exact_match(bool require_exact_match) {
    require_exact_match_ = require_exact_match;
  }

  bool installed_internally() const { return installed_internally_; }

 private:
  AppSearchProvider::DataSource* data_source_;
  std::unique_ptr<TokenizedString> tokenized_indexed_name_;
  std::unique_ptr<TokenizedString> tokenized_indexed_searchable_text_;
  const std::string id_;
  const base::string16 name_;
  const base::Time last_launch_time_;
  const base::Time install_time_;
  bool recommendable_ = true;
  bool searchable_ = true;
  base::string16 searchable_text_;
  bool require_exact_match_ = false;
  // Set to true in case app was installed internally, by sync, policy or as a
  // default app.
  const bool installed_internally_;

  DISALLOW_COPY_AND_ASSIGN(App);
};

class AppSearchProvider::DataSource {
 public:
  DataSource(Profile* profile, AppSearchProvider* owner)
      : profile_(profile),
        owner_(owner) {}
  virtual ~DataSource() {}

  virtual void AddApps(Apps* apps) = 0;

  virtual std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) = 0;

 protected:
  Profile* profile() { return profile_; }
  AppSearchProvider* owner() { return owner_; }

 private:
  // Unowned pointers.
  Profile* profile_;
  AppSearchProvider* owner_;

  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

namespace {

class ExtensionDataSource : public AppSearchProvider::DataSource,
                            public extensions::ExtensionRegistryObserver {
 public:
  ExtensionDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner),
        extension_registry_observer_(this) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
  }
  ~ExtensionDataSource() override {}

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps) override {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    AddApps(apps, registry->enabled_extensions());
    AddApps(apps, registry->disabled_extensions());
    AddApps(apps, registry->terminated_extensions());
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) override {
    return std::make_unique<ExtensionAppResult>(
        profile(), app_id, list_controller, is_recommended);
  }

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    owner()->RefreshAppsAndUpdateResults(true);
  }

 private:
  void AddApps(AppSearchProvider::Apps* apps,
               const extensions::ExtensionSet& extensions) {
    extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(
        profile());
    for (const auto& it : extensions) {
      const extensions::Extension* extension = it.get();

      if (!app_list::ShouldShowInLauncher(extension, profile())) {
        continue;
      }

      if (profile()->IsOffTheRecord() &&
          !extensions::util::CanLoadInIncognito(extension, profile())) {
        continue;
      }

      apps->emplace_back(std::make_unique<AppSearchProvider::App>(
          this, extension->id(), extension->short_name(),
          prefs->GetLastLaunchTime(extension->id()),
          prefs->GetInstallTime(extension->id()),
          extension->was_installed_by_default() ||
              extension->was_installed_by_oem() ||
              extensions::Manifest::IsComponentLocation(
                  extension->location()) ||
              extensions::Manifest::IsPolicyLocation(extension->location())));
    }
  }

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDataSource);
};

class ArcDataSource : public AppSearchProvider::DataSource,
                      public ArcAppListPrefs::Observer {
 public:
  ArcDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner) {
    ArcAppListPrefs::Get(profile)->AddObserver(this);
  }

  ~ArcDataSource() override {
    ArcAppListPrefs::Get(profile())->RemoveObserver(this);
  }

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps) override {
    ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
    CHECK(arc_prefs);

    const std::vector<std::string> app_ids = arc_prefs->GetAppIds();
    for (const auto& app_id : app_ids) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          arc_prefs->GetApp(app_id);
      if (!app_info) {
        NOTREACHED();
        continue;
      }

      if (!app_info->show_in_launcher)
        continue;

      apps->emplace_back(std::make_unique<AppSearchProvider::App>(
          this, app_id, app_info->name, app_info->last_launch_time,
          app_info->install_time,
          arc_prefs->IsDefault(app_id) ||
              arc_prefs->IsControlledByPolicy(app_info->package_name)));
    }
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) override {
    return std::make_unique<ArcAppResult>(profile(), app_id, list_controller,
                                          is_recommended);
  }

  // ArcAppListPrefs::Observer overrides:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

  void OnAppRemoved(const std::string& id) override {
    owner()->RefreshAppsAndUpdateResults(true);
  }

  void OnAppNameUpdated(const std::string& id,
                        const std::string& name) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDataSource);
};

class InternalDataSource : public AppSearchProvider::DataSource,
                           syncer::SyncServiceObserver {
 public:
  InternalDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner) {
    browser_sync::ProfileSyncService* service =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
    if (service)
      service->AddObserver(this);
  }

  ~InternalDataSource() override {
    browser_sync::ProfileSyncService* service =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile());
    if (service)
      service->RemoveObserver(this);
  }

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps) override {
    for (const auto& internal_app : GetInternalAppList(profile())) {
      if (!std::strcmp(internal_app.app_id, kInternalAppIdContinueReading)) {
        if (!app_list_features::IsContinueReadingEnabled())
          continue;

        auto* service =
            ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile());
        if (!service ||
            !service->GetActiveDataTypes().Has(syncer::PROXY_TABS)) {
          continue;
        }
      }

      apps->emplace_back(std::make_unique<AppSearchProvider::App>(
          this, internal_app.app_id,
          l10n_util::GetStringUTF8(internal_app.name_string_resource_id),
          base::Time() /* last_launch_time */, base::Time() /* install_time */,
          true /* installed_internally */));
      apps->back()->set_recommendable(internal_app.recommendable);
      apps->back()->set_searchable(internal_app.searchable);
      if (internal_app.searchable_string_resource_id != 0) {
        apps->back()->set_searchable_text(l10n_util::GetStringUTF16(
            internal_app.searchable_string_resource_id));
      }
    }
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) override {
    return std::make_unique<InternalAppResult>(profile(), app_id,
                                               list_controller, is_recommended);
  }

  // syncer::SyncServiceObserver overrides:
  void OnForeignSessionUpdated(syncer::SyncService* sync) override {
    owner()->RefreshAppsAndUpdateResults(/*force_inline=*/false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalDataSource);
};

class CrostiniDataSource : public AppSearchProvider::DataSource,
                           public crostini::CrostiniRegistryService::Observer {
 public:
  CrostiniDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner) {
    crostini::CrostiniRegistryServiceFactory::GetForProfile(profile)
        ->AddObserver(this);
  }

  ~CrostiniDataSource() override {
    crostini::CrostiniRegistryServiceFactory::GetForProfile(profile())
        ->RemoveObserver(this);
  }

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps) override {
    crostini::CrostiniRegistryService* registry_service =
        crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
    for (const std::string& app_id : registry_service->GetRegisteredAppIds()) {
      crostini::CrostiniRegistryService::Registration registration =
          *registry_service->GetRegistration(app_id);
      if (registration.NoDisplay())
        continue;
      // Eventually it would be nice to use additional data points, for example
      // the 'Keywords' desktop entry field and the executable file name.
      apps->emplace_back(std::make_unique<AppSearchProvider::App>(
          this, app_id, registration.Name(), registration.LastLaunchTime(),
          registration.InstallTime(), false /* installed_internally */));

      // Until it's been installed, the Terminal is hidden unless you search
      // for 'Terminal' exactly (case insensitive).
      if (app_id == crostini::kCrostiniTerminalId &&
          !crostini::IsCrostiniEnabled(profile())) {
        apps->back()->set_recommendable(false);
        apps->back()->set_require_exact_match(true);
      }
    }
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      bool is_recommended) override {
    return std::make_unique<CrostiniAppResult>(profile(), app_id,
                                               list_controller, is_recommended);
  }

  // crostini::CrostiniRegistryService::Observer overrides:
  void OnRegistryUpdated(
      crostini::CrostiniRegistryService* registry_service,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override {
    owner()->RefreshAppsAndUpdateResults(!removed_apps.empty());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniDataSource);
};

}  // namespace

AppSearchProvider::AppSearchProvider(Profile* profile,
                                     AppListControllerDelegate* list_controller,
                                     base::Clock* clock,
                                     AppListModelUpdater* model_updater)
    : profile_(profile),
      list_controller_(list_controller),
      model_updater_(model_updater),
      clock_(clock),
      ranker_(std::make_unique<AppSearchResultRanker>(
          profile->GetPath(),
          chromeos::ProfileHelper::IsEphemeralUserProfile(profile))),
      update_results_factory_(this) {
  data_sources_.emplace_back(
      std::make_unique<ExtensionDataSource>(profile, this));
  if (arc::IsArcAllowedForProfile(profile))
    data_sources_.emplace_back(std::make_unique<ArcDataSource>(profile, this));
  if (crostini::IsCrostiniUIAllowedForProfile(profile)) {
    data_sources_.emplace_back(
        std::make_unique<CrostiniDataSource>(profile, this));
  }
  data_sources_.emplace_back(
      std::make_unique<InternalDataSource>(profile, this));
}

AppSearchProvider::~AppSearchProvider() {}

void AppSearchProvider::Start(const base::string16& query) {
  query_ = query;
  const bool show_recommendations = query.empty();
  // Refresh list of apps to ensure we have the latest launch time information.
  // This will also cause the results to update.
  if (show_recommendations || apps_.empty())
    RefreshApps();

  UpdateResults();
}

void AppSearchProvider::Train(const std::string& id) {
  ranker_->Train(id);
}

void AppSearchProvider::RefreshApps() {
  apps_.clear();
  apps_.reserve(kMinimumReservedAppsContainerCapacity);
  for (auto& data_source : data_sources_)
    data_source->AddApps(&apps_);
}

void AppSearchProvider::UpdateRecommendedResults(
    const base::flat_map<std::string, uint16_t>& id_to_app_list_index) {
  SearchProvider::Results new_results;
  std::set<std::string> seen_or_filtered_apps;
  const uint16_t apps_size = apps_.size();
  new_results.reserve(apps_size);
  const auto& ranker_scores = ranker_->Rank();

  for (auto& app : apps_) {
    // Skip apps which cannot be shown as a suggested app.
    if (!app->recommendable())
      continue;

    base::string16 title = app->name();
    if (app->id() == kInternalAppIdContinueReading) {
      if (HasRecommendableForeignTab(profile_, &title, nullptr))
        app->set_searchable_text(title);
      else
        continue;
    }

    std::unique_ptr<AppResult> result =
        app->data_source()->CreateResult(app->id(), list_controller_, true);
    result->SetTitle(title);

    // Set app->relevance based on the following criteria.
    const auto find_in_ranker = ranker_scores.find(app->id());
    const auto find_in_app_list = id_to_app_list_index.find(app->id());
    const base::Time time = app->GetLastActivityTime();

    if (app->id() == kInternalAppIdContinueReading) {
      // Case 1: if it's |kInternalAppIdContinueReading|, set relevance as 1.0
      // (always show it as the first).
      result->set_relevance(1.0);
    } else if (find_in_ranker != ranker_scores.end()) {
      // Case 2: if it's recommended by |ranker_|, set relevance as a score
      // in [0.67, 0.99].
      result->set_relevance(ReRange(find_in_ranker->second, 0.67, 0.99));
    } else if (!time.is_null()) {
      // Case 3: if it has last activity time or install time, set the relevance
      // in [0.34, 0.66] based on the time.
      result->UpdateFromLastLaunchedOrInstalledTime(clock_->Now(), time);
      result->set_relevance(ReRange(result->relevance(), 0.34, 0.66));
    } else if (find_in_app_list != id_to_app_list_index.end()) {
      // Case 4: if it's in the app_list_index, set the relevance in [0.1, 0.33]
      result->set_relevance(
          ReRange(1.0f / (1.0f + find_in_app_list->second), 0.1, 0.33));
    } else {
      // Case 5: otherwise set the relevance as 0.0f;
      result->set_relevance(0.0f);
    }

    MaybeAddResult(&new_results, std::move(result), &seen_or_filtered_apps);
  }

  SwapResults(&new_results);
  update_results_factory_.InvalidateWeakPtrs();
}

void AppSearchProvider::UpdateQueriedResults() {
  SearchProvider::Results new_results;
  std::set<std::string> seen_or_filtered_apps;
  const size_t apps_size = apps_.size();
  new_results.reserve(apps_size);

  const TokenizedString query_terms(query_);
  for (auto& app : apps_) {
    if (!app->searchable() ||
        (app->require_exact_match() &&
         !base::EqualsCaseInsensitiveASCII(query_, app->name()))) {
      continue;
    }

    TokenizedStringMatch match;
    TokenizedString* indexed_name = app->GetTokenizedIndexedName();
    if (!match.Calculate(query_terms, *indexed_name) &&
        !app->MatchSearchableText(query_terms)) {
      continue;
    }

    std::unique_ptr<AppResult> result =
        app->data_source()->CreateResult(app->id(), list_controller_, false);
    result->UpdateFromMatch(*indexed_name, match);
    MaybeAddResult(&new_results, std::move(result), &seen_or_filtered_apps);
  }

  SwapResults(&new_results);
  update_results_factory_.InvalidateWeakPtrs();
}

void AppSearchProvider::UpdateResults() {
  const bool show_recommendations = query_.empty();

  // Presort app based on last active time in order to be able to remove
  // duplicates from results.
  std::sort(apps_.begin(), apps_.end(), App::CompareByLastActivityTime());

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

void AppSearchProvider::RefreshAppsAndUpdateResults(bool force_inline) {
  RefreshApps();

  if (force_inline) {
    UpdateResults();
  } else {
    if (!update_results_factory_.HasWeakPtrs()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&AppSearchProvider::UpdateResults,
                                update_results_factory_.GetWeakPtr()));
    }
  }
}

}  // namespace app_list
