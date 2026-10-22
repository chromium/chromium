// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/full_webui_omnibox_layout_helper.h"

#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"
#include "content/public/browser/web_ui_data_source.h"

// static
gfx::Insets FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets() {
  return FullWebUIOmniboxFrame::GetLocationBarAlignmentInsets();
}

// static
void FullWebUIOmniboxLayoutHelper::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  const gfx::Insets insets = GetLocationBarAlignmentInsets();
  source->AddInteger("alignmentInsetTop", insets.top());
  source->AddInteger("alignmentInsetHorizontal", insets.left());
  source->AddInteger("alignmentInsetBottom", insets.bottom());
}
