// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_CONTROLLER_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace tab_groups {
class TabGroupId;
}

class TabGroupController {
 public:
  virtual void CreateTabGroup(const tab_groups::TabGroupId& group) = 0;
  virtual void OpenTabGroupEditor(const tab_groups::TabGroupId& group) = 0;
  virtual void ChangeTabGroupContents(const tab_groups::TabGroupId& group) = 0;
  virtual void ChangeTabGroupVisuals(
      const tab_groups::TabGroupId& group,
      const TabGroupChange::VisualsChange& visuals) = 0;
  virtual void MoveTabGroup(const tab_groups::TabGroupId& group) = 0;
  virtual void CloseTabGroup(const tab_groups::TabGroupId& group) = 0;
  virtual std::u16string GetTitleAt(int index) const = 0;

  // Methods from TabStripModel that are exposed to TabGroup.
  virtual std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const = 0;
  virtual int GetTabCount() const = 0;

 protected:
  virtual ~TabGroupController() {}
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_CONTROLLER_H_
