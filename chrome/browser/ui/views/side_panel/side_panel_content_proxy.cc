// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(SidePanelContentProxy*)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(SidePanelContentProxy,
                                   kSidePanelContentProxyKey,
                                   nullptr)

SidePanelContentProxy::SidePanelContentProxy(bool available)
    : available_(available) {}

SidePanelContentProxy::~SidePanelContentProxy() = default;

void SidePanelContentProxy::SetAvailable(bool available) {
  available_ = available;
  if (available && available_callback_)
    std::move(available_callback_).Run();
}

void SidePanelContentProxy::ResetAvailableCallback() {
  available_callback_.Reset();
}
