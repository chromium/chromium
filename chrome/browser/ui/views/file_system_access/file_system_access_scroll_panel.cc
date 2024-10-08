// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_scroll_panel.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/file_system_access/file_system_access_ui_helpers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kFolderIconSize = 16;
constexpr int kFilenameAreaMargin = 12;
constexpr int kBetweenFilenameSpacing = 6;
constexpr int kMaxFilenamesInViewPort = 3;
constexpr float kCornerRadius = 8.0f;

}  // namespace

std::unique_ptr<views::ScrollView> FileSystemAccessScrollPanel::Create(
    const std::vector<base::FilePath>& file_paths) {
  auto file_list_container = std::make_unique<views::View>();
  ChromeLayoutProvider* chrome_layout_provider = ChromeLayoutProvider::Get();
  file_list_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kFilenameAreaMargin, kFilenameAreaMargin), 0));
  for (const auto& file_path : file_paths) {
    auto* line_container =
        file_list_container->AddChildView(std::make_unique<views::View>());
    line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(kBetweenFilenameSpacing, 0, kBetweenFilenameSpacing,
                          0),
        chrome_layout_provider->GetDistanceMetric(
            DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING)));

    auto* icon = line_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            vector_icons::kFolderOpenIcon, ui::kColorIcon, kFolderIconSize)));
    icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);

    auto* label = line_container->AddChildView(std::make_unique<views::Label>(
        file_system_access_ui_helper::GetPathForDisplayAsParagraph(
            content::PathInfo(file_path))));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  // TODO(crbug.com/40101962): Determine if/how file names should be focused for
  // accessibility.
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetBackgroundThemeColorId(ui::kColorSubtleEmphasisBackground);
  int line_container_height =
      file_list_container->children().empty()
          ? 0
          : file_list_container->children()[0]->GetPreferredSize().height();
  scroll_view->SetContents(std::move(file_list_container));
  scroll_view->SetPaintToLayer();
  scroll_view->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));
  // 1 pixel correction per item in the `ScrollView` helps avoid showing
  // pixels from the next/previous entry that is outside the viewport.
  int max_scroll_view_height = 2 * kFilenameAreaMargin +
                               kMaxFilenamesInViewPort * line_container_height -
                               kMaxFilenamesInViewPort;
  scroll_view->ClipHeightTo(0, max_scroll_view_height);
  return scroll_view;
}
