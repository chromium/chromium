// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

ExtensionsMenuMainPageView::ExtensionsMenuMainPageView(
    ExtensionsMenuNavigationHandler* navigation_handler)
    : navigation_handler_(navigation_handler) {
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);

  views::Builder<ExtensionsMenuMainPageView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      // TODO(crbug.com/1390952): Add margins after adding the menu
      // items, to make sure all items are aligned.
      .AddChildren(
          views::Builder<views::FlexLayoutView>()
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .SetProperty(views::kFlexBehaviorKey, stretch_specification)
              .AddChildren(
                  views::Builder<views::FlexLayoutView>()
                      .SetOrientation(views::LayoutOrientation::kVertical)
                      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                      .SetProperty(views::kFlexBehaviorKey,
                                   stretch_specification)
                      .AddChildren(
                          views::Builder<views::Label>()
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_MENU_TITLE))
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetTextContext(
                                  views::style::CONTEXT_DIALOG_TITLE)
                              .SetTextStyle(views::style::STYLE_SECONDARY),
                          views::Builder<views::Label>()
                              // TODO(crbug.com/1390952): Change to current
                              // site.
                              .SetText(u"site.com")
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetTextContext(views::style::CONTEXT_LABEL)
                              .SetTextStyle(views::style::STYLE_SECONDARY)
                              .SetAllowCharacterBreak(true)
                              .SetMultiLine(true)
                              .SetProperty(views::kFlexBehaviorKey,
                                           stretch_specification)),
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuNavigationHandler::CloseBubble,
                              base::Unretained(navigation_handler_))))))
      .BuildChildren();
}
