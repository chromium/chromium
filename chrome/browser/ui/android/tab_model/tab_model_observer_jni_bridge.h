// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_JNI_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_OBSERVER_JNI_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/tabs/tab_list_interface_observer.h"

class TabModel;

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

  void Destroy(JNIEnv* env);

  // The following functions are called by JNI.

  void DidSelectTab(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jtab,
                    int type,
                    int last_id);

  void WillCloseTab(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& jtab);

  void OnFinishingTabClosure(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& jtab,
                             int source);

  void OnFinishingMultipleTabClosure(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jtabs,
      bool canRestore);

  void WillAddTab(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jtab,
                  int type);

  void DidAddTab(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& jtab,
                 int type);

  void DidMoveTab(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jtab,
                  int new_index,
                  int cur_index);

  void TabPendingClosure(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jtab,
                         int source);

  void TabClosureUndone(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jtab);

  void OnTabCloseUndone(JNIEnv* env,
                        const base::android::JavaParamRef<jobjectArray>& jtabs);

  void TabClosureCommitted(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& jtab);

  void AllTabsPendingClosure(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jtabs);

  void AllTabsClosureCommitted(JNIEnv* env);

  void TabRemoved(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jtab);

  void AddObserver(TabModelObserver* observer);
  void AddTabListInterfaceObserver(TabListInterfaceObserver* observer);
  void RemoveObserver(TabModelObserver* observer);
  void RemoveTabListInterfaceObserver(TabListInterfaceObserver* observer);

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
