// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_app_result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/common/url_icon_source.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/grit/extensions_browser_resources.h"

namespace {
// Seconds in between refreshes;
constexpr base::TimeDelta kRefresh = base::TimeDelta::FromMinutes(30);

constexpr char kAppListLatency[] = "Apps.AppListRecommendedResponse.Latency";
constexpr char kAppListCounts[] = "Apps.AppListRecommendedResponse.Count";
constexpr char kAppListImpressionsBeforeOpen[] =
    "Apps.AppListRecommendedImpResultCountAfterOpen";

// If uninstalled in this time, do not recommend.
constexpr base::FeatureParam<int> kUninstallGrace(
    &app_list_features::kEnableAppReinstallZeroState,
    "uninstall_time_hours",
    24 * 90);

// If install start triggered within this many days, do not recommend.
constexpr base::FeatureParam<int> kInstallStartGrace(
    &app_list_features::kEnableAppReinstallZeroState,
    "install_start_hours",
    24);

// If install impression older than this age, reset impressions.
constexpr base::FeatureParam<int> kResetImpressionGrace(
    &app_list_features::kEnableAppReinstallZeroState,
    "reset_impression_hours",
    30 * 24);

// Count an impression as new if it's more than this much away from the
// previous.
constexpr base::FeatureParam<int> kNewImpressionTime(
    &app_list_features::kEnableAppReinstallZeroState,
    "new_impression_seconds",
    30);

// Maximum number of impressions to show an item.
constexpr base::FeatureParam<int> kImpressionLimit(
    &app_list_features::kEnableAppReinstallZeroState,
    "impression_count_limit",
    5);

// If a user has meaningfully interacted with this feature within this grace
// period, do not show anything. If set to 0, ignored.
constexpr base::FeatureParam<int> kInteractionGrace(
    &app_list_features::kEnableAppReinstallZeroState,
    "interaction_grace_hours",
    0);

void SetStateInt64(Profile* profile,
                   const std::string& package_name,
                   const std::string& key,
                   const int64_t value) {
  const std::string int64_str = base::NumberToString(value);
  DictionaryPrefUpdate update(
      profile->GetPrefs(), app_list::ArcAppReinstallSearchProvider::kAppState);
  base::DictionaryValue* const dictionary = update.Get();
  base::Value* package_item =
      dictionary->FindKeyOfType(package_name, base::Value::Type::DICTIONARY);
  if (!package_item) {
    package_item = dictionary->SetKey(
        package_name, base::Value(base::Value::Type::DICTIONARY));
  }

  package_item->SetKey(key, base::Value(int64_str));
}

void UpdateStateRemoveKey(Profile* profile,
                          const std::string& package_name,
                          const std::string& key) {
  DictionaryPrefUpdate update(
      profile->GetPrefs(), app_list::ArcAppReinstallSearchProvider::kAppState);
  base::DictionaryValue* const dictionary = update.Get();
  base::Value* package_item =
      dictionary->FindKeyOfType(package_name, base::Value::Type::DICTIONARY);
  if (!package_item) {
    return;
  }
  package_item->RemoveKey(key);
}

void UpdateStateTime(Profile* profile,
                     const std::string& package_name,
                     const std::string& key) {
  const int64_t timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
  SetStateInt64(profile, package_name, key, timestamp);
}

bool GetStateInt64(Profile* profile,
                   const std::string& package_name,
                   const std::string& key,
                   int64_t* value) {
  const base::DictionaryValue* dictionary = profile->GetPrefs()->GetDictionary(
      app_list::ArcAppReinstallSearchProvider::kAppState);
  if (!dictionary)
    return false;
  const base::Value* package_item =
      dictionary->FindKeyOfType(package_name, base::Value::Type::DICTIONARY);
  if (!package_item)
    return false;
  const std::string* value_str = package_item->FindStringKey(key);
  if (!value_str)
    return false;

  if (!base::StringToInt64(*value_str, value)) {
    LOG(ERROR) << "Failed conversion " << *value_str;
    return false;
  }

  return true;
}

bool GetStateTime(Profile* profile,
                  const std::string& package_name,
                  const std::string& key,
                  base::TimeDelta* time_delta) {
  int64_t value;
  if (!GetStateInt64(profile, package_name, key, &value))
    return false;

  *time_delta = base::TimeDelta::FromMilliseconds(value);
  return true;
}

bool GetKnownPackageNames(Profile* profile,
                          std::unordered_set<std::string>* package_names) {
  const base::DictionaryValue* dictionary = profile->GetPrefs()->GetDictionary(
      app_list::ArcAppReinstallSearchProvider::kAppState);
  for (const auto& it : dictionary->DictItems()) {
    if (it.second.is_dict()) {
      package_names->insert(it.first);
    }
  }
  return true;
}

void RecordUmaResponseParseResult(arc::mojom::AppReinstallState result) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListRecommendedResponse", result);
}

