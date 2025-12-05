// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_ICON_H_

#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace tabs {
class TabInterface;
}

// View for a vertical tab's favicon.
// TODO(466091175): Replace with just TabIcon.
class VerticalTabIcon : public TabIcon {
  METADATA_HEADER(VerticalTabIcon, TabIcon)

 public:
  explicit VerticalTabIcon(const tabs::TabInterface* tab);
  VerticalTabIcon(const VerticalTabIcon&) = delete;
  VerticalTabIcon& operator=(const VerticalTabIcon&) = delete;
  ~VerticalTabIcon() override;

  void SetData(const tabs::TabInterface* tab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_ICON_H_
