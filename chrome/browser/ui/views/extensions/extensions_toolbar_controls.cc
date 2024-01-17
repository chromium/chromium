// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_controls.h"

#include "chrome/browser/ui/views/extensions/extensions_request_access_button.h"

ExtensionsToolbarControls::ExtensionsToolbarControls(
    raw_ptr<ExtensionsToolbarButton> extensions_button,
    raw_ptr<ExtensionsRequestAccessButton> request_access_button)
    : request_access_button_(request_access_button),
      extensions_button_(extensions_button) {
  request_access_button_->SetVisible(false);
}

ExtensionsToolbarControls::~ExtensionsToolbarControls() = default;

void ExtensionsToolbarControls::ResetConfirmation() {
  request_access_button_->ResetConfirmation();
}

bool ExtensionsToolbarControls::IsShowingConfirmation() const {
  return request_access_button_->IsShowingConfirmation();
}

bool ExtensionsToolbarControls::IsShowingConfirmationFor(
    const url::Origin& origin) const {
  return request_access_button_->IsShowingConfirmationFor(origin);
}
