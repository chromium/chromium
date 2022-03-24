// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/games/game_result.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace {

constexpr char16_t kPlatformDelimiter[] = u", ";
constexpr char16_t kDetailsDelimiter[] = u" - ";
constexpr char16_t kA11yDelimiter[] = u", ";

}  // namespace

GameResult::GameResult(Profile* profile,
                       AppListControllerDelegate* list_controller,
                       GameIndexManager* index_manager,
                       const GameData& game_data,
                       double relevance,
                       const std::u16string& query)
    : profile_(profile),
      list_controller_(list_controller),
      launch_url_(game_data.launch_url) {
  DCHECK(profile);
  DCHECK(list_controller);
  DCHECK(index_manager);
  DCHECK(launch_url_.is_valid());

  set_id(launch_url_.spec());
  set_relevance(relevance);

  SetMetricsType(ash::GAME_SEARCH);
  SetResultType(ResultType::kGames);
  SetDisplayType(DisplayType::kList);
  SetCategory(Category::kGames);

  UpdateText(game_data, query);

  // TODO(crbug.com/1305880): Set a default icon.

  index_manager->GetIcon(game_data.icon_url,
                         base::BindOnce(&GameResult::OnIconLoaded,
                                        weak_ptr_factory_.GetWeakPtr()));
}

GameResult::~GameResult() = default;

void GameResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, launch_url_, ui::PAGE_TRANSITION_TYPED,
                            ui::DispositionFromEventFlags(event_flags));
}

void GameResult::UpdateText(const GameData& game_data,
                            const std::u16string& query) {
  SetTitle(game_data.title);
  SetTitleTags(CalculateTags(query, title()));

  std::vector<ash::SearchResultTextItem> details;
  std::vector<std::u16string> accessible_name;

  accessible_name.push_back(title());
  accessible_name.push_back(kA11yDelimiter);

  std::u16string source = GameSourceDisplayString(game_data.source);
  details.push_back(CreateStringTextItem(source).SetElidable(false));
  accessible_name.push_back(source);

  if (game_data.platforms) {
    std::u16string platforms =
        base::JoinString(game_data.platforms.value(), kPlatformDelimiter);

    details.push_back(CreateStringTextItem(kDetailsDelimiter));
    details.push_back(
        CreateStringTextItem(IDS_APP_LIST_SEARCH_GAME_PLATFORMS_PREFIX));
    details.push_back(CreateStringTextItem(u" "));
    details.push_back(CreateStringTextItem(platforms));

    accessible_name.push_back(kA11yDelimiter);
    accessible_name.push_back(
        l10n_util::GetStringUTF16(IDS_APP_LIST_SEARCH_GAME_PLATFORMS_PREFIX));
    accessible_name.push_back(u" ");
    accessible_name.push_back(platforms);
  }

  SetDetailsTextVector(details);
  SetAccessibleName(base::StrCat(accessible_name));
}

void GameResult::OnIconLoaded(const SkBitmap* bitmap) {
  if (!bitmap || bitmap->isNull())
    return;

  IconInfo icon_info(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap),
                     GetAppIconDimension(), IconShape::kRoundedRectangle);
  SetIcon(icon_info);
}

}  // namespace app_list
