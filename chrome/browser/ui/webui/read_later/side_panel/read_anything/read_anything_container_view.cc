// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_container_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_constants.h"
#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

ReadAnythingContainerView::ReadAnythingContainerView(Browser* browser) {
  // Create and set a FlexLayout LayoutManager for this view, set background.
  auto layout = std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets(kInternalInsets));

  SetLayoutManager(std::move(layout));
  SetBackground(
      views::CreateThemedSolidBackground(this, ui::kColorPrimaryBackground));

  // Create the toolbar for the side panel.
  auto toolbar = std::make_unique<ReadAnythingToolbarView>();
  toolbar->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(1));

  // Create a separator.
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(2));

  // Create the main content view for the side panel.
  auto content_web_view = std::make_unique<SidePanelWebUIViewT<ReadAnythingUI>>(
      browser,
      /* on_show_cb= */ base::RepeatingClosure(),
      /* close_cb= */ base::RepeatingClosure(),
      /* contents_wrapper= */
      std::make_unique<BubbleContentsWrapperT<ReadAnythingUI>>(
          /* webui_url= */ GURL(chrome::kChromeUIReadAnythingSidePanelURL),
          /* browser_context= */ browser->profile(),
          /* task_manager_string_id= */ IDS_READ_ANYTHING_TITLE,
          /* webui_resizes_host= */ false,
          /* esc_closes_ui= */ false));
  content_web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  // Add all components to view.
  AddChildView(std::move(toolbar));
  AddChildView(std::move(separator));
  AddChildView(std::move(content_web_view));
}

ReadAnythingContainerView::~ReadAnythingContainerView() = default;
