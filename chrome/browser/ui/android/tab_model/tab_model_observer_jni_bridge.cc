// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_observer_jni_bridge.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabModelObserverJniBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaObjectArrayReader;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Converts from a Java TabModel.TabLaunchType to a C++ TabModel::TabLaunchType.
TabModel::TabLaunchType GetTabLaunchType(JNIEnv* env, int type) {
  return static_cast<TabModel::TabLaunchType>(type);
}

// Converts from a Java TabModel.TabSelectionType to a C++
// TabModel::TabSelectionType.
TabModel::TabSelectionType GetTabSelectionType(JNIEnv* env, int type) {
  return static_cast<TabModel::TabSelectionType>(type);
}

}  // namespace

TabModelObserverJniBridge::TabModelObserverJniBridge(
    JNIEnv* env,
    const JavaRef<jobject>& tab_model) {
  // Create the Java object. This immediately adds it as an observer on the
  // corresponding TabModel.
  java_object_.Reset(Java_TabModelObserverJniBridge_create(
      env, reinterpret_cast<uintptr_t>(this), tab_model));
}

TabModelObserverJniBridge::~TabModelObserverJniBridge() {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelObserverJniBridge_detachFromTabModel(env, java_object_);
}

void TabModelObserverJniBridge::DidSelectTab(JNIEnv* env,
                                             const JavaParamRef<jobject>& jobj,
                                             const JavaParamRef<jobject>& jtab,
                                             int jtype,
                                             int last_id) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  TabModel::TabSelectionType type = GetTabSelectionType(env, jtype);
  for (auto& observer : observers_) {
    observer.DidSelectTab(tab, type);
  }
}

void TabModelObserverJniBridge::WillCloseTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  for (auto& observer : observers_) {
    observer.WillCloseTab(tab);
  }
}

void TabModelObserverJniBridge::OnFinishingTabClosure(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    int tab_id,
    bool incognito) {
  for (auto& observer : observers_) {
    observer.OnFinishingTabClosure(tab_id, incognito);
  }
}

void TabModelObserverJniBridge::OnFinishingMultipleTabClosure(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jobjectArray>& jtabs,
    bool canRestore) {
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> tabs =
      TabAndroid::GetAllNativeTabs(env,
                                   ScopedJavaLocalRef<jobjectArray>(jtabs));
  for (auto& observer : observers_) {
    observer.OnFinishingMultipleTabClosure(tabs, canRestore);
  }
}

void TabModelObserverJniBridge::WillAddTab(JNIEnv* env,
                                           const JavaParamRef<jobject>& jobj,
                                           const JavaParamRef<jobject>& jtab,
                                           int jtype) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  TabModel::TabLaunchType type = GetTabLaunchType(env, jtype);
  for (auto& observer : observers_) {
    observer.WillAddTab(tab, type);
  }
}

void TabModelObserverJniBridge::DidAddTab(JNIEnv* env,
                                          const JavaParamRef<jobject>& jobj,
                                          const JavaParamRef<jobject>& jtab,
                                          int jtype) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  TabModel::TabLaunchType type = GetTabLaunchType(env, jtype);
  for (auto& observer : observers_) {
    observer.DidAddTab(tab, type);
  }
}

void TabModelObserverJniBridge::DidMoveTab(JNIEnv* env,
                                           const JavaParamRef<jobject>& jobj,
                                           const JavaParamRef<jobject>& jtab,
                                           int new_index,
                                           int cur_index) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  for (auto& observer : observers_) {
    observer.DidMoveTab(tab, new_index, cur_index);
  }
}

void TabModelObserverJniBridge::TabPendingClosure(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  for (auto& observer : observers_) {
    observer.TabPendingClosure(tab);
  }
}

void TabModelObserverJniBridge::TabClosureUndone(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  for (auto& observer : observers_) {
    observer.TabClosureUndone(tab);
  }
}

void TabModelObserverJniBridge::TabClosureCommitted(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  for (auto& observer : observers_) {
    observer.TabClosureCommitted(tab);
  }
}

void TabModelObserverJniBridge::AllTabsPendingClosure(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jobjectArray>& jtabs) {
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> tabs =
      TabAndroid::GetAllNativeTabs(env,
                                   ScopedJavaLocalRef<jobjectArray>(jtabs));
  for (auto& observer : observers_) {
    observer.AllTabsPendingClosure(tabs);
  }
}

void TabModelObserverJniBridge::AllTabsClosureCommitted(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  for (auto& observer : observers_) {
    observer.AllTabsClosureCommitted();
  }
}

void TabModelObserverJniBridge::TabRemoved(JNIEnv* env,
                                           const JavaParamRef<jobject>& jobj,
                                           const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  CHECK(tab);
  for (auto& observer : observers_) {
    observer.TabRemoved(tab);
  }
}

void TabModelObserverJniBridge::AddObserver(TabModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabModelObserverJniBridge::RemoveObserver(TabModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
