// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/help_app_zero_state_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
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
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kHelpAppDiscoverResult[] = "help-app://discover";
constexpr char kHelpAppUpdatesResult[] = "help-app://updates";

// Whether we should show the Discover Tab suggestion chip.
bool ShouldShowDiscoverTabSuggestionChip(Profile* profile) {
  if (!base::FeatureList::IsEnabled(ash::features::kHelpAppDiscoverTab)) {
    return false;
  }

  if (ash::features::IsProductivityLauncherEnabled())
    return false;

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

}  // namespace

HelpAppZeroStateResult::HelpAppZeroStateResult(Profile* profile,
                                               const std::string& id,
                                               DisplayType display_type,
                                               const std::u16string& title,
                                               const std::u16string& details,
                                               const gfx::ImageSkia& icon)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(id);
  SetCategory(Category::kHelp);
  SetTitle(title);
  if (!details.empty())
    SetDetails(details);
  // Show this in the first position, in front of any other chips that may be
  // also claiming the first slot.
  SetDisplayIndex(DisplayIndex::kFirstIndex);
  SetPositionPriority(1.0f);
  SetResultType(ResultType::kZeroStateHelpApp);
  SetDisplayType(display_type);
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

HelpAppZeroStateResult::~HelpAppZeroStateResult() = default;

void HelpAppZeroStateResult::Open(int event_flags) {
  // Note: event_flags is ignored, LaunchSWA doesn't need it.
  if (id() == kHelpAppDiscoverResult) {
    // Launch discover tab suggestion chip.
    web_app::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app/discover");
    params.launch_source =
        apps::mojom::LaunchSource::kFromAppListRecommendation;
    web_app::LaunchSystemWebAppAsync(
        profile_, ash::SystemWebAppType::HELP, params,
        apps::MakeWindowInfo(display::kDefaultDisplayId));

    StopShowingDiscoverTabSuggestionChip(profile_);
  } else if (id() == kHelpAppUpdatesResult) {
    // Launch release notes suggestion chip.
    base::RecordAction(
        base::UserMetricsAction("ReleaseNotes.SuggestionChipLaunched"));

    web_app::SystemAppLaunchParams params;
    params.url = GURL("chrome://help-app/updates");
    params.launch_source =
        apps::mojom::LaunchSource::kFromAppListRecommendation;
    web_app::LaunchSystemWebAppAsync(
        profile_, ash::SystemWebAppType::HELP, params,
        apps::MakeWindowInfo(display::kDefaultDisplayId));

    ash::ReleaseNotesStorage(profile_).StopShowingSuggestionChip();
  }
}

HelpAppZeroStateProvider::HelpAppZeroStateProvider(
    Profile* profile,
    ash::AppListNotifier* notifier)
    : profile_(profile), notifier_(notifier) {
  DCHECK(profile_);

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  Observe(&app_service_proxy_->AppRegistryCache());
  LoadIcon();

  if (notifier_)
    notifier_->AddObserver(this);
}

HelpAppZeroStateProvider::~HelpAppZeroStateProvider() {
  if (notifier_)
    notifier_->RemoveObserver(this);
}

void HelpAppZeroStateProvider::Start(const std::u16string& query) {
  // TODO(crbug.com/1258415): Remove this when non-categorical search is
  // removed. With categorical search enabled, `ClearResultsSilently()` is
  // actually no-op, and the search controller already clears all results
  // that need to be cleared when search query is updated.
  ClearResultsSilently();
}

void HelpAppZeroStateProvider::StartZeroState() {
  SearchProvider::Results search_results;
  ClearResultsSilently();

  if (ShouldShowDiscoverTabSuggestionChip(profile_)) {
    search_results.emplace_back(std::make_unique<HelpAppZeroStateResult>(
        profile_, kHelpAppDiscoverResult, DisplayType::kChip,
        l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_SUGGESTION_CHIP),
        /*details=*/u"", icon_));
  } else if (ash::ReleaseNotesStorage(profile_).ShouldShowSuggestionChip()) {
    // With productivity launcher enabled, release notes are shown in continue
    // section.
    if (ash::features::IsProductivityLauncherEnabled()) {
      auto* color_provider = ash::ColorProvider::Get();
      // NOTE: Color provider may not be set in unit tests.
      SkColor icon_color = color_provider
                               ? color_provider->GetContentLayerColor(
                                     ash::ColorProvider::ContentLayerType::
                                         kButtonIconColorPrimary)
                               : gfx::kGoogleGrey900;
      gfx::ImageSkia icon =
          gfx::CreateVectorIcon(ash::kReleaseNotesChipIcon,
                                app_list::kSystemIconDimension, icon_color);
      search_results.emplace_back(std::make_unique<HelpAppZeroStateResult>(
          profile_, kHelpAppUpdatesResult, DisplayType::kContinue,
          l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_TITLE),
          l10n_util::GetStringUTF16(
              IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_DETAILS),
          icon));
    } else {
      search_results.emplace_back(std::make_unique<HelpAppZeroStateResult>(
          profile_, kHelpAppUpdatesResult, DisplayType::kChip,
          l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP),
          /*details=*/u"", icon_));
    }
  }

  SwapResults(&search_results);
}

ash::AppListSearchResultType HelpAppZeroStateProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateHelpApp;
}

bool HelpAppZeroStateProvider::ShouldBlockZeroState() const {
  return true;
}

void HelpAppZeroStateProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == web_app::kHelpAppId && update.ReadinessChanged() &&
      update.Readiness() == apps::Readiness::kReady) {
    LoadIcon();
  }
}

void HelpAppZeroStateProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void HelpAppZeroStateProvider::OnImpression(
    ash::AppListNotifier::Location location,
    const std::vector<ash::AppListNotifier::Result>& results,
    const std::u16string& query) {
  if (location != ash::AppListNotifier::Location::kChip &&
      location != ash::AppListNotifier::Location::kContinue) {
    return;
  }

  for (const auto& result : results) {
    if (result.id == kHelpAppDiscoverResult) {
      DecreaseTimesLeftToShowDiscoverTabSuggestionChip(profile_);
    } else if (result.id == kHelpAppUpdatesResult) {
      ash::ReleaseNotesStorage(profile_)
          .DecreaseTimesLeftToShowSuggestionChip();
    }
  }
}

void HelpAppZeroStateProvider::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = icon_value->uncompressed;
  }
}

void HelpAppZeroStateProvider::LoadIcon() {
  auto app_type =
      app_service_proxy_->AppRegistryCache().GetAppType(web_app::kHelpAppId);

  if (base::FeatureList::IsEnabled(features::kAppServiceLoadIconWithoutMojom)) {
    app_service_proxy_->LoadIcon(
        app_type, web_app::kHelpAppId, apps::IconType::kStandard,
        ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&HelpAppZeroStateProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  } else {
    app_service_proxy_->LoadIcon(
        apps::ConvertAppTypeToMojomAppType(app_type), web_app::kHelpAppId,
        apps::mojom::IconType::kStandard,
        ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        apps::MojomIconValueToIconValueCallback(
            base::BindOnce(&HelpAppZeroStateProvider::OnLoadIcon,
                           weak_factory_.GetWeakPtr())));
  }
}

}  // namespace app_list
