// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_base.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Android implementation of `SidePanelUIBase`.
//
// It's named as `SidePanelCoordinatorAndroid` to be consistent with
// `SidePanelCoordinator`, which is the main `SidePanelUIBase` implementation on
// Windows, Mac, and Linux.
//
// The word "coordinator" does not refer to the "coordinator" component in
// Chrome Android MVC UI Architecture:
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_overview.md
class SidePanelCoordinatorAndroid : public SidePanelUIBase {
 public:
  DECLARE_USER_DATA(SidePanelCoordinatorAndroid);

  // Returns the `SidePanelCoordinatorAndroid` associated with the given
  // `browser`. A `nullptr` will be returned if `browser` is a `nullptr`.
  static SidePanelCoordinatorAndroid* From(BrowserWindowInterface* browser);

  SidePanelCoordinatorAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_coordinator,
      BrowserWindowInterface* browser);

  ~SidePanelCoordinatorAndroid() override;

  SidePanelCoordinatorAndroid(const SidePanelCoordinatorAndroid&) = delete;
  SidePanelCoordinatorAndroid& operator=(const SidePanelCoordinatorAndroid&) =
      delete;

  // Implements Java `SidePanelCoordinatorAndroid.Natives#destroy`.
  void Destroy(JNIEnv* env);

  // Implements `SidePanelUI`:
  void ShowFrom(SidePanelEntryKey entry_key,
                gfx::Rect starting_bounds_in_browser_coordinates) override;
  void Close(SidePanelEntry::PanelType panel_type,
             SidePanelEntryHideReason hide_reason,
             bool suppress_animations) override;
  void Toggle(SidePanelEntryKey key,
              SidePanelOpenTrigger open_trigger) override;
  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;
  void SetNoDelaysForTesting(bool no_delays_for_testing) override;


 protected:
  // Implements `SidePanelUIBase`:
  void Show(const UniqueKey& key,
            std::optional<SidePanelOpenTrigger> open_trigger,
            bool suppress_animations) override;
  void PopulateSidePanel(
      bool suppress_animations,
      const UniqueKey& unique_key,
      std::optional<SidePanelOpenTrigger> open_trigger,
      SidePanelEntry* entry,
      std::optional<SidePanelNativeView> content_view) override;
  void MaybeShowEntryOnTabStripModelChanged(
      SidePanelRegistry* old_contextual_registry,
      SidePanelRegistry* new_contextual_registry) override;

 private:
  class TabListObserver final : public TabListInterfaceObserver {
   public:
    TabListObserver(TabListInterface* tab_list,
                    SidePanelCoordinatorAndroid* coordinator);
    ~TabListObserver() override;

   private:
    // Implements `TabListInterfaceObserver`:
    void OnActiveTabChanged(TabListInterface& tab_list,
                            tabs::TabInterface* tab) override;
    void OnTabListDestroyed(TabListInterface& tab_list) override;

    const raw_ptr<SidePanelCoordinatorAndroid> coordinator_;

    // `TabHandle` for the current active tab.
    //
    // We need to cache the active tab handle because
    // `TabListInterfaceObserver:: OnActiveTabChanged()` does not provide the
    // previous active tab.
    //
    // Note:
    //
    // We shouldn't cache a `TabInterface*` as closing a tab can also trigger an
    // active tab change, and the cached `TabInterface*` will be invalid in that
    // case.
    //
    // We also shouldn't cache the "active tab index" as it may not refer to the
    // right tab after an active tab change.
    // For example, if we have tabs [tab_0, tab_1, tab_2], and tab_1 is the
    // active tab.
    // If we cache the active tab index, its initial value would be 1.
    // If tab_1 is closed, we will have [tab_0, tab_2], and the active tab will
    // become tab_2, but the new active tab index is still 1 (same as the
    // initial active tab index). In this case, it's hard to tell if the active
    // tab has changed or not.
    tabs::TabHandle active_tab_handle_;

    base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
        observation_{this};
  };

  base::android::ScopedJavaLocalRef<jobject> java_coordinator() const;

  // A weak reference to the Java `SidePanelCoordinatorAndroid`, which is
  // the sole owner of the C++ `SidePanelCoordinatorAndroid`.
  JavaObjectWeakGlobalRef java_coordinator_;

  ui::ScopedUnownedUserData<SidePanelCoordinatorAndroid>
      scoped_unowned_user_data_;

  TabListObserver tab_list_observer_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
