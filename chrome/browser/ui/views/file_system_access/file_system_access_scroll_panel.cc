// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_scroll_panel.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_ui_helpers.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"


std::unique_ptr<views::ScrollView> FileSystemAccessScrollPanel::Create(
    const std::vector<base::FilePath>& file_paths) {
  auto file_list_container = std::make_unique<views::View>();
  ChromeLayoutProvider* chrome_layout_provider = ChromeLayoutProvider::Get();
  file_list_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(FILENAME_AREA_MARGIN, FILENAME_AREA_MARGIN),
      BETWEEN_FILENAME_SPACING));
  for (const auto& file_path : file_paths) {
    auto* line_container =
        file_list_container->AddChildView(std::make_unique<views::View>());
    line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        chrome_layout_provider->GetDistanceMetric(
            DISTANCE_PERMISSION_PROMPT_HORIZONTAL_ICON_LABEL_PADDING)));

    auto* icon = line_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            vector_icons::kFolderOpenIcon, ui::kColorIcon, FOLDER_ICON_SIZE)));
    icon->SetVerticalAlignment(views::ImageView::Alignment::kCenter);

    auto* label = line_container->AddChildView(std::make_unique<views::Label>(
        file_system_access_ui_helper::GetPathForDisplayAsParagraph(file_path)));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
  // TODO(crbug.com/1011533): Add border radius to the scroll view, and
  // determine if/how file names should be focused for accessibility.
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetBackgroundThemeColorId(ui::kColorSubtleEmphasisBackground);
  scroll_view->SetContents(std::move(file_list_container));
  scroll_view->ClipHeightTo(0, MAX_SCROLL_HEIGHT);
  return scroll_view;
}
