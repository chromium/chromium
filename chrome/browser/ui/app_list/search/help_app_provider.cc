// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/help_app_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kHelpAppDiscoverResult[] = "help-app://discover";
constexpr char kHelpAppUpdatesResult[] = "help-app://updates";

constexpr size_t kMinQueryLength = 3u;
constexpr double kMinScore = 0.4;
constexpr size_t kNumRequestedResults = 5u;
constexpr size_t kMaxShownResults = 2u;

// Whether we should show the Discover Tab suggestion chip.
bool ShouldShowDiscoverTabSuggestionChip(Profile* profile) {
  if (!base::FeatureList::IsEnabled(ash::features::kHelpAppDiscoverTab)) {
    return false;
  }
  const int times_left_to_show = profile->GetPrefs()->GetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow);
  return times_left_to_show > 0;
}

// Decrements the times left to show the Discover Tab suggestion chip in
// PrefService.
void DecreaseTimesLeftToShowDiscoverTabSuggestionChip(Profile* profile) {
  const int times_left_to_show = profile->GetPrefs()->GetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow);
  profile->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, times_left_to_show - 1);
}

// Sets the times left to show the Discover Tab suggestion chip to 0 in
// PrefService.
void StopShowingDiscoverTabSuggestionChip(Profile* profile) {
  profile->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
}

// The end result of a list search. Logged once per time a list search finishes.
// Not logged if the search is canceled by a new search starting. Not logged for
// suggestion chips. These values persist to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class ListSearchResultState {
  // Search finished with no problems.
  kOk = 0,
  // Search canceled because no help app icon.
  kNoHelpAppIcon = 1,
  // Search canceled because the search backend isn't available.
  kSearchHandlerUnavailable = 2,
  kMaxValue = kSearchHandlerUnavailable,
};

// Use this when a list search finishes.
void LogListSearchResultState(ListSearchResultState state) {
  base::UmaHistogramEnumeration(
      "Apps.AppList.HelpAppProvider.ListSearchResultState", state);
}

}  // namespace

HelpAppResult::HelpAppResult(Profile* profile,
                             const std::string& id,
                             const std::u16string& title,
                             const gfx::ImageSkia& icon)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(id);
  SetCategory(Category::kHelp);
  SetTitle(title);
  // Show this in the first position, in front of any other chips that may be
  // also claiming the first slot.
  SetDisplayIndex(DisplayIndex::kFirstIndex);
  SetPositionPriority(1.0f);
  SetResultType(ResultType::kHelpApp);
  SetDisplayType(DisplayType::kChip);
  // Some chips have different metrics types.
  if (id == kHelpAppDiscoverResult) {
    SetMetricsType(ash::HELP_APP_DISCOVER);
  } else if (id == kHelpAppUpdatesResult) {
    SetMetricsType(ash::HELP_APP_UPDATES);
  } else {
    SetMetricsType(ash::HELP_APP_DEFAULT);
  }
  SetChipIcon(icon);
}

HelpAppResult::HelpAppResult(
    const float& relevance,
    Profile* profile,
    const ash::help_app::mojom::SearchResultPtr& result,
    const gfx::ImageSkia& icon,
    const std::u16string& query)
    : profile_(profile),
      url_path_(result->url_path_with_parameters),
      help_app_content_id_(result->id) {
  DCHECK(profile_);
  set_id(ash::kChromeUIHelpAppURL + url_path_);
  set_relevance(relevance);
  SetTitle(result->title);
  SetTitleTags(CalculateTags(query, result->title));
  SetResultType(ResultType::kHelpApp);
  SetDisplayType(DisplayType::kList);
  SetMetricsType(ash::HELP_APP_DEFAULT);
  SetIcon(IconInfo(icon, GetAppIconDimension()));
  SetDetails(result->main_category);
}

HelpAppResult::~HelpAppResult() = default;

