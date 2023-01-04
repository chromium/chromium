// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view_controller.h"

#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"
#include "ui/views/widget/widget.h"

ExtensionsMenuViewController::ExtensionsMenuViewController(
    Browser* browser,
    PageSwitcherView* contents_view)
    : browser_(browser), contents_view_(contents_view) {}

void ExtensionsMenuViewController::OpenMainPage() {
  auto main_page = std::make_unique<ExtensionsMenuMainPageView>(browser_, this);
  contents_view_->SwitchToPage(std::move(main_page));
}

void ExtensionsMenuViewController::OpenSitePermissionsPage() {
  auto site_permissions_page =
      std::make_unique<ExtensionsMenuSitePermissionsPage>(this);
  contents_view_->SwitchToPage(std::move(site_permissions_page));
}

void ExtensionsMenuViewController::CloseBubble() {
  contents_view_->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}
