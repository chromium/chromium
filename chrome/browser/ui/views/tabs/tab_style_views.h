// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"
#include "ui/gfx/geometry/rect_f.h"

class Tab;

// Holds Views-specific logic for rendering and sizing tabs.
class TabStyleViews : public TabStyle {
 public:
  // Factory function allows to experiment with different variations on tab
  // style at runtime or via flag.
  static std::unique_ptr<TabStyleViews> CreateForTab(Tab* tab);

  ~TabStyleViews() override;

  // Returns the minimum possible width of a selected Tab. Selected tabs must
  // always show a close button, and thus have a larger minimum size than
  // unselected tabs.
  static int GetMinimumActiveWidth();

  // Returns the minimum possible width of a single unselected Tab.
  static int GetMinimumInactiveWidth();

 protected:
  // Avoid implicitly-deleted constructor.
  TabStyleViews() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STYLE_VIEWS_H_