void HelpAppResult::Open(int event_flags) {
  // Note: event_flags is ignored, LaunchSWA doesn't need it.
  if (id() == kHelpAppDiscoverResult) {
    // Launch discover tab suggestion chip.
    web_app::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app/discover");
    params.launch_source =
        apps::mojom::LaunchSource::kFromAppListRecommendation;
    web_app::LaunchSystemWebAppAsync(
        profile_, web_app::SystemAppType::HELP, params,
        apps::MakeWindowInfo(display::kDefaultDisplayId));

    StopShowingDiscoverTabSuggestionChip(profile_);
    return;
  } else if (id() == kHelpAppUpdatesResult) {
    // Launch release notes suggestion chip.
    base::RecordAction(
        base::UserMetricsAction("ReleaseNotes.SuggestionChipLaunched"));

    web_app::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app/updates");
    params.launch_source =
        apps::mojom::LaunchSource::kFromAppListRecommendation;
    web_app::LaunchSystemWebAppAsync(
        profile_, web_app::SystemAppType::HELP, params,
        apps::MakeWindowInfo(display::kDefaultDisplayId));

    ash::ReleaseNotesStorage(profile_).StopShowingSuggestionChip();
    return;
  }
  // Launch list result.
  web_app::SystemAppLaunchParams params;
  params.url = GURL(ash::kChromeUIHelpAppURL + url_path_);
  params.launch_source = apps::mojom::LaunchSource::kFromAppListQuery;
  web_app::LaunchSystemWebAppAsync(
      profile_, web_app::SystemAppType::HELP, params,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  // This is a google-internal histogram. If changing this, also change the
  // corresponding histograms file.
  base::UmaHistogramSparse("Discover.LauncherSearch.ContentLaunched",
                           base::PersistentHash(help_app_content_id_));
}

HelpAppProvider::HelpAppProvider(Profile* profile)
    : profile_(profile), search_handler_(nullptr) {
  DCHECK(profile_);

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  Observe(&app_service_proxy_->AppRegistryCache());
  LoadIcon();

  if (!base::FeatureList::IsEnabled(
          chromeos::features::kHelpAppLauncherSearch)) {
    // Only get the help app manager if the launcher search feature is enabled.
    return;
  }
  search_handler_ =
      ash::help_app::HelpAppManagerFactory::GetForBrowserContext(profile_)
          ->search_handler();
  if (!search_handler_) {
    return;
  }
  search_handler_->Observe(
      search_results_observer_receiver_.BindNewPipeAndPassRemote());
}

HelpAppProvider::~HelpAppProvider() = default;

void HelpAppProvider::Start(const std::u16string& query) {
  ClearResultsSilently();

  if (query.size() < kMinQueryLength) {
    // Do not do a list search for queries that are too short because the
    // results generally aren't meaningful. This isn't worth logging as a list
    // search result case because it happens frequently when entering a new
    // search query.
    return;
  }

  // Start a search for list results.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  last_query_ = query;

  // Stop the search if:
  //  - the search backend isn't available (or the feature is disabled)
  //  - we don't have an icon to display with results.
  if (!search_handler_) {
    LogListSearchResultState(ListSearchResultState::kSearchHandlerUnavailable);
    return;
  } else if (icon_.isNull()) {
    LogListSearchResultState(ListSearchResultState::kNoHelpAppIcon);
    // This prevents a timeout in the test, but it does not change the user
    // experience because the results were already cleared at the start.
    SearchProvider::Results search_results;
    SwapResults(&search_results);
    return;
  }

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(
      query, kNumRequestedResults,
      base::BindOnce(&HelpAppProvider::OnSearchReturned,
                     weak_factory_.GetWeakPtr(), query, start_time));
}

void HelpAppProvider::StartZeroState() {
  SearchProvider::Results search_results;
  ClearResultsSilently();
  last_query_.clear();

  if (ShouldShowDiscoverTabSuggestionChip(profile_)) {
    search_results.emplace_back(std::make_unique<HelpAppResult>(
        profile_, kHelpAppDiscoverResult,
        l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_SUGGESTION_CHIP),
        icon_));
  } else if (ash::ReleaseNotesStorage(profile_).ShouldShowSuggestionChip()) {
    search_results.emplace_back(std::make_unique<HelpAppResult>(
        profile_, kHelpAppUpdatesResult,
        l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP),
        icon_));
  }
  SwapResults(&search_results);
}

void HelpAppProvider::ViewClosing() {
  last_query_.clear();
}

void HelpAppProvider::OnSearchReturned(
    const std::u16string& query,
    const base::TimeTicks& start_time,
    std::vector<ash::help_app::mojom::SearchResultPtr> sorted_results) {
  DCHECK_LE(sorted_results.size(), kNumRequestedResults);

  SearchProvider::Results search_results;
  for (const auto& result : sorted_results) {
    if (result->relevance_score < kMinScore) {
      break;
    } else if (!app_list_features::IsCategoricalSearchEnabled() &&
               search_results.size() == kMaxShownResults) {
      // Categorical search imposes its own maximums on search results
      // elsewhere.
      break;
    }

    search_results.emplace_back(std::make_unique<HelpAppResult>(
        result->relevance_score, profile_, result, icon_, last_query_));
  }

  base::UmaHistogramTimes("Apps.AppList.HelpAppProvider.QueryTime",
                          base::TimeTicks::Now() - start_time);
  LogListSearchResultState(ListSearchResultState::kOk);
  SwapResults(&search_results);
}

// TODO(b/171828539): Consider using AppListNotifier for better proxy of
// impressions.
void HelpAppProvider::AppListShown() {
  for (auto& result : results()) {
    if (result->id() == kHelpAppDiscoverResult) {
      DecreaseTimesLeftToShowDiscoverTabSuggestionChip(profile_);
    } else if (result->id() == kHelpAppUpdatesResult) {
      ash::ReleaseNotesStorage(profile_)
          .DecreaseTimesLeftToShowSuggestionChip();
    }
  }
}

ash::AppListSearchResultType HelpAppProvider::ResultType() const {
  return ash::AppListSearchResultType::kHelpApp;
}

bool HelpAppProvider::ShouldBlockZeroState() const {
  return true;
}

void HelpAppProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == web_app::kHelpAppId && update.ReadinessChanged() &&
      update.Readiness() == apps::Readiness::kReady) {
    LoadIcon();
  }
}

void HelpAppProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

// If the availability of search results changed, start a new search.
void HelpAppProvider::OnSearchResultAvailabilityChanged() {
  if (last_query_.empty())
    return;
  Start(last_query_);
}

void HelpAppProvider::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = icon_value->uncompressed;
  }
}

void HelpAppProvider::LoadIcon() {
  auto app_type =
      app_service_proxy_->AppRegistryCache().GetAppType(web_app::kHelpAppId);

  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    app_service_proxy_->LoadIcon(
        app_type, web_app::kHelpAppId, apps::IconType::kStandard,
        ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&HelpAppProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  } else {
    app_service_proxy_->LoadIcon(
        apps::ConvertAppTypeToMojomAppType(app_type), web_app::kHelpAppId,
        apps::mojom::IconType::kStandard,
        ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        apps::MojomIconValueToIconValueCallback(base::BindOnce(
            &HelpAppProvider::OnLoadIcon, weak_factory_.GetWeakPtr())));
  }
}

}  // namespace app_list
