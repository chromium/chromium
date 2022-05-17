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
  request_access_button_->SetVisible(false);
  // TODO(emiliapaz): Consider changing AddMainItem() to receive a unique_ptr.
  AddMainItem(extensions_button.release());
}

ExtensionsToolbarControls::~ExtensionsToolbarControls() = default;

void ExtensionsToolbarControls::UpdateAllIcons() {}

void ExtensionsToolbarControls::UpdateSiteAccessButtonVisibility(
    bool visibility) {
  site_access_button_->SetVisible(visibility);

  ResetLayout();
}

void ExtensionsToolbarControls::UpdateRequestAccessButton(
    std::vector<ToolbarActionViewController*> extensions_requesting_access) {
  if (extensions_requesting_access.empty()) {
    request_access_button_->SetVisible(false);
  } else {
    // TODO(crbug.com/1239772): Update icons, based on the number of extensions
    // requesting access, once multiple icons in button is supported. Since we
    // will need to access the extension information, this method may receive
    // actions instead of actions count. For now, just show the number of
    // actions.
    request_access_button_->UpdateExtensionsRequestingAccess(
        extensions_requesting_access);
    request_access_button_->SetVisible(true);
  }

  ResetLayout();
}

void ExtensionsToolbarControls::ResetLayout() {
  GetAnimatingLayoutManager()->ResetLayout();
}

BEGIN_METADATA(ExtensionsToolbarControls, ToolbarIconContainerView)
END_METADATA
