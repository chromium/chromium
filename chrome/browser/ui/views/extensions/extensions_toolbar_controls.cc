// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

ExtensionsToolbarControls::ExtensionsToolbarControls(
    Browser* browser,
    ExtensionsToolbarButton* extensions_button)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      extensions_button_(extensions_button) {
  // Do not flip the Extensions icon in RTL.
  extensions_button_->SetFlipCanvasOnPaintForRTLUI(false);
  extensions_button_->SetID(VIEW_ID_EXTENSIONS_MENU_BUTTON);

  AddMainItem(extensions_button_);
}

ExtensionsToolbarControls::~ExtensionsToolbarControls() = default;

void ExtensionsToolbarControls::UpdateAllIcons() {
  extensions_button_->UpdateIcon();
}

BEGIN_METADATA(ExtensionsToolbarControls, ToolbarIconContainerView)
END_METADATA