// Limits icon size to be downloaded with FIFE. The input |icon_dimension| is in
// dip and the FIFE requires pixel value. Thus, we need to multiply
// |icon_dimension| with the maximum device scale factor to avoid potential
// issues.
std::string LimitIconSizeWithFife(const std::string& icon_url,
                                  int icon_dimension) {
  DCHECK_GT(icon_dimension, 0);
  // Maximum device scale factor (DSF).
  static const int kMaxDeviceScaleFactor = 3;
  // We append a suffix to icon url
  return base::StrCat(
      {icon_url, "=s",
       base::NumberToString(icon_dimension * kMaxDeviceScaleFactor)});
}

}  // namespace

namespace app_list {

// static
constexpr char ArcAppReinstallSearchProvider::kInstallTime[];

// static
constexpr char ArcAppReinstallSearchProvider::kAppState[];

// static
constexpr char ArcAppReinstallSearchProvider::kImpressionCount[];

// static
constexpr char ArcAppReinstallSearchProvider::kImpressionTime[];

// static
constexpr char ArcAppReinstallSearchProvider::kUninstallTime[];

// static
constexpr char ArcAppReinstallSearchProvider::kOpenTime[];

// static
constexpr char ArcAppReinstallSearchProvider::kInstallStartTime[];

ArcAppReinstallSearchProvider::ArcAppReinstallSearchProvider(
    Profile* profile,
    unsigned int max_result_count)
    : profile_(profile),
      max_result_count_(max_result_count),
      icon_dimension_(ash::AppListConfig::instance().GetPreferredIconDimension(
          ash::SearchResultDisplayType::kTile)),
      app_fetch_timer_(std::make_unique<base::RepeatingTimer>()) {
  DCHECK(profile_);
  ArcAppListPrefs::Get(profile_)->AddObserver(this);
  MaybeUpdateFetching();
}

ArcAppReinstallSearchProvider::~ArcAppReinstallSearchProvider() {
  ArcAppListPrefs::Get(profile_)->RemoveObserver(this);
}

void ArcAppReinstallSearchProvider::BeginRepeatingFetch() {
  // If already running, do not re-start.
  if (app_fetch_timer_->IsRunning())
    return;

  app_fetch_timer_->Start(FROM_HERE, kRefresh, this,
                          &ArcAppReinstallSearchProvider::StartFetch);
  StartFetch();
}

void ArcAppReinstallSearchProvider::StopRepeatingFetch() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  app_fetch_timer_->AbandonAndStop();
  loaded_value_.clear();
  icon_urls_.clear();
  loading_icon_urls_.clear();
  UpdateResults();
}

ash::AppListSearchResultType ArcAppReinstallSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kPlayStoreReinstallApp;
}

void ArcAppReinstallSearchProvider::Start(const base::string16& query) {
  query_is_empty_ = query.empty();
  if (!query_is_empty_) {
    ClearResults();
    return;
  }

  // Always check if suggested content is enabled before searching for
  // reinstall recommendations.
  bool should_show_arc_app_reinstall_result = true;
  PrefService* pref_service = profile_->GetPrefs();
  if (pref_service &&
      !pref_service->GetBoolean(chromeos::prefs::kSuggestedContentEnabled))
    should_show_arc_app_reinstall_result = false;
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kSuggestedContentToggle))
    should_show_arc_app_reinstall_result = false;

  if (!should_show_arc_app_reinstall_result) {
    ClearResults();
    return;
  }

  UpdateResults();
}

