// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_LAYOUT_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_LAYOUT_H_

#include "ui/gfx/geometry/size.h"

namespace base {
class Value;
}

struct TabStripUILayout {
  static TabStripUILayout CalculateForWebViewportSize(
      const gfx::Size& viewport_size);

  // Returns a dictionary of CSS variables.
  base::Value AsDictionary() const;

  // Returns the tab strip's total height. This should be used to size
  // its container.
  int CalculateContainerHeight() const;

  int padding_around_tab_list;
  int tab_title_height;
  gfx::Size tab_thumbnail_size;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_LAYOUT_H_
