// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_CONTAINER_INTERNAL_ANDROID_DEV_SIDE_PANEL_TAB_SCOPED_DEV_FEATURE_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_CONTAINER_INTERNAL_ANDROID_DEV_SIDE_PANEL_TAB_SCOPED_DEV_FEATURE_H_

#include "chrome/browser/ui/side_panel/side_panel_registry.h"

namespace tabs {
class TabInterface;
}

class SidePanelTabScopedDevFeature {
 public:
  SidePanelTabScopedDevFeature(tabs::TabInterface* tab,
                               SidePanelRegistry* registry);
  ~SidePanelTabScopedDevFeature();

  SidePanelTabScopedDevFeature(const SidePanelTabScopedDevFeature&) = delete;
  SidePanelTabScopedDevFeature& operator=(const SidePanelTabScopedDevFeature&) =
      delete;

 private:
  tabs::TabInterface* const tab_;
  SidePanelRegistry* const registry_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_CONTAINER_INTERNAL_ANDROID_DEV_SIDE_PANEL_TAB_SCOPED_DEV_FEATURE_H_