void ArcAppReinstallSearchProvider::StartFetch() {
  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppReinstallCandidates)
          : nullptr;
  if (app_instance == nullptr)
    return;

  if (profile_->GetPrefs()->IsManagedPreference(
          prefs::kAppReinstallRecommendationEnabled) &&
      !profile_->GetPrefs()->GetBoolean(
          prefs::kAppReinstallRecommendationEnabled)) {
    // This user profile is managed, and the app reinstall recommendation is
    // switched off. This is updated dynamically, usually, so we need to update
    // the loaded value and return.
    OnGetAppReinstallCandidates(base::Time::UnixEpoch(),
                                arc::mojom::AppReinstallState::REQUEST_SUCCESS,
                                {});
    return;
  }

  app_instance->GetAppReinstallCandidates(base::BindOnce(
      &ArcAppReinstallSearchProvider::OnGetAppReinstallCandidates,
      weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
}

void ArcAppReinstallSearchProvider::OnGetAppReinstallCandidates(
    base::Time start_time,
    arc::mojom::AppReinstallState state,
    std::vector<arc::mojom::AppReinstallCandidatePtr> results) {
  RecordUmaResponseParseResult(state);

  // fake result insertion is indicated by unix epoch start time.
  if (start_time != base::Time::UnixEpoch()) {
    UMA_HISTOGRAM_TIMES(kAppListLatency, base::Time::Now() - start_time);
    UMA_HISTOGRAM_COUNTS_100(kAppListCounts, results.size());
  }
  if (state != arc::mojom::AppReinstallState::REQUEST_SUCCESS) {
    LOG(ERROR) << "Failed to get reinstall candidates: " << state;
    return;
  }
  loaded_value_.clear();

  for (const auto& candidate : results) {
    // only keep candidates with icons.
    if (candidate->icon_url != base::nullopt) {
      loaded_value_.push_back(candidate.Clone());
    }
  }

  // Update the dictionary to reset old impression counts.
  const base::TimeDelta now = base::Time::Now().ToDeltaSinceWindowsEpoch();
  // Remove stale impressions from state.
  std::unordered_set<std::string> package_names;
  GetKnownPackageNames(profile_, &package_names);
  for (const std::string& package_name : package_names) {
    base::TimeDelta latest_impression;
    if (!GetStateTime(profile_, package_name, kImpressionTime,
                      &latest_impression)) {
      continue;
    }
    if (now - latest_impression >
        base::TimeDelta::FromHours(kResetImpressionGrace.Get())) {
      SetStateInt64(profile_, package_name, kImpressionCount, 0);
      UpdateStateRemoveKey(profile_, package_name, kImpressionTime);
    }
  }

  UpdateResults();
}

void ArcAppReinstallSearchProvider::UpdateResults() {
  // We clear results if there are none from the server, or the user has entered
  // a non-zero query.
  if (loaded_value_.empty() || !query_is_empty_) {
    if (loaded_value_.empty())
      icon_urls_.clear();
    ClearResults();
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(prefs);

  std::vector<std::unique_ptr<ChromeSearchResult>> new_results;
  std::unordered_set<std::string> used_icon_urls;

  // Lock over the whole list.
  if (ShouldShowAnything()) {
    for (size_t i = 0, processed = 0;
         i < loaded_value_.size() && processed < max_result_count_; ++i) {
      // Any packages that are installing or installed and not in sync with the
      // server are removed with IsUnknownPackage.
      if (!prefs->IsUnknownPackage(loaded_value_[i]->package_name))
        continue;
      // Should we filter this ?
      if (!ShouldShowPackage(loaded_value_[i]->package_name)) {
        continue;
      }
      processed++;

      // From this point, we believe that this item should be in the result
      // list. We try to find this icon, and if it is not available, we load it.
      const std::string& icon_url = loaded_value_[i]->icon_url.value();
      // All the icons we are showing.
      used_icon_urls.insert(icon_url);

      const auto icon_it = icon_urls_.find(icon_url);
      const auto loading_icon_it = loading_icon_urls_.find(icon_url);
      if (icon_it == icon_urls_.end() &&
          loading_icon_it == loading_icon_urls_.end()) {
        // this icon is not loaded, nor is it in the loading set. Add it.
        loading_icon_urls_[icon_url] = gfx::ImageSkia(
            std::make_unique<UrlIconSource>(
                base::BindRepeating(
                    &ArcAppReinstallSearchProvider::OnIconLoaded,
                    weak_ptr_factory_.GetWeakPtr(), icon_url),
                profile_,
                GURL(LimitIconSizeWithFife(icon_url, icon_dimension_)),
                icon_dimension_, IDR_APP_DEFAULT_ICON),
            gfx::Size(icon_dimension_, icon_dimension_));
        loading_icon_urls_[icon_url].GetRepresentation(1.0f);
      } else if (icon_it != icon_urls_.end()) {
        // Icon is loaded, add it to the results.
        new_results.emplace_back(std::make_unique<ArcAppReinstallAppResult>(
            loaded_value_[i], icon_it->second, this));
      }
    }
  }

  // Remove unused icons.
  std::unordered_set<std::string> unused_icon_urls;
  for (const auto& it : icon_urls_) {
    if (used_icon_urls.find(it.first) == used_icon_urls.end()) {
      // This url is used, remove.
      unused_icon_urls.insert(it.first);
    }
  }

  for (const std::string& url : unused_icon_urls) {
    icon_urls_.erase(url);
    loading_icon_urls_.erase(url);
  }

  // Now we are ready with new_results. do we actually need to replace things on
  // screen?
  if (!ResultsIdentical(results(), new_results)) {
    SwapResults(&new_results);
  }
}

void ArcAppReinstallSearchProvider::MaybeUpdateFetching() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(arc::kPlayStoreAppId);
  if (app_info && app_info->ready)
    BeginRepeatingFetch();
  else
    StopRepeatingFetch();
}

void ArcAppReinstallSearchProvider::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  OnAppStatesChanged(app_id, app_info);
}

