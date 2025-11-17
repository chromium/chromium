// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_

#include <jni.h>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

class SessionID;
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
  void Destroy(JNIEnv* env);

  TabModelJniBridge(const TabModelJniBridge&) = delete;
  TabModelJniBridge& operator=(const TabModelJniBridge&) = delete;

  ~TabModelJniBridge() override;

  void AssociateWithBrowserWindow(JNIEnv* env,
                                  long native_android_browser_window);
  void TabAddedToModel(JNIEnv* env, TabAndroid* tab);
  TabAndroid* DuplicateTab(JNIEnv* env, TabAndroid* tab);
  void MoveTabToWindowForTesting(JNIEnv* env,
                                 TabAndroid* tab,
                                 long android_browser_window_ptr,
                                 int new_index);
  void MoveTabGroupToWindowForTesting(JNIEnv* env,
                                      const base::Token& group_id,
                                      long android_browser_window_ptr,
                                      int new_index);
  void SetMuteSetting(JNIEnv* env, std::vector<TabAndroid*> tabs, bool mute);
  jint GetSessionIdForTesting(JNIEnv* env);
  chrome::android::ActivityType GetActivityTypeForTesting(JNIEnv* env);

  // TabModel::
  void AddTabListInterfaceObserver(TabListInterfaceObserver* observer) override;
  void RemoveTabListInterfaceObserver(
      TabListInterfaceObserver* observer) override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  tabs::TabInterface* GetActiveTab() override;
  content::WebContents* GetWebContentsAt(int index) const override;
  TabAndroid* GetTabAt(int index) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;

  void SetActiveIndex(int index) override;
  void ForceCloseAllTabs() override;
  void CloseTabAt(int index) override;

  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents,
                 bool select) override;
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override;

  content::WebContents* CreateNewTabForDevTools(const GURL& url,
                                                bool new_window) override;

  // Return true if we are currently restoring sessions asynchronously.
  bool IsSessionRestoreInProgress() const override;

  // Return true if this class is the currently selected in the correspond
  // tab model selector.
  bool IsActiveModel() const override;

  void AddObserver(TabModelObserver* observer) override;
  void RemoveObserver(TabModelObserver* observer) override;

  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete(JNIEnv* env);

  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override;

  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override;

  tabs::TabInterface* DuplicateTab(TabAndroid* tab);

  // TODO(crbug.com/415351293): Implement these.
  // TabListInterface implementation.
  tabs::TabInterface* OpenTab(const GURL& url, int index) override;
  void DiscardTab(tabs::TabHandle tab) override;
  tabs::TabInterface* DuplicateTab(tabs::TabHandle tab) override;
  tabs::TabInterface* GetTab(int index) override;
  int GetIndexOfTab(tabs::TabHandle tab) override;
  void HighlightTabs(tabs::TabHandle tab_to_activate,
                     const std::set<tabs::TabHandle>& tabs) override;
  void MoveTab(tabs::TabHandle tab, int index) override;
  void CloseTab(tabs::TabHandle tab) override;
  std::vector<tabs::TabInterface*> GetAllTabs() override;
  void PinTab(tabs::TabHandle tab) override;
  void UnpinTab(tabs::TabHandle tab) override;
  std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) override;
  void Ungroup(const std::set<tabs::TabHandle>& tabs) override;
  void MoveGroupTo(tab_groups::TabGroupId group_id, int index) override;
  void MoveTabToWindow(tabs::TabHandle tab,
                       SessionID destination_window_id,
                       int destination_index) override;
  void MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                            SessionID destination_window_id,
                            int destination_index) override;

  // Returns a corresponding Java Class object.
  static jclass GetClazz(JNIEnv* env);

  static TabModel* GetArchivedTabModelPtr();

 protected:
  jni_zero::ScopedJavaLocalRef<jobject> GetActivityForWindow(
      SessionID window_id);

  JavaObjectWeakGlobalRef java_object_;

  // The observer bridge. This exists as long as there are registered observers.
  // It corresponds to a Java observer that is registered with the corresponding
  // Java TabModelJniBridge.
  std::unique_ptr<TabModelObserverJniBridge> observer_bridge_;

  bool is_archived_tab_model_;
  // Cannot use a conventional member variable because this is initialized after
  // the constructor.
  std::unique_ptr<ui::ScopedUnownedUserData<TabModel>>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_JNI_BRIDGE_H_
