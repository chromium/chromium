// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_

#include <type_traits>

#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "ui/base/class_property.h"

class Browser;
class SidePanelRegistry;
class SidePanelContentProxy;

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class View;
}  // namespace views

class SidePanelUtil {
 public:
  using SidePanelContentState = ::SidePanelContentState;

  static void PopulateGlobalEntries(Browser* browser,
                                    SidePanelRegistry* global_registry);

  // Gets the SidePanelContentProxy for the provided view. If one does not
  // exist, this creates one indicating the view is available.
  static SidePanelContentProxy* GetSidePanelContentProxy(
      views::View* content_view);

  static actions::ActionItem* GetActionItem(Browser* browser,
                                            SidePanelEntryKey entry_key);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_UTIL_H_
