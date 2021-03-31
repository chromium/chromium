// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/help_app_provider.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr char kHelpAppResult[] = "help-app://updates";

}  // namespace

HelpAppResult::HelpAppResult(float relevance,
                             Profile* profile,
                             const gfx::ImageSkia& icon)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(kHelpAppResult);
  SetTitle(l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP));
  // Show this in the first position, in front of any other chips that may be
  // also claiming the first slot.
  SetDisplayIndex(DisplayIndex::kFirstIndex);
  SetPositionPriority(1.0f);
  SetResultType(ResultType::kHelpApp);
  SetDisplayType(DisplayType::kChip);
  SetMetricsType(ash::HELP_APP);
  SetChipIcon(icon);
}

HelpAppResult::~HelpAppResult() = default;

void HelpAppResult::Open(int event_flags) {
  base::RecordAction(
      base::UserMetricsAction("ReleaseNotes.SuggestionChipLaunched"));
  apps::AppServiceProxyFactory::GetForProfile(profile_)->LaunchAppWithUrl(
      web_app::kHelpAppId, event_flags, GURL("chrome://help-app/updates"),
      apps::mojom::LaunchSource::kFromAppListRecommendation,
      apps::MakeWindowInfo(display::kDefaultDisplayId));
  chromeos::ReleaseNotesStorage(profile_).StopShowingSuggestionChip();
}

HelpAppProvider::HelpAppProvider(Profile* profile) : profile_(profile) {
  DCHECK(profile_);

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  Observe(&app_service_proxy_->AppRegistryCache());
  LoadIcon();
}

HelpAppProvider::~HelpAppProvider() = default;

void HelpAppProvider::Start(const std::u16string& query) {
  // This provider doesn't handle searches, if there is any query just clear the
  // results and return.
  if (!query.empty()) {
    ClearResultsSilently();
    return;
  }

  SearchProvider::Results search_results;
  if (chromeos::ReleaseNotesStorage(profile_).ShouldShowSuggestionChip()) {
    search_results.emplace_back(
        std::make_unique<HelpAppResult>(1.0f, profile_, icon_));
  }
  SwapResults(&search_results);
}

// TODO(b/171828539): Consider using AppListNotifier for better proxy of
// impressions.
void HelpAppProvider::AppListShown() {
  chromeos::ReleaseNotesStorage(profile_)
      .DecreaseTimesLeftToShowSuggestionChip();
}

ash::AppListSearchResultType HelpAppProvider::ResultType() {
  return ash::AppListSearchResultType::kHelpApp;
}

void HelpAppProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == web_app::kHelpAppId && update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kReady) {
    LoadIcon();
  }
}

void HelpAppProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void HelpAppProvider::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type == icon_type) {
    icon_ = icon_value->uncompressed;
  }
}

void HelpAppProvider::LoadIcon() {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  app_service_proxy_->LoadIcon(
      apps::mojom::AppType::kWeb, web_app::kHelpAppId, icon_type,
      ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&HelpAppProvider::OnLoadIcon, weak_factory_.GetWeakPtr()));
}

}  // namespace app_list
