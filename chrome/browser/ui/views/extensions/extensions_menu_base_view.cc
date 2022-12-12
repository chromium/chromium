// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_base_view.h"

#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

ExtensionsMenuBaseView::ExtensionsMenuBaseView(Browser* browser) {
  auto initial_page =
      std::make_unique<ExtensionsMenuMainPageView>(browser, this);

  views::Builder<ExtensionsMenuBaseView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .AddChildren(
          views::Builder<PageSwitcherView>(
              std::make_unique<PageSwitcherView>(std::move(initial_page)))
              .CopyAddressTo(&page_container_))
      .BuildChildren();
}

void ExtensionsMenuBaseView::CloseBubble() {
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}
