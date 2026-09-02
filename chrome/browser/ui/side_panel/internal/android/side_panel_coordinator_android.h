// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_

#include <jni.h>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_deferred_entry_tracker.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_model_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_base.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntryWaiter;
class TabAndroid;

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
      const jni_zero::JavaRef<jobject>& java_coordinator,
      BrowserWindowInterface* browser);

  ~SidePanelCoordinatorAndroid() override;

  SidePanelCoordinatorAndroid(const SidePanelCoordinatorAndroid&) = delete;
  SidePanelCoordinatorAndroid& operator=(const SidePanelCoordinatorAndroid&) =
      delete;

  // Implements Java `SidePanelCoordinatorAndroidBridge.Natives`. These methods
  // are called from Java via JNI, see `SidePanelCoordinatorAndroidBridge.java`.
  void Init();
  void Destroy();
  void ClosePanel(bool suppress_animations);
  bool HasContentToShow(TabAndroid* tab);
  void OnPanelContainerUpdated(int old_width, int new_width);
  void OnPanelContentReplaced();
  void OnActiveChanged(bool active);
  void OnWillAutoClose();
  void OnWillAutoRestore();

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

  /////////////////////////////////////////////////////////////////
  //            Start of other public functions                  //
  /////////////////////////////////////////////////////////////////

  // Called when a tab is closed (destroyed).
  void OnTabClosed(TabAndroid* tab);

  // Called when the given `tab` is removed from this window's TabModel and
  // _has_ become the active tab of another window.
  void OnTabReparented(TabAndroid* tab);

  /////////////////////////////////////////////////////////////////
  //            End of other public functions                    //
  /////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
  //            Start of functions for testing                   //
  /////////////////////////////////////////////////////////////////

  // Enables/Disables deferred View replacement for testing.
  //
  // See the Java
  // `SidePanelContainerCoordinator#configDeferredViewReplacementForTesting`
  // for detailed documentation.
  void ConfigDeferredViewReplacementForTesting(bool enable);

  // See the Java
  // `SidePanelContainerCoordinator#simulateAutoCloseConditionForTesting`
  // for documentation.
  void SimulateAutoCloseConditionForTesting();

  // See the Java
  // `SidePanelContainerCoordinator#simulateAutoRestoreConditionForTesting`
  // for documentation.
  void SimulateAutoRestoreConditionForTesting();

  SidePanelState GetStateForTesting();
  int GetContainerWidthForTesting();
  SidePanelEntryWaiter* GetWaiterForTesting() { return waiter(); }
  const SidePanelDeferredEntryTracker& GetDeferredEntryTrackerForTesting()
      const {
    return deferred_entry_tracker_;
  }
  bool HasPendingReplacedEntryForTesting() const;

  /////////////////////////////////////////////////////////////////
  //            End of functions for testing                     //
  /////////////////////////////////////////////////////////////////

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
  void ClearCachedEntryViews(bool include_active_entry = false);

  UniqueKey GetCurrentKeyNonNull() const;
  SidePanelEntry* GetEntryForCurrentKeyNonNull() const;

  jni_zero::ScopedJavaLocalRef<jobject> java_coordinator() const;

  // Starts opening the side panel.
  // This should only be called when the side panel isn't currently shown.
  // `FinishOpeningPanel()` will be called when the side panel is fully opened.
  void StartOpeningPanel(
      SidePanelEntry* entry,
      const UniqueKey& unique_key,
      bool suppress_animations,
      std::unique_ptr<SidePanelNativeViewAndroid> native_view);

  // Completes the state updates for opening the side panel.
  void FinishOpeningPanel();

  // Starts closing the side panel.
  // This should only be called when the side panel is currently shown.
  // `FinishClosingPanel()` will be called when the side panel is fully closed.
  void StartClosingPanel(SidePanelEntryHideReason hide_reason,
                         bool suppress_animations);

  // Completes the state updates for closing the side panel.
  void FinishClosingPanel();

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

  // Flushes any async view detachments (e.g. from a tab switch) if the given
  // tab's active entry is currently pending replacement.
  void CompletePendingContentReplacementForTab(TabAndroid* tab);

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
  jni_zero::ScopedJavaGlobalWeakRef java_coordinator_;

  // Whether there is insufficient space to show the side panel.
  bool has_insufficient_space_ = false;

  SidePanelDeferredEntryTracker deferred_entry_tracker_{browser()};

  ui::ScopedUnownedUserData<SidePanelCoordinatorAndroid>
      scoped_unowned_user_data_;

  SidePanelTabModelObserver tab_model_observer_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_INTERNAL_ANDROID_SIDE_PANEL_COORDINATOR_ANDROID_H_
