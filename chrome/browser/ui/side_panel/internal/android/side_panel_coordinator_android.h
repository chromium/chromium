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
  void Init(JNIEnv* env);
  void Destroy(JNIEnv* env);
  bool HasContentToShow(JNIEnv* env);
  void OnPanelClosed(JNIEnv* env);
  void OnPanelOpened(JNIEnv* env);
  void OnPanelContentReplaced(JNIEnv* env);
  void OnWillAutoClose(JNIEnv* env);
  void OnWillAutoRestore(JNIEnv* env);

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

  UniqueKey GetCurrentKeyNonNull() const;
  SidePanelEntry* GetEntryForCurrentKeyNonNull() const;

  base::android::ScopedJavaLocalRef<jobject> java_coordinator() const;

  // Starts opening the side panel.
  // This should only be called when the side panel isn't currently shown.
  // `OnPanelOpened()` will be called when the side panel is fully opened.
  void StartOpeningPanel(
      SidePanelEntry* entry,
      const UniqueKey& unique_key,
      bool suppress_animations,
      std::unique_ptr<SidePanelNativeViewAndroid> native_view);

  // Starts replacing the entry shown in the side panel.
  // This should only be called when the side panel is already shown.
  // `OnPanelContentReplaced()` will be called when the side panel content is
  // fully replaced.
  void StartReplacingPanelContent(
      SidePanelEntry* new_entry,
      const UniqueKey& new_key,
      SidePanelOpenTrigger open_trigger,
      std::unique_ptr<SidePanelNativeViewAndroid> native_view);

  // Immediately ends all ongoing animations.
  //
  // This will also complete all state updates scheduled at the end of the
  // animations and ensure the side panel is in a stable state.
  void EndAnimations();

  bool CanShowEntryForKey(const UniqueKey& key) const;

  // The current state of the Side Panel.
  //
  // `kOpening`: set as soon as the side panel starts opening.
  // `kShown`: set after the side panel is fully opened.
  // `kClosing`: set as soon as the side panel starts closing.
  // `kClosed`: set after the side panel is fully closed.
  //
  // State changes always start from a _stable_ state (`kOpening` or `kShown`).
  // If a `Show()`/`Close()` request is received when the state is `kOpening` or
  // `kClosing`, we'll always reach a stable state first, then make state
  // changes for the new request.
  //
  // For example, if a `Close()` request is received when the state is
  // `kOpening`, we'll fast-forward the opening animation to its end so the
  // state becomes `kShown`, then change it to `kClosing`.
  //
  // In other words:
  // - `kOpening` will always be followed by `kShown`.
  // - `kClosing` will always be followed by `kClosed`.
  // - We don't allow state transitions from `kOpening` to `kClosing` or vice
  //   versa.
  //
  // This makes the state transitions easier to reason about and less
  // error-prone.
  SidePanelState state_ = SidePanelState::kClosed;

  // Tracks the `SidePanelEntryHideReason` for the current "close side panel" or
  // "replace side panel content" operation.
  std::optional<SidePanelEntryHideReason> pending_hide_reason_;

  // Tracks the entry that is being replaced since the "replace side panel
  // content" operation is async on the Java side.
  raw_ptr<SidePanelEntry> pending_replaced_entry_ = nullptr;

  // A weak reference to the Java `SidePanelCoordinatorAndroid`, which is
  // the sole owner of the C++ `SidePanelCoordinatorAndroid`.
  JavaObjectWeakGlobalRef java_coordinator_;

  // Whether there is insufficient space to show the side panel.
  bool has_insufficient_space_ = false;

  SidePanelDeferredEntryTracker deferred_entry_tracker_{browser()};

  std::optional<gfx::Rect> last_starting_bounds_;

  ui::ScopedUnownedUserData<SidePanelCoordinatorAndroid>
      scoped_unowned_user_data_;

  SidePanelTabListObserverAndroid tab_list_observer_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
