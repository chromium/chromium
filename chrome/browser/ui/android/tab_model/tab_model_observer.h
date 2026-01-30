// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"

class TabAndroid;

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

// An interface to be notified about changes to a TabModel. Notifications are
// routed to instances of this class via TabModelObserverJniBridge. See
// TabModelObserver.java for more details about individual notifications.
class TabModelObserver {
 public:
  TabModelObserver();

  TabModelObserver(const TabModelObserver&) = delete;
  TabModelObserver& operator=(const TabModelObserver&) = delete;

  virtual ~TabModelObserver();

  // Called when a |tab| is selected.
  virtual void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type);

  // Called when a |tab| starts closing.
  virtual void WillCloseTab(TabAndroid* tab);

  // Called after a tab has been removed due to closure. At this point the tab
  // is no longer in the TabModel.
  virtual void DidRemoveTabForClosure(TabAndroid* tab);

  // Called right before a |tab| has been destroyed.
  virtual void OnFinishingTabClosure(TabAndroid* tab,
                                     TabModel::TabClosingSource source);

  // Called right before all |tabs| are destroyed.
  virtual void OnFinishingMultipleTabClosure(
      const std::vector<TabAndroid*>& tabs,
      bool canRestore);

  // Called before a |tab| is added to the TabModel.
  virtual void WillAddTab(TabAndroid* tab, TabModel::TabLaunchType type);

  // Called after a |tab| has been added to the TabModel.
  virtual void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type);

  // Called after a |tab| has been moved from one position in the TabModel to
  // another.
  virtual void DidMoveTab(TabAndroid* tab, int new_index, int old_index);

  // Called when tabs are pending closure (ie, the user has just closed it, but
  // it can still be undone). At this point the tabs have been removed from the
  // TabModel.
  virtual void OnTabClosePending(const std::vector<TabAndroid*>& tabs,
                                 TabModel::TabClosingSource source);

  // Called when all |tabs| closure is undone.
  virtual void OnTabCloseUndone(const std::vector<TabAndroid*>& tabs);

  // Called when a |tab| closure is undone.
  virtual void TabClosureUndone(TabAndroid* tab);

  // Called when a |tab| closure is committed and can't be undone anymore.
  virtual void TabClosureCommitted(TabAndroid* tab);

  // Called when an all tabs closure has been committed and can't be undone
  // anymore.
  virtual void AllTabsClosureCommitted();

  // Called when tabs are being closed and there are no more tabs left.
  virtual void AllTabsAreClosing();

  // Called after a tab has been removed. At this point the tab is no longer in
  // the TabModel.
  virtual void TabRemoved(TabAndroid* tab);

  // Called after a tab group has been created.
  virtual void OnTabGroupCreated(tab_groups::TabGroupId group_id);

  // Called before a tab group will be removed.
  virtual void OnTabGroupRemoving(tab_groups::TabGroupId group_id);

  // Called after a tab group has been moved to a new position.
  virtual void OnTabGroupMoved(tab_groups::TabGroupId group_id, int old_index);

  // Called after a tab group's visual data has been changed.
  virtual void OnTabGroupVisualsChanged(tab_groups::TabGroupId group_id);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_H_
