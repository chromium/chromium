// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include <memory>

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"

ExtensionsToolbarControls::ExtensionsToolbarControls(
    std::unique_ptr<ExtensionsToolbarButton> extensions_button,
    std::unique_ptr<ExtensionsToolbarButton> site_access_button,
    std::unique_ptr<ExtensionsRequestAccessButton> request_access_button)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      request_access_button_(AddChildView(std::move(request_access_button))),
      site_access_button_(AddChildView(std::move(site_access_button))),
      extensions_button_(extensions_button.get()) {
  site_access_button_->SetVisible(false);
  request_access_button_->SetVisible(true);
  // TODO(emiliapaz): Consider changing AddMainItem() to receive a unique_ptr.
  AddMainItem(extensions_button.release());
}

ExtensionsToolbarControls::~ExtensionsToolbarControls() = default;

void ExtensionsToolbarControls::UpdateAllIcons() {}

void ExtensionsToolbarControls::UpdateSiteAccessButtonVisibility(
    bool visibility) {
  site_access_button_->SetVisible(visibility);
}

void ExtensionsToolbarControls::UpdateRequestAccessButton(
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions) {
  // TODO(crbug.com/1239772): Display site access button, and add the action
  // icons to the button and tooltip only when 1+ actions are given. For now,
  // setting visible as true to see the button in the toolbar.
  request_access_button_->SetVisible(true);
}

BEGIN_METADATA(ExtensionsToolbarControls, ToolbarIconContainerView)
END_METADATA
