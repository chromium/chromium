// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_observer_jni_bridge.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/token_android.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabModelObserverJniBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

TabModelObserverJniBridge::TabModelObserverJniBridge(
    JNIEnv* env,
    const JavaRef<jobject>& java_tab_model,
    TabModel& tab_model)
    : tab_model_(tab_model) {
  // Create the Java object. This immediately adds it as an observer on the
  // corresponding TabModel.
  java_object_.Reset(Java_TabModelObserverJniBridge_create(
      env, jni_zero::MakeRaw(this), java_tab_model));
}

TabModelObserverJniBridge::~TabModelObserverJniBridge() {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelObserverJniBridge_detachFromTabModel(env, java_object_);
}

void TabModelObserverJniBridge::DidSelectTab(TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidSelectTab(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnActiveTabChanged(*tab_model_, tab);
  }
}

void TabModelObserverJniBridge::WillCloseTabs(
    const std::vector<TabAndroid*>& tabs,
    bool is_all_tabs,
    bool allow_undo) {
  for (auto& observer : model_observers_) {
    observer.WillCloseTabs(tabs, is_all_tabs, allow_undo);
  }
  if (is_all_tabs) {
    for (auto& observer : interface_observers_) {
      observer.OnAllTabsAreClosing(*tab_model_);
    }
  }
}

void TabModelObserverJniBridge::WillCloseTab(TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.WillCloseTab(tab);
  }
}

void TabModelObserverJniBridge::DidRemoveTabForClosure(TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidRemoveTabForClosure(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabRemoved(*tab_model_, tab, TabRemovedReason::kDeleted);
  }
}

void TabModelObserverJniBridge::OnFinishingTabClosure(TabAndroid* tab,
                                                      int source) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.OnFinishingTabClosure(
        tab, static_cast<TabModel::TabClosingSource>(source));
  }
}

void TabModelObserverJniBridge::OnFinishingMultipleTabClosure(
    const std::vector<TabAndroid*>& tabs,
    bool can_restore) {
  for (auto& observer : model_observers_) {
    observer.OnFinishingMultipleTabClosure(tabs, can_restore);
  }
}

void TabModelObserverJniBridge::OnTabCloseCommitted(
    const std::vector<TabAndroid*>& tabs,
    bool is_all_tabs,
    bool can_restore,
    int source) {
  for (auto& observer : model_observers_) {
    observer.OnTabCloseCommitted(
        tabs, is_all_tabs, can_restore,
        static_cast<TabModel::TabClosingSource>(source));
  }
}

void TabModelObserverJniBridge::DidAddTab(TabAndroid* tab,
                                          int type,
                                          int index) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidAddTab(tab, static_cast<TabModel::TabLaunchType>(type));
  }

  for (auto& observer : interface_observers_) {
    observer.OnTabAdded(*tab_model_, tab, index);
  }
}

void TabModelObserverJniBridge::DidMoveTab(TabAndroid* tab,
                                           int new_index,
                                           int cur_index) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidMoveTab(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabMoved(*tab_model_, tab, cur_index, new_index);
  }
}

void TabModelObserverJniBridge::OnTabClosePending(
    const std::vector<TabAndroid*>& tabs) {
  for (auto& observer : model_observers_) {
    observer.OnTabClosePending(tabs);
  }
}

void TabModelObserverJniBridge::TabClosureUndone(TabAndroid* tab, int index) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabClosureUndone(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabAdded(*tab_model_, tab, index);
  }
}

void TabModelObserverJniBridge::OnTabCloseUndone(
    const std::vector<TabAndroid*>& tabs,
    const std::vector<int>& indices) {
  CHECK_EQ(tabs.size(), indices.size());
  for (auto& observer : model_observers_) {
    observer.OnTabCloseUndone(tabs);
  }
  for (auto& observer : interface_observers_) {
    for (size_t i = 0; i < tabs.size(); ++i) {
      observer.OnTabAdded(*tab_model_, tabs[i], indices[i]);
    }
  }
}

void TabModelObserverJniBridge::OnTabsSelectionChanged() {
  if (!interface_observers_.empty()) {
    std::set<tabs::TabInterface*> highlighted_tabs;
    const int count = tab_model_->GetTabCount();

    for (int i = 0; i < count; ++i) {
      TabAndroid* tab = tab_model_->GetTabAt(i);
      if (tab->IsSelected()) {
        highlighted_tabs.insert(tab);
      }
    }

    for (auto& observer : interface_observers_) {
      observer.OnHighlightedTabsChanged(*tab_model_, highlighted_tabs);
    }
  }

  for (auto& observer : model_observers_) {
    observer.OnTabsSelectionsChanged();
  }
}

void TabModelObserverJniBridge::TabClosureCommitted(TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabClosureCommitted(tab);
  }
}

void TabModelObserverJniBridge::AllTabsClosureCommitted() {
  for (auto& observer : model_observers_) {
    observer.AllTabsClosureCommitted();
  }
}

void TabModelObserverJniBridge::AllTabsAreClosing() {
  for (auto& observer : model_observers_) {
    observer.AllTabsAreClosing();
  }
  for (auto& observer : interface_observers_) {
    observer.OnAllTabsAreClosing(*tab_model_);
  }
}

void TabModelObserverJniBridge::TabRemoved(TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabRemoved(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabRemoved(*tab_model_, tab,
                          TabRemovedReason::kInsertedIntoOtherTabStrip);
  }
}

void TabModelObserverJniBridge::OnTabGroupCreated(base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupCreated(tab_group_id);
  }
}

void TabModelObserverJniBridge::OnTabGroupRemoving(base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupRemoving(tab_group_id);
  }
}

void TabModelObserverJniBridge::OnTabGroupMoved(base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupMoved(tab_group_id);
  }
}

void TabModelObserverJniBridge::OnTabGroupVisualsChanged(base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupVisualsChanged(tab_group_id);
  }
}

void TabModelObserverJniBridge::OnWillActiveStateChange(TabModel* tab_model,
                                                        bool active) {
  for (auto& observer : model_observers_) {
    observer.OnWillActiveStateChange(*tab_model, active);
  }
}

void TabModelObserverJniBridge::OnDidActiveStateChange(TabModel* tab_model,
                                                       bool active) {
  for (auto& observer : model_observers_) {
    observer.OnDidActiveStateChange(*tab_model, active);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabListActiveChanged(*tab_model, active);
  }
}

void TabModelObserverJniBridge::AddObserver(TabModelObserver* observer) {
  model_observers_.AddObserver(observer);
}

void TabModelObserverJniBridge::AddTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  interface_observers_.AddObserver(observer);
}

void TabModelObserverJniBridge::RemoveObserver(TabModelObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

void TabModelObserverJniBridge::RemoveTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  interface_observers_.RemoveObserver(observer);
}

void TabModelObserverJniBridge::NotifyShutdown() {
  for (auto& observer : model_observers_) {
    observer.OnTabModelDestroyed(*tab_model_);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabListDestroyed(*tab_model_);
  }
}

DEFINE_JNI(TabModelObserverJniBridge)
