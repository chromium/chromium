// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_FULL_WEBUI_OMNIBOX_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_FULL_WEBUI_OMNIBOX_LAYOUT_HELPER_H_

#include "ui/gfx/geometry/insets.h"

namespace content {
class WebUIDataSource;
}

// Single source of truth for full WebUI Omnibox layout constants, font sizes,
// and alignment insets, injecting them dynamically into the WebUI data source.
class FullWebUIOmniboxLayoutHelper {
 public:
  FullWebUIOmniboxLayoutHelper() = delete;

  // Returns the alignment insets for the full WebUI Omnibox popup.
  static gfx::Insets GetLocationBarAlignmentInsets();

  // Returns the location bar height in DIPs.
  static int GetLocationBarHeight();

  // Returns the page info icon vertical padding in DIPs.
  static int GetLocationBarPageInfoIconVerticalPadding();

  // Returns the location bar icon size in DIPs.
  static int GetLocationBarIconSize();

  // Returns the font size for the Omnibox input text.
  static int GetFontSize();

  // Injects layout constants, font sizes, and alignment insets into the WebUI
  // data source.
  static void PopulateLoadTimeData(content::WebUIDataSource* source);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_POPUP_FULL_WEBUI_OMNIBOX_LAYOUT_HELPER_H_