void ArcAppReinstallSearchProvider::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == arc::kPlayStoreAppId)
    MaybeUpdateFetching();
}

void ArcAppReinstallSearchProvider::OnAppRemoved(const std::string& app_id) {
  if (app_id == arc::kPlayStoreAppId)
    MaybeUpdateFetching();
}

void ArcAppReinstallSearchProvider::OnInstallationStarted(
    const std::string& package_name) {
  UpdateStateTime(profile_, package_name, kInstallStartTime);
  UpdateResults();
}

void ArcAppReinstallSearchProvider::OnInstallationFinished(
    const std::string& package_name,
    bool success) {
  if (success) {
    UpdateStateTime(profile_, package_name, kInstallTime);
  }
  UpdateResults();
}

void ArcAppReinstallSearchProvider::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  UpdateResults();
}

void ArcAppReinstallSearchProvider::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  // If we uninstalled this, update the timestamp before updating results.
  // Otherwise, it's just an app no longer available.
  if (uninstalled) {
    UpdateStateTime(profile_, package_name, kUninstallTime);
  }
  UpdateResults();
}

void ArcAppReinstallSearchProvider::SetTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  app_fetch_timer_ = std::move(timer);
}

void ArcAppReinstallSearchProvider::OnOpened(const std::string& package_name) {
  UpdateStateTime(profile_, package_name, kOpenTime);
  int64_t impression_count;
  if (GetStateInt64(profile_, package_name, kImpressionCount,
                    &impression_count)) {
    UMA_HISTOGRAM_COUNTS_100(kAppListImpressionsBeforeOpen, impression_count);
  }
  UpdateResults();
}

void ArcAppReinstallSearchProvider::OnVisibilityChanged(
    const std::string& package_name,
    bool visibility) {
  if (!visibility) {
    // do not update state when showing, update when we hide.
    return;
  }

  // If never shown before, or shown more than |kNewImpressionTime| ago,
  // increment the count here.
  const base::TimeDelta now = base::Time::Now().ToDeltaSinceWindowsEpoch();
  base::TimeDelta latest_impression;
  int64_t impression_count;
  if (!GetStateInt64(profile_, package_name, kImpressionCount,
                     &impression_count)) {
    impression_count = 0;
  }
  UMA_HISTOGRAM_COUNTS_100("Arc.AppListRecommendedImp.AllImpression", 1);
  // Get impression count and time. If neither is set, set them.
  // If they're set, update if appropriate.
  if (!GetStateTime(profile_, package_name, kImpressionTime,
                    &latest_impression) ||
      impression_count == 0 ||
      (now - latest_impression >
       base::TimeDelta::FromSeconds(kNewImpressionTime.Get()))) {
    UpdateStateTime(profile_, package_name, kImpressionTime);
    SetStateInt64(profile_, package_name, kImpressionCount,
                  impression_count + 1);
    UMA_HISTOGRAM_COUNTS_100("Arc.AppListRecommendedImp.CountedImpression", 1);
    UpdateResults();
  }
}

void ArcAppReinstallSearchProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kAppState);
}

