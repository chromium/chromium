// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_observer_jni_bridge.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabModelObserverJniBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
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
    observer.OnActiveTabChanged(tab);
  }
}

void TabModelObserverJniBridge::WillCloseTab(JNIEnv* env, TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.WillCloseTab(tab);
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
    observer.OnTabAdded(tab, index);
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
}

void TabModelObserverJniBridge::OnTabCloseUndone(
    JNIEnv* env,
    const std::vector<TabAndroid*>& tabs) {
  for (auto& observer : model_observers_) {
    observer.OnTabCloseUndone(tabs);
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

void TabModelObserverJniBridge::TabRemoved(JNIEnv* env, TabAndroid* tab) {
  CHECK(tab);
  for (auto& observer : model_observers_) {
    observer.TabRemoved(tab);
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

DEFINE_JNI(TabModelObserverJniBridge)
