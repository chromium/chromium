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
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_deferred_entry_tracker.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_list_observer_android.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_base.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntryWaiter;

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

  // Implements Java `SidePanelCoordinatorAndroid.Natives`. These methods are
  // called from Java via JNI, see `SidePanelCoordinatorAndroidImpl.java`.
  void Destroy(JNIEnv* env);
  void NotifyCloseAnimationFinished(JNIEnv* env);
  void NotifyOpenAnimationFinished(JNIEnv* env);
  void OnWindowResized(JNIEnv* env, bool can_show_side_panel);

  // Implements `SidePanelUI`:
  void ShowFrom(SidePanelEntryKey entry_key,
                gfx::Rect starting_bounds_in_browser_coordinates) override;
  void Close(SidePanelEntryHideReason hide_reason,
             bool suppress_animations) override;
  void Toggle(SidePanelEntryKey key,
              SidePanelOpenTrigger open_trigger) override;
  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;
  void SetNoDelaysForTesting(bool no_delays_for_testing) override;

  // Other public functions:
  void ClearDeferredEntryForTab(const tabs::TabHandle& tab_handle);
  // Called when a tab is detached from this window's tab strip for reparenting
  // into another window.
  void OnTabReparented(tabs::TabInterface* tab);
  bool IsClosing() const { return state_ == SidePanelState::kClosing; }
  bool ShouldClose() const {
    return state_ == SidePanelState::kShown ||
           state_ == SidePanelState::kOpening;
  }

  // Functions for testing:
  SidePanelState GetStateForTesting();
  int GetContainerWidthForTesting();
  SidePanelEntryWaiter* GetWaiterForTesting() { return waiter(); }
  const SidePanelDeferredEntryTracker& GetDeferredEntryTrackerForTesting()
      const {
    return deferred_entry_tracker_;
  }

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
  // Delegates to `SidePanelRegistry::ClearCachedEntryViews` in all
  // `SidePanelRegistry` instances accessible from this class, including
  // the window-scoped registry and all contextual (tab-scoped) registries.
  void ClearCachedEntryViews();

  base::android::ScopedJavaLocalRef<jobject> java_coordinator() const;

  // Handles the JNI call to Java to populate the side panel UI.
  void PopulateJavaSidePanel(const base::android::JavaRef<jobject>& view,
                             bool suppress_animations);

  bool CanShowEntryForKey(const UniqueKey& key) const;

  // The current state of the Side Panel.
  //
  // A SidePanelEntry is considered current/active as soon as the state becomes
  // `kOpening` and the entry will become inactive when the state becomes
  // `kClosed`.
  SidePanelState state_ = SidePanelState::kClosed;

  // Tracks the hide reason for the current close operation.
  std::optional<SidePanelEntryHideReason> pending_hide_reason_;

  // A weak reference to the Java `SidePanelCoordinatorAndroid`, which is
  // the sole owner of the C++ `SidePanelCoordinatorAndroid`.
  JavaObjectWeakGlobalRef java_coordinator_;

  // Tracks the previous entry that is being replaced, which we keep in state
  // until animations have completed and it is fully replaced.
  raw_ptr<SidePanelEntry> pending_replaced_entry_ = nullptr;

  // Whether the window is too small to show a side panel.
  bool is_window_too_small_ = false;

  SidePanelDeferredEntryTracker deferred_entry_tracker_{browser()};

  std::optional<gfx::Rect> last_starting_bounds_;

  ui::ScopedUnownedUserData<SidePanelCoordinatorAndroid>
      scoped_unowned_user_data_;

  SidePanelTabListObserverAndroid tab_list_observer_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
