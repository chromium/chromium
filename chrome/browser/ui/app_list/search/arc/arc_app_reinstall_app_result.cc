// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_app_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

namespace app_list {

namespace {
constexpr char kPlayStoreAppUrlPrefix[] =
    "https://play.google.com/store/apps/details?id=";

// We choose a default app reinstallation relevance; This ranks app reinstall
// app result as a top result typically.
constexpr float kAppReinstallRelevance = 0.7;

// Apply background and mask to make the icon similar to the icon of
// ArcPlaystoreSearchResult.
gfx::ImageSkia ApplyBackgroundAndMask(const gfx::ImageSkia& image) {
  return gfx::ImageSkiaOperations::CreateResizedImage(
      apps::ApplyBackgroundAndMask(image),
      skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(
          ash::SharedAppListConfig::instance().search_tile_icon_dimension(),
          ash::SharedAppListConfig::instance().search_tile_icon_dimension()));
}
}  // namespace

ArcAppReinstallAppResult::ArcAppReinstallAppResult(
    const arc::mojom::AppReinstallCandidatePtr& mojom_data,
    const gfx::ImageSkia& app_icon,
    Observer* observer)
    : observer_(observer), package_name_(mojom_data->package_name) {
  DCHECK(observer_);
  set_id(kPlayStoreAppUrlPrefix + mojom_data->package_name);
  SetResultType(ash::AppListSearchResultType::kPlayStoreReinstallApp);
  SetTitle(base::UTF8ToUTF16(mojom_data->title));
  SetDisplayType(ash::SearchResultDisplayType::kTile);
  SetMetricsType(ash::PLAY_STORE_REINSTALL_APP);
  SetDisplayIndex(ash::SearchResultDisplayIndex::kSixthIndex);
  SetIsRecommendation(true);
  set_relevance(kAppReinstallRelevance);
  SetNotifyVisibilityChange(true);
  const gfx::ImageSkia masked_app_icon(ApplyBackgroundAndMask(app_icon));
  SetIcon(masked_app_icon);
  SetChipIcon(masked_app_icon);
  SetBadgeIcon(ui::ImageModel::FromVectorIcon(
      vector_icons::kCloudDownloadIcon,
      ui::NativeTheme::kColorId_DefaultIconColor,
      ash::SharedAppListConfig::instance().search_tile_badge_icon_dimension()));
  SetUseBadgeIconBackground(true);
  SetNotifyVisibilityChange(true);

  if (mojom_data->star_rating != 0.0f) {
    SetRating(mojom_data->star_rating);
  }
}

ArcAppReinstallAppResult::~ArcAppReinstallAppResult() = default;

void ArcAppReinstallAppResult::Open(int /*event_flags*/) {
  RecordAction(base::UserMetricsAction("ArcAppReinstall_Clicked"));

  DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      ProfileManager::GetPrimaryUserProfile()));
  apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile())
      ->LaunchAppWithUrl(arc::kPlayStoreAppId, ui::EF_NONE, GURL(id()),
                         apps::mojom::LaunchSource::kFromChromeInternal);

  observer_->OnOpened(package_name_);
}

void ArcAppReinstallAppResult::OnVisibilityChanged(bool visibility) {
  ChromeSearchResult::OnVisibilityChanged(visibility);
  observer_->OnVisibilityChanged(package_name_, visibility);
}

}  // namespace app_list
