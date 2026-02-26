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
      env, reinterpret_cast<uintptr_t>(this), java_tab_model));
}

TabModelObserverJniBridge::~TabModelObserverJniBridge() {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelObserverJniBridge_detachFromTabModel(env, java_object_);
}

void TabModelObserverJniBridge::DidSelectTab(JNIEnv* env,
                                             TabAndroid* tab,
                                             int type,
                                             int last_id) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidSelectTab(tab, static_cast<TabModel::TabSelectionType>(type));
  }
  for (auto& observer : interface_observers_) {
    observer.OnActiveTabChanged(*tab_model_, tab);
  }
}

void TabModelObserverJniBridge::WillCloseTab(JNIEnv* env, TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.WillCloseTab(tab);
  }
}

void TabModelObserverJniBridge::DidRemoveTabForClosure(JNIEnv* env,
                                                       TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidRemoveTabForClosure(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabRemoved(*tab_model_, tab, TabRemovedReason::kDeleted);
  }
}

void TabModelObserverJniBridge::OnFinishingTabClosure(JNIEnv* env,
                                                      TabAndroid* tab,
                                                      int source) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.OnFinishingTabClosure(
        tab, static_cast<TabModel::TabClosingSource>(source));
  }
}

void TabModelObserverJniBridge::OnFinishingMultipleTabClosure(
    JNIEnv* env,
    const std::vector<TabAndroid*>& tabs,
    bool can_restore) {
  for (auto& observer : model_observers_) {
    observer.OnFinishingMultipleTabClosure(tabs, can_restore);
  }
}

void TabModelObserverJniBridge::WillAddTab(JNIEnv* env,
                                           TabAndroid* tab,
                                           int type) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.WillAddTab(tab, static_cast<TabModel::TabLaunchType>(type));
  }
}

void TabModelObserverJniBridge::DidAddTab(JNIEnv* env,
                                          TabAndroid* tab,
                                          int type) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidAddTab(tab, static_cast<TabModel::TabLaunchType>(type));
  }

  int index = tab_model_->GetIndexOfTab(tab->GetHandle());
  for (auto& observer : interface_observers_) {
    observer.OnTabAdded(*tab_model_, tab, index);
  }
}

void TabModelObserverJniBridge::DidMoveTab(JNIEnv* env,
                                           TabAndroid* tab,
                                           int new_index,
                                           int cur_index) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.DidMoveTab(tab, new_index, cur_index);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabMoved(*tab_model_, tab, cur_index, new_index);
  }
}

void TabModelObserverJniBridge::OnTabClosePending(
    JNIEnv* env,
    const std::vector<TabAndroid*>& tabs,
    int source) {
  for (auto& observer : model_observers_) {
    observer.OnTabClosePending(tabs,
                               static_cast<TabModel::TabClosingSource>(source));
  }
}

void TabModelObserverJniBridge::TabClosureUndone(JNIEnv* env, TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabClosureUndone(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabAdded(*tab_model_, tab,
                        tab_model_->GetIndexOfTab(tab->GetHandle()));
  }
}

void TabModelObserverJniBridge::OnTabCloseUndone(
    JNIEnv* env,
    const std::vector<TabAndroid*>& tabs) {
  for (auto& observer : model_observers_) {
    observer.OnTabCloseUndone(tabs);
  }
  for (auto& observer : interface_observers_) {
    for (TabAndroid* tab : tabs) {
      observer.OnTabAdded(*tab_model_, tab,
                          tab_model_->GetIndexOfTab(tab->GetHandle()));
    }
  }
}

void TabModelObserverJniBridge::TabClosureCommitted(JNIEnv* env,
                                                    TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabClosureCommitted(tab);
  }
}

void TabModelObserverJniBridge::AllTabsClosureCommitted(JNIEnv* env) {
  for (auto& observer : model_observers_) {
    observer.AllTabsClosureCommitted();
  }
}

void TabModelObserverJniBridge::AllTabsAreClosing(JNIEnv* env) {
  for (auto& observer : model_observers_) {
    observer.AllTabsAreClosing();
  }
  for (auto& observer : interface_observers_) {
    observer.OnAllTabsAreClosing(*tab_model_);
  }
}

void TabModelObserverJniBridge::TabRemoved(JNIEnv* env, TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabRemoved(tab);
  }
  for (auto& observer : interface_observers_) {
    observer.OnTabRemoved(*tab_model_, tab,
                          TabRemovedReason::kInsertedIntoOtherTabStrip);
  }
}

void TabModelObserverJniBridge::OnTabGroupCreated(JNIEnv* env,
                                                  base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupCreated(tab_group_id);
  }
}

void TabModelObserverJniBridge::OnTabGroupRemoving(JNIEnv* env,
                                                   base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupRemoving(tab_group_id);
  }
}

void TabModelObserverJniBridge::OnTabGroupMoved(JNIEnv* env,
                                                base::Token group_id,
                                                int old_index) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupMoved(tab_group_id, old_index);
  }
}

void TabModelObserverJniBridge::OnTabGroupVisualsChanged(JNIEnv* env,
                                                         base::Token group_id) {
  auto tab_group_id = tab_groups::TabGroupId::FromRawToken(group_id);
  CHECK(!tab_group_id.is_empty());
  for (auto& observer : model_observers_) {
    observer.OnTabGroupVisualsChanged(tab_group_id);
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
  for (auto& observer : interface_observers_) {
    observer.OnTabListDestroyed(*tab_model_);
  }
}

DEFINE_JNI(TabModelObserverJniBridge)
