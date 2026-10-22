// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_FULL_WEBUI_OMNIBOX_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_FULL_WEBUI_OMNIBOX_LAYOUT_HELPER_H_

#include "ui/gfx/geometry/insets.h"

namespace content {
class WebUIDataSource;
}

// Injects the full WebUI Omnibox popup's alignment insets into the WebUI data
// source.
class FullWebUIOmniboxLayoutHelper {
 public:
  FullWebUIOmniboxLayoutHelper() = delete;

  // Returns the alignment insets for the full WebUI Omnibox popup.
  static gfx::Insets GetLocationBarAlignmentInsets();

  // Injects the popup-only alignment insets into the WebUI data source. The
  // location bar's layout constants and fonts come from
  // WebUIToolbarLayoutCssHelper's layout_constants_v0.css.
  static void PopulateLoadTimeData(content::WebUIDataSource* source);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_FULL_WEBUI_OMNIBOX_LAYOUT_HELPER_H_
