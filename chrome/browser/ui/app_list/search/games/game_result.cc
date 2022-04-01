// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_result.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_discovery_service/game_extras.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {
namespace {

constexpr char16_t kPlatformDelimiter[] = u", ";
constexpr char16_t kDetailsDelimiter[] = u" - ";
constexpr char16_t kA11yDelimiter[] = u", ";

// TODO(crbug.com/1305880): Replace with launch URL once finalized.
constexpr char kLaunchUrlPrefix[] = "https://todo.com?game=";

GURL LaunchUrlFromId(const std::string& id) {
  return GURL(base::StrCat({kLaunchUrlPrefix, id}));
}

std::u16string DisplayStringForGameSource(apps::GameExtras::Source source) {
  // TODO(crbug.com/1305880): Replace with display string once finalized.
  return u"[game source]";
}

bool IsDarkModeEnabled() {
  // TODO(crbug.com/1258415): Simplify this logic once the productivity launcher
  // is launched.

  // Launcher search results UI is light by default, so use icons for light
  // background if dark/light mode feature is not enabled. Productivity launcher
  // has dark background by default, so use icons for dark background in that
  // case.
  if (ash::features::IsProductivityLauncherEnabled())
    return true;
  auto* provider = ash::ColorProvider::Get();
  if (!provider)
    return false;
  return ash::features::IsDarkLightModeEnabled() &&
         provider->IsDarkModeEnabled();
}

}  // namespace

GameResult::GameResult(Profile* profile,
                       AppListControllerDelegate* list_controller,
                       apps::AppDiscoveryService* app_discovery_service,
                       const apps::Result& game,
                       double relevance,
                       const std::u16string& query)
    : profile_(profile),
      list_controller_(list_controller),
      launch_url_(LaunchUrlFromId(game.GetAppId())) {
  DCHECK(profile);
  DCHECK(list_controller);
  DCHECK(app_discovery_service);

  set_id(launch_url_.spec());
  set_relevance(relevance);

  SetMetricsType(ash::GAME_SEARCH);
  SetResultType(ResultType::kGames);
  SetDisplayType(DisplayType::kList);
  SetCategory(Category::kGames);

  UpdateText(game, query);

  SetGenericIcon();
  // TODO(crbug.com/1305880): Request icon from app disocvery service once API
  // added.
}

GameResult::~GameResult() = default;

void GameResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, launch_url_, ui::PAGE_TRANSITION_TYPED,
                            ui::DispositionFromEventFlags(event_flags));
}

void GameResult::UpdateText(const apps::Result& game,
                            const std::u16string& query) {
  const apps::GameExtras* extras = game.GetSourceExtras()->AsGameExtras();

  SetTitle(game.GetAppTitle());
  SetTitleTags(CalculateTags(query, title()));

  std::vector<ash::SearchResultTextItem> details;
  std::vector<std::u16string> accessible_name;

  accessible_name.push_back(title());
  accessible_name.push_back(kA11yDelimiter);

  std::u16string source = DisplayStringForGameSource(extras->GetSource());
  details.push_back(CreateStringTextItem(source).SetElidable(false));
  accessible_name.push_back(source);

  const auto& platforms = extras->GetPlatforms();
  if (platforms) {
    std::u16string platforms_string =
        base::JoinString(platforms.value(), kPlatformDelimiter);

    details.push_back(CreateStringTextItem(kDetailsDelimiter));
    details.push_back(
        CreateStringTextItem(IDS_APP_LIST_SEARCH_GAME_PLATFORMS_PREFIX));
    details.push_back(CreateStringTextItem(u" "));
    details.push_back(CreateStringTextItem(platforms_string));

    accessible_name.push_back(kA11yDelimiter);
    accessible_name.push_back(
        l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_GAME_PLATFORMS_PREFIX));
    accessible_name.push_back(u" ");
    accessible_name.push_back(platforms_string);
  }

  SetDetailsTextVector(details);
  SetAccessibleName(base::StrCat(accessible_name));
}

void GameResult::SetGenericIcon() {
  const auto color = cros_styles::ResolveColor(
      cros_styles::ColorName::kIconColorPrimary, IsDarkModeEnabled(),
      /*use_debug_colors=*/false);
  const gfx::IconDescription description(ash::kGameGenericIcon,
                                         GetAppIconDimension(), color);
  SetIcon(IconInfo(gfx::CreateVectorIcon(description), GetAppIconDimension(),
                   IconShape::kRectangle));
}

void GameResult::OnIconLoaded(const SkBitmap* bitmap) {
  // TODO(crbug.com/1305880): This is not used yet. Should be passed as the
  // callback to the app discovery icon fetching method once available.
  if (!bitmap || bitmap->isNull())
    return;
  // TODO(crbug.com/1305880): Possibly change the icon shape to a square with a
  // border circle, once UI decision is finalized.
  SetIcon(IconInfo(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap),
                   GetAppIconDimension(), IconShape::kRoundedRectangle));
}

}  // namespace app_list
