// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_util.h"

#include <memory>

#include "chrome/browser/ui/side_panel/side_panel_content_proxy.h"
#include "ui/base/class_property.h"

// static
SidePanelContentProxy* SidePanelUtil::GetSidePanelContentProxy(
    ui::PropertyHandler* content_view) {
  if (!content_view->GetProperty(kSidePanelContentProxyKey)) {
    content_view->SetProperty(
        kSidePanelContentProxyKey,
        std::make_unique<SidePanelContentProxy>(true).release());
  }
  return content_view->GetProperty(kSidePanelContentProxyKey);
}
