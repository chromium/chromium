// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop_view_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"

ExtensionsToolbarDesktopViewController::ExtensionsToolbarDesktopViewController(
    Browser* browser,
    ExtensionsToolbarDesktop* extensions_container)
    : browser_(browser), extensions_container_(extensions_container) {}

ExtensionsToolbarDesktopViewController::
    ~ExtensionsToolbarDesktopViewController() {
  extensions_container_ = nullptr;
}

void ExtensionsToolbarDesktopViewController::
    WindowControlsOverlayEnabledChanged(bool enabled) {
  if (!extensions_container_->main_item()) {
    return;
  }

  extensions_container_->UpdateContainerVisibility();
  extensions_container_->main_item()->ClearProperty(views::kFlexBehaviorKey);

  views::MinimumFlexSizeRule min_flex_rule =
      enabled ? views::MinimumFlexSizeRule::kPreferred
              : views::MinimumFlexSizeRule::kPreferredSnapToZero;
  extensions_container_->main_item()->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(min_flex_rule,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(ExtensionsToolbarDesktopViewController::
                         kFlexOrderExtensionsButton));
}
