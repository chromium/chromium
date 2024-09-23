// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"

class TabAndroid;

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

  // Called right before a |tab| has been destroyed.
  virtual void OnFinishingTabClosure(int tab_id, bool incognito);

  // Called right before all |tabs| are destroyed.
  virtual void OnFinishingMultipleTabClosure(
      const std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs,
      bool canRestore);

  // Called before a |tab| is added to the TabModel.
  virtual void WillAddTab(TabAndroid* tab, TabModel::TabLaunchType type);

  // Called after a |tab| has been added to the TabModel.
  virtual void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type);

  // Called after a |tab| has been moved from one position in the TabModel to
  // another.
  virtual void DidMoveTab(TabAndroid* tab, int new_index, int old_index);

  // Called when a tab is pending closure (ie, the user has just closed it, but
  // it can still be undone). At this point the |tab| has been removed from the
  // TabModel.
  virtual void TabPendingClosure(TabAndroid* tab);

  // Called when a |tab| closure is undone.
  virtual void TabClosureUndone(TabAndroid* tab);

  // Called when a |tab| closure is committed and can't be undone anymore.
  virtual void TabClosureCommitted(TabAndroid* tab);

  // Called when all |tabs| are pending closure.
  virtual void AllTabsPendingClosure(
      const std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs);

  // Called when an all tabs closure has been committed and can't be undone
  // anymore.
  virtual void AllTabsClosureCommitted();

  // Called after a tab has been removed. At this point the tab is no longer in
  // the TabModel.
  virtual void TabRemoved(TabAndroid* tab);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_H_
