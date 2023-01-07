// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_LAYOUT_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_LAYOUT_H_

#include "base/containers/flat_map.h"
#include "ui/gfx/geometry/size.h"

struct TabStripUILayout {
  static TabStripUILayout CalculateForWebViewportSize(
      const gfx::Size& viewport_size);

  // Returns the tab strip's total height. This should be used to size
  // its container.
  static int GetContainerHeight();

  // Returns a dictionary of CSS variables.
  base::flat_map<std::string, std::string> AsDictionary() const;

  int viewport_width = 0;
  gfx::Size tab_thumbnail_size;
  double tab_thumbnail_aspect_ratio = 1.0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_LAYOUT_H_
