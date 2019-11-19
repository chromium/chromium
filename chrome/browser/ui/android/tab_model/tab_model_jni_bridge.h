// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"

class TabAndroid;
class TabModelObserverJniBridge;

namespace content {
class WebContents;
}

// Bridges calls between the C++ and the Java TabModels. Functions in this
// class should do little more than make calls out to the Java TabModel, which
// is what actually stores Tabs.
class TabModelJniBridge : public TabModel {
 public:
  TabModelJniBridge(JNIEnv* env,
                    jobject obj,
                    bool is_incognito,
                    bool is_tabbed_activity);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  ~TabModelJniBridge() override;

  // Called by JNI
  base::android::ScopedJavaLocalRef<jobject> GetProfileAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void TabAddedToModel(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& jtab);

  // TabModel::
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  content::WebContents* GetWebContentsAt(int index) const override;
  TabAndroid* GetTabAt(int index) const override;

  void SetActiveIndex(int index) override;
  void CloseTabAt(int index) override;

  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents) override;
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override;

  content::WebContents* CreateNewTabForDevTools(const GURL& url) override;

  // Return true if we are currently restoring sessions asynchronously.
  bool IsSessionRestoreInProgress() const override;

  // Return true if this class is the currently selected in the correspond
  // tab model selector.
  bool IsCurrentModel() const override;

  void AddObserver(TabModelObserver* observer) override;
  void RemoveObserver(TabModelObserver* observer) override;

  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 protected:
  JavaObjectWeakGlobalRef java_object_;

  // The observer bridge. This exists as long as there are registered observers.
  // It corresponds to a Java observer that is registered with the corresponding
  // Java TabModelJniBridge.
  std::unique_ptr<TabModelObserverJniBridge> observer_bridge_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabModelJniBridge);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_
