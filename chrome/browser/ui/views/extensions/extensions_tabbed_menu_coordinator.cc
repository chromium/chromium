// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_coordinator.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/ui/views/extensions/extensions_tabbed_menu_view.h"
#include "extensions/common/extension_features.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

ExtensionsTabbedMenuCoordinator::ExtensionsTabbedMenuCoordinator(
    Browser* browser,
    ExtensionsContainer* extensions_container,
    bool allow_pining)
    : browser_(browser),
      extensions_container_(extensions_container),
      allow_pining_(allow_pining) {}

ExtensionsTabbedMenuCoordinator::~ExtensionsTabbedMenuCoordinator() {
  Hide();
}

void ExtensionsTabbedMenuCoordinator::Show(
    views::View* anchor_view,
    ExtensionsToolbarButton::ButtonType button_type) {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  auto menu = std::make_unique<ExtensionsTabbedMenuView>(
      anchor_view, browser_, extensions_container_, button_type, allow_pining_);
  extensions_tabbed_menu_view_tracker_.SetView(menu.get());
  views::BubbleDialogDelegateView::CreateBubble(std::move(menu))->Show();
}

void ExtensionsTabbedMenuCoordinator::Hide() {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));
  if (IsShowing()) {
    extensions_tabbed_menu_view_tracker_.view()->GetWidget()->Close();
    // Immediately stop tracking the view. Widget will be destroyed
    // asynchronously.
    extensions_tabbed_menu_view_tracker_.SetView(nullptr);
  }
}

bool ExtensionsTabbedMenuCoordinator::IsShowing() const {
  return !!extensions_tabbed_menu_view_tracker_.view();
}

ExtensionsTabbedMenuView*
ExtensionsTabbedMenuCoordinator::GetExtensionsTabbedMenuView() {
  return IsShowing() ? static_cast<ExtensionsTabbedMenuView*>(
                           extensions_tabbed_menu_view_tracker_.view())
                     : nullptr;
}