// For icon load callback, in OnGetAppReinstallCandidates
void ArcAppReinstallSearchProvider::OnIconLoaded(const std::string& icon_url) {
  auto skia_ptr = loading_icon_urls_.find(icon_url);
  DCHECK(skia_ptr != loading_icon_urls_.end());
  if (skia_ptr == loading_icon_urls_.end()) {
    return;
  }
  const std::vector<gfx::ImageSkiaRep> image_reps =
      skia_ptr->second.image_reps();
  for (const gfx::ImageSkiaRep& rep : image_reps)
    skia_ptr->second.RemoveRepresentation(rep.scale());
  DCHECK_LE(skia_ptr->second.width(), icon_dimension_);

  // ImageSkia is now ready to serve, move to the done list and update the
  // screen.
  icon_urls_[icon_url] = skia_ptr->second;
  loading_icon_urls_.erase(icon_url);
  UpdateResults();
}

bool ArcAppReinstallSearchProvider::ShouldShowPackage(
    const std::string& package_id) const {
  base::TimeDelta timestamp;
  const base::TimeDelta now = base::Time::Now().ToDeltaSinceWindowsEpoch();
  if (GetStateTime(profile_, package_id, kUninstallTime, &timestamp)) {
    const auto delta = now - timestamp;
    if (delta < base::TimeDelta::FromHours(kUninstallGrace.Get())) {
      // We uninstalled this recently, don't show.
      return false;
    }
  }
  if (GetStateTime(profile_, package_id, kInstallStartTime, &timestamp)) {
    const auto delta = now - timestamp;
    if (delta < base::TimeDelta::FromHours(kInstallStartGrace.Get())) {
      // We started install on this recently, don't show.
      return false;
    }
  }
  int64_t value;
  if (GetStateInt64(profile_, package_id, kImpressionCount, &value)) {
    if (value > kImpressionLimit.Get()) {
      // Shown too many times, ignore.
      return false;
    }
  }
  return true;
}

bool ArcAppReinstallSearchProvider::ShouldShowAnything() const {
  if (!kInteractionGrace.Get()) {
    return true;
  }
  const base::TimeDelta grace_period =
      base::TimeDelta::FromHours(kInteractionGrace.Get());
  const base::TimeDelta now = base::Time::Now().ToDeltaSinceWindowsEpoch();
  std::unordered_set<std::string> package_names;
  GetKnownPackageNames(profile_, &package_names);

  for (const std::string& package_name : package_names) {
    base::TimeDelta install_time;
    if (GetStateTime(profile_, package_name, kInstallTime, &install_time)) {
      if (now - install_time < grace_period) {
        // installed in grace, do not show anything.
        return false;
      }
    }

    base::TimeDelta result_open;
    if (GetStateTime(profile_, package_name, kOpenTime, &result_open)) {
      if (now - result_open < grace_period) {
        // Shown in grace, do not show anything.
        return false;
      }
    }

    int64_t impression_count;
    if (GetStateInt64(profile_, package_name, kImpressionCount,
                      &impression_count)) {
      if (impression_count >= kImpressionLimit.Get()) {
        base::TimeDelta impression_time;
        if (GetStateTime(profile_, package_name, kImpressionTime,
                         &impression_time)) {
          if (now - impression_time < grace_period) {
            // We showed a too-many-shown result recently, within grace, don't
            // show anything.
            return false;
          }
        }
      }
    }
  }
  return true;
}

bool ArcAppReinstallSearchProvider::ResultsIdentical(
    const std::vector<std::unique_ptr<ChromeSearchResult>>& old_results,
    const std::vector<std::unique_ptr<ChromeSearchResult>>& new_results) {
  if (old_results.size() != new_results.size()) {
    return false;
  }
  for (size_t i = 0; i < old_results.size(); ++i) {
    const ChromeSearchResult& old_result = *(old_results[i]);
    const ChromeSearchResult& new_result = *(new_results[i]);
    if (!old_result.icon().BackedBySameObjectAs(new_result.icon())) {
      return false;
    }
    if (old_result.title() != new_result.title()) {
      return false;
    }
    if (old_result.id() != new_result.id()) {
      return false;
    }
    if (old_result.relevance() != new_result.relevance()) {
      return false;
    }
    if (old_result.rating() != new_result.rating()) {
      return false;
    }
  }
  return true;
}

}  // namespace app_list
