// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_

#include <jni.h>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
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
                    const jni_zero::JavaRef<jobject>& jobj,
                    Profile* profile,
                    chrome::android::ActivityType activity_type,
                    bool is_archived_tab_model);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  TabModelJniBridge(const TabModelJniBridge&) = delete;
  TabModelJniBridge& operator=(const TabModelJniBridge&) = delete;

  ~TabModelJniBridge() override;

  // Called by JNI
  void TabAddedToModel(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       const base::android::JavaParamRef<jobject>& jtab);

  // TabModel::
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  content::WebContents* GetWebContentsAt(int index) const override;
  TabAndroid* GetTabAt(int index) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;

  void SetActiveIndex(int index) override;
  void CloseTabAt(int index) override;

  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents,
                 bool select) override;
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override;

  content::WebContents* CreateNewTabForDevTools(const GURL& url) override;

  // Return true if we are currently restoring sessions asynchronously.
  bool IsSessionRestoreInProgress() const override;

  // Return true if this class is the currently selected in the correspond
  // tab model selector.
  bool IsActiveModel() const override;

  // Return whether |tab| is grouped together with other Tab objects.
  static bool IsTabInTabGroup(TabAndroid* tab);

  void AddObserver(TabModelObserver* observer) override;
  void RemoveObserver(TabModelObserver* observer) override;

  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override;

  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override;

  // Returns a corresponding Java Class object.
  static jclass GetClazz(JNIEnv* env);

  static TabModel* GetArchivedTabModelPtr();

 protected:
  JavaObjectWeakGlobalRef java_object_;

  // The observer bridge. This exists as long as there are registered observers.
  // It corresponds to a Java observer that is registered with the corresponding
  // Java TabModelJniBridge.
  std::unique_ptr<TabModelObserverJniBridge> observer_bridge_;

  bool is_archived_tab_model_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_
