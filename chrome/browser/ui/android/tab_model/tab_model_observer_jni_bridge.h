// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_JNI_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_JNI_BRIDGE_H_

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

class TabAndroid;
class TabModel;

namespace base {
class Token;
}  // namespace base

// Bridges calls between the C++ and the Java TabModelObservers. Functions in
// this class do little more than translating between Java TabModelObserver
// notifications to native TabModelObserver notifications.
class TabModelObserverJniBridge {
 public:
  TabModelObserverJniBridge(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_tab_model,
      TabModel& tab_model);

  TabModelObserverJniBridge(const TabModelObserverJniBridge&) = delete;
  TabModelObserverJniBridge& operator=(const TabModelObserverJniBridge&) =
      delete;

  ~TabModelObserverJniBridge();

  // The following functions are called by JNI.

  void DidSelectTab(TabAndroid* tab);

  void WillCloseTabs(const std::vector<TabAndroid*>& tabs,
                     bool is_all_tabs,
                     bool allow_undo);

  void WillCloseTab(TabAndroid* tab);

  void DidRemoveTabForClosure(TabAndroid* tab);

  void OnFinishingTabClosure(TabAndroid* tab, int source);

  void OnFinishingMultipleTabClosure(const std::vector<TabAndroid*>& tabs,
                                     bool can_restore);

  void OnTabCloseCommitted(const std::vector<TabAndroid*>& tabs,
                           bool is_all_tabs,
                           bool can_restore,
                           int source);

  void DidAddTab(TabAndroid* tab, int type, int index);

  void DidMoveTab(TabAndroid* tab, int new_index, int cur_index);

  void OnTabClosePending(const std::vector<TabAndroid*>& tabs);

  void TabClosureUndone(TabAndroid* tab, int index);

  void OnTabCloseUndone(const std::vector<TabAndroid*>& tabs,
                        const std::vector<int>& indices);

  void OnTabsSelectionChanged();

  void TabClosureCommitted(TabAndroid* tab);

  void AllTabsClosureCommitted();

  void AllTabsAreClosing();

  void TabRemoved(TabAndroid* tab);

  void OnTabGroupCreated(base::Token group_id);

  void OnTabGroupRemoving(base::Token group_id);

  void OnTabGroupMoved(base::Token group_id);

  void OnTabGroupVisualsChanged(base::Token group_id);

  void OnWillActiveStateChange(TabModel* tab_model, bool active);
  void OnDidActiveStateChange(TabModel* tab_model, bool active);

  void AddObserver(TabModelObserver* observer);
  void AddTabListInterfaceObserver(TabListInterfaceObserver* observer);
  void RemoveObserver(TabModelObserver* observer);
  void RemoveTabListInterfaceObserver(TabListInterfaceObserver* observer);
  void NotifyShutdown();

  bool has_observers() const {
    return !model_observers_.empty() || !interface_observers_.empty();
  }

 private:
  // This object's Java counterpart. This objects controls its lifetime.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // TabModelObservers attached to this bridge.
  base::ObserverList<TabModelObserver>::Unchecked model_observers_;

  // The C++ TabModel. Owns this object, so is guaranteed to outlive it.
  base::raw_ref<TabModel> tab_model_;

  // TabListInterfaceObservers attached to this bridge.
  base::ObserverList<TabListInterfaceObserver> interface_observers_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_JNI_BRIDGE_H_
