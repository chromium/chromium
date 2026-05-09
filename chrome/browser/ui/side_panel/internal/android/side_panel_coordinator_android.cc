// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelCoordinatorAndroidImpl_jni.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_list_observer_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_enums_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_metrics.h"

#define LOG_TAG "SidePanelCoordinatorAndroid"
#define SPLOG(message)                                     \
  if (base::FeatureList::IsEnabled(                        \
          chrome::android::kEnableAndroidSidePanelLogs)) { \
    LOG(ERROR) << LOG_TAG << ": " << message;              \
  }

namespace {
constexpr int kInvalidCoordinate = -1;
const gfx::Rect kNoBounds(kInvalidCoordinate,
                          kInvalidCoordinate,
                          kInvalidCoordinate,
                          kInvalidCoordinate);
}  // namespace

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

DEFINE_USER_DATA(SidePanelCoordinatorAndroid);

// static
SidePanelCoordinatorAndroid* SidePanelCoordinatorAndroid::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

SidePanelCoordinatorAndroid::SidePanelCoordinatorAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& java_coordinator,
    BrowserWindowInterface* browser)
    : SidePanelUIBase(browser),
      java_coordinator_(env, java_coordinator),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this),
      tab_list_observer_(TabListInterface::From(browser), this) {
  SPLOG("SidePanelCoordinatorAndroid Constructor - browser: " << browser);
}

SidePanelCoordinatorAndroid::~SidePanelCoordinatorAndroid() {
  SPLOG("SidePanelCoordinatorAndroid Destructor");
  Java_SidePanelCoordinatorAndroidImpl_clearNativePtr(AttachCurrentThread(),
                                                      java_coordinator());
}

void SidePanelCoordinatorAndroid::Destroy(JNIEnv* env) {
  SPLOG("Destroy");
  delete this;
}

void SidePanelCoordinatorAndroid::NotifyOpenAnimationFinished(
    JNIEnv* env,
    SidePanelType panel_type) {
  SPLOG("NotifyOpenAnimationFinished - panel_type: " << ToString(panel_type));

  // We need to make the round trip to Java even when animations are suppressed,
  // which can happen when the panel is already shown and being replaced.
  CHECK(state_ == SidePanelState::kOpening || state_ == SidePanelState::kShown)
      << "Should only receive open animation finished callback when side "
         "panel is opening or being replaced (shown).";

  // We should have a key and entry whether we are opening or shown.
  std::optional<UniqueKey> key = current_key();
  CHECK(key) << "Current key should exist when side panel is opening or shown.";

  SidePanelEntry* entry = GetEntryForUniqueKey(*key);
  CHECK(entry)
      << "Current entry should exist when side panel is opening or shown.";

  if (pending_replaced_entry_) {
    pending_replaced_entry_->OnEntryHidden();
    pending_replaced_entry_->OnEntryHiddenWithReason(pending_hide_reason_);
    pending_replaced_entry_ = nullptr;
    // TODO(crbug.com/494001968): Set `pending_hide_reason_` as
    // std::nullopt or kUnknown
  }

  state_ = SidePanelState::kShown;
}

void SidePanelCoordinatorAndroid::NotifyCloseAnimationFinished(
    JNIEnv* env,
    SidePanelType panel_type) {
  SPLOG("NotifyCloseAnimationFinished - panel_type: " << ToString(panel_type));

  CHECK(IsClosing())
      << "Should only receive close animation finished callback when side "
         "panel is closing.";

  std::optional<UniqueKey> key = current_key();
  CHECK(key) << "Current key should exist when side panel animation finishes.";

  SidePanelEntry* entry = GetEntryForUniqueKey(*key);
  CHECK(entry)
      << "Current entry should still exist when side panel is closing.";

  SetCurrentKey(/*new_key=*/std::nullopt);

  // Now that the animation has completed, we can update our local state to be
  // closed, and trigger the entry hidden callbacks.
  entry->OnEntryHidden();
  entry->OnEntryHiddenWithReason(pending_hide_reason_);

  // We need to explicitly reset the active entry for the "close side panel"
  // case.
  //
  // Context as of Apr 15, 2026:
  //
  // `SidePanelRegistry` observes all its `SidePanelEntries` via
  // `SidePanelEntryObserver`.
  //
  // For the "open side panel" case, the active entry is set via
  // `SidePanelEntry::OnEntryShown()` -> `SidePanelRegistry::OnEntryShown()`.
  //
  // For the "close side panel" case, `SidePanelRegistry` doesn't implement
  // `SidePanelEntryObserver::OnEntryHidden()` or
  // `SidePanelEntryObserver::OnEntryHiddenWithReason()`, so
  // `SidePanelEntry::OnEntryHidden()` and
  // `SidePanelEntry::OnEntryHiddenWithReason()` can't reset the active entry.
  //
  // TODO(crbug.com/503113522): Consider having `SidePanelRegistry` _reset_ the
  // active entry so it's consistent with how the active entry is _set_.
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntry();
  }
  if (auto* window_registry = SidePanelRegistry::From(browser())) {
    window_registry->ResetActiveEntry();
  }
  ClearCachedEntryViews();

  SidePanelMetrics::RecordSidePanelClosed(opened_timestamp());

  state_ = SidePanelState::kClosed;
}

void SidePanelCoordinatorAndroid::ShowFrom(
    SidePanelEntryKey entry_key,
    gfx::Rect starting_bounds_in_browser_coordinates) {
  SPLOG("ShowFrom - entry_key: "
        << entry_key.ToString() << ", starting_bounds: "
        << starting_bounds_in_browser_coordinates.ToString());
  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(entry_key);
  CHECK(unique_key.has_value())
      << "Entry should exist for the given key: " << entry_key.ToString();
  last_starting_bounds_ = starting_bounds_in_browser_coordinates;
  SidePanelUI::Show(entry_key);
}

void SidePanelCoordinatorAndroid::Close(SidePanelEntryHideReason hide_reason,
                                        bool suppress_animations) {
  SPLOG("Close - hide_reason: " << ToString(hide_reason)
                                << ", suppress_animations: "
                                << suppress_animations);
  CHECK(state_ == SidePanelState::kOpening || state_ == SidePanelState::kShown)
      << "Close calls should only occur for opening or shown side panels. "
         "Current state: "
      << ToString(state_);

  // Stop any pending load.
  waiter()->ResetLoadingEntryIfNecessary();

  // If a ShowFrom() was pending, clear the starting bounds.
  last_starting_bounds_.reset();

  if (!IsSidePanelShowing()) {
    return;
  }

  std::optional<UniqueKey> key = current_key();
  CHECK(key) << "Current key should exist when side panel is showing.";

  SidePanelEntry* entry = GetEntryForUniqueKey(*key);
  CHECK(entry) << "SidePanelEntry should exist when side panel is showing.";

  // TODO(crbug.com/494001968): Handle kOpening state case.

  // When we start to close, we will update state to closing, and send a remove
  // request to Java, which will handle animations and call back when done.
  state_ = SidePanelState::kClosing;
  pending_hide_reason_ = hide_reason;

  // TOOD(crbug.com/494001968): Handle suppressed animations case.
  entry->OnEntryWillHide(pending_hide_reason_);
  Java_SidePanelCoordinatorAndroidImpl_removeContentAndClose(
      AttachCurrentThread(), java_coordinator(), suppress_animations);
}

void SidePanelCoordinatorAndroid::OnWindowResized(JNIEnv* env,
                                                  bool should_show_side_panel) {
  SPLOG("OnWindowResized - should_show_side_panel: " << should_show_side_panel);

  if (is_window_too_small_ == !should_show_side_panel) {
    return;
  }

  is_window_too_small_ = !should_show_side_panel;

  if (should_show_side_panel) {
    CHECK(!IsSidePanelShowing() || IsClosing())
        << "Side panel should not be visible when the window changes from "
           "being too small to being large enough.";
    if (key_to_restore_after_window_resize_ &&
        CanShowEntryForKey(*key_to_restore_after_window_resize_)) {
      // TODO(crbug.com/507911289): Revisit animations.
      Show(*key_to_restore_after_window_resize_, std::nullopt,
           /*suppress_animations=*/true);
    } else {
      // If we didn't restore a cached key (e.g. because we are on a different
      // tab), check if the current tab has an active entry that was blocked.
      SidePanelRegistry* contextual_registry = GetActiveContextualRegistry();
      std::optional<SidePanelEntry*> active_contextual_entry =
          contextual_registry ? contextual_registry->GetActiveEntry()
                              : std::nullopt;
      if (active_contextual_entry) {
        Show(UniqueKey{contextual_registry->GetTabInterface().GetHandle(),
                       (*active_contextual_entry)->key()},
             std::nullopt, /*suppress_animations=*/true);
      }
    }
  } else {
    if (IsSidePanelShowing() && !IsClosing()) {
      std::optional<UniqueKey> current_key = this->current_key();
      CHECK(current_key);
      key_to_restore_after_window_resize_ = *current_key;
      Close(SidePanelEntryHideReason::kWindowResized,
            /*suppress_animations=*/true);
    }
  }
}

void SidePanelCoordinatorAndroid::Toggle(SidePanelEntryKey key,
                                         SidePanelOpenTrigger open_trigger) {
  SPLOG("Toggle - key: " << key.ToString()
                         << ", open_trigger: " << ToString(open_trigger));

  // If an entry is already showing in the sidepanel, or is currently loading,
  // the sidepanel should be closed.
  SidePanelEntry* entry = GetActiveContextualEntryForKey(key);
  if (!entry) {
    entry = SidePanelRegistry::From(browser())->GetEntryForKey(key);
  }

  if (entry && ShouldClose() && IsSidePanelShowing() &&
      IsSidePanelEntryShowing(key)) {
    Close(SidePanelEntryHideReason::kSidePanelClosed,
          /*suppress_animations=*/false);
    return;
  }

  std::optional<UniqueKey> unique_key = GetUniqueKeyForKey(key);
  if (unique_key.has_value()) {
    Show(unique_key.value(), open_trigger, /*suppress_animations=*/false);
  }
}

content::WebContents*
SidePanelCoordinatorAndroid::GetWebContentsForTest(  // IN-TEST
    SidePanelEntryId id) {
  // TODO(crbug.com/494001633): Implement this.
  return nullptr;
}

void SidePanelCoordinatorAndroid::DisableAnimationsForTesting() {
  // TODO(crbug.com/494000532): Implement this.
}

void SidePanelCoordinatorAndroid::SetNoDelaysForTesting(  // IN-TEST
    bool no_delays_for_testing) {
  waiter()->SetNoDelaysForTesting(no_delays_for_testing);  // IN-TEST
}

void SidePanelCoordinatorAndroid::Show(
    const UniqueKey& key,
    std::optional<SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  SPLOG("Show - key: " << key << ", open_trigger: "
                       << (open_trigger ? ToString(*open_trigger) : "nullopt")
                       << ", suppress_animations: " << suppress_animations);

  if (is_window_too_small_) {
    SPLOG("Show - window is too small, skipping.");
    return;
  }

  SidePanelEntry* entry = GetEntryForUniqueKey(key);
  if (!entry) {
    return;
  }

  if (!IsSidePanelShowing()) {
    SetOpenedTimestamp(base::TimeTicks::Now());
    SidePanelMetrics::RecordSidePanelOpen(open_trigger);
  }

  SidePanelMetrics::RecordSidePanelShowOrChangeEntryTrigger(open_trigger);

  if (IsSidePanelShowing()) {
    SPLOG("Show - Side panel is already showing.");
    std::optional<UniqueKey> current_entry_key = current_key();
    CHECK(current_entry_key)
        << "Current entry key should exist when side panel is showing.";

    // If the current entry is the same as the new entry we're trying to show,
    // we should cancel loading the new entry and keep the side panel visible.
    //
    // Not doing the above will cause the same entry to be loaded again and sent
    // to `PopulateSidePanel()`, whose logic will replace the current entry
    // with itself and then mark the entry as closed, since the same entry is
    // both the "previous entry" and the "new entry".
    if (*current_entry_key == key) {
      SPLOG("Show - Entry already visible, resetting and returning.");
      waiter()->ResetLoadingEntryIfNecessary();

      // If a ShowFrom() was pending or attempted on a visible entry, clear it.
      last_starting_bounds_.reset();

      // TODO(crbug.com/493931047): Handle the case where the current entry is
      // being closed, i.e., when `state_` is `SidePanelState::kClosing`.
      // In this case, we should:
      //   (1) stop the closing animation and keep the side panel open, and
      //   (2) notify the entry of `OnEntryHideCancelled()`.
      return;
    }
  }

  SidePanelMetrics::RecordEntryShowTriggeredMetrics(entry->key().id(),
                                                    open_trigger);

  waiter()->WaitForEntry(
      entry, base::BindOnce(&SidePanelCoordinatorAndroid::PopulateSidePanel,
                            base::Unretained(this), suppress_animations, key,
                            open_trigger));
}

void SidePanelCoordinatorAndroid::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<SidePanelNativeView> content_view) {
  SPLOG("PopulateSidePanel - unique_key: "
        << unique_key << ", suppress_animations: " << suppress_animations);
  std::unique_ptr<SidePanelNativeViewAndroid> native_view =
      content_view ? std::move(*content_view) : entry->GetContent();

  if (!native_view) {
    SPLOG("PopulateSidePanel - No native view found, returning.");
    return;
  }

  // Case 1: If the side panel isn't shown, just show it.
  //
  // If the side panel isn't shown, we will open it with/without animations
  // based on the `suppress_animations` param.
  if (!IsSidePanelShowing()) {
    SPLOG("PopulateSidePanel - No Side Panel showing, opening new panel.");
    state_ = SidePanelState::kOpening;
    SetCurrentKey(unique_key);
    entry->OnEntryShown();

    // We need to cache the `native_view` here after its internal Java View has
    // been populated into the UI. Otherwise, the `native_view` will be
    // destroyed since `entry->GetContent()` std::moved it. The underlying Java
    // View will still be alive, since it's in the View hierarchy. Without
    // caching the `native_view`, a new Java View will be created for the same
    // entry in cases like switching tabs.
    //
    // Note that this is slightly different from the WML `SidePanelCoordinator`.
    // On WML, when the View is being shown on the UI, the ownership of the View
    // is transferred to the UI and the cache in `SidePanelEntry` is empty.
    // When the View is removed from the UI, it'll be put back into the cache.
    PopulateJavaSidePanel(native_view->view(), suppress_animations);
    entry->CacheView(std::move(native_view));
    return;
  }
  SPLOG("PopulateSidePanel - Side Panel already showing, replacing content.");

  // Case 2: If the side panel is already shown, replace the UI contents.
  //
  // Note: when we replace the side panel's UI contents, no animation should be
  // played. However, we can't CHECK(suppress_animations) as the side panel
  // feature calling Show() may not be aware of the current side panel state.
  std::optional<UniqueKey> previous_entry_key = current_key();
  CHECK(previous_entry_key)
      << "Current key should exist when side panel is showing.";

  pending_replaced_entry_ = GetEntryForUniqueKey(*previous_entry_key);
  CHECK(pending_replaced_entry_)
      << "SidePanelEntry should exist when side panel is showing.";

  // The existing panel may have been loading, so we should cancel any load
  // methods as well.
  waiter()->ResetLoadingEntryIfNecessary();

  // The existing panel will receive a hidden event, which needs a reason.
  pending_hide_reason_ = SidePanelEntryHideReason::kReplaced;
  if (open_trigger && *open_trigger == SidePanelOpenTrigger::kTabChanged) {
    pending_hide_reason_ = SidePanelEntryHideReason::kBackgrounded;
  } else if (!open_trigger && previous_entry_key->tab_handle &&
             unique_key.tab_handle &&
             previous_entry_key->tab_handle != unique_key.tab_handle) {
    // Some side panel features observe active tab changes on their own and call
    // `SidePanelCoordinatorAndroid::Show` without an `open_trigger`. In such
    // cases, we use the entry keys' `tab_handle`s as a heuristic to
    // determine if `SidePanelEntryHideReason` should be `kBackgrounded`.
    //
    // TODO(crbug.com/503719405): Investigate whether we should always require
    // `open_trigger` for `SidePanelCoordinatorAndroid::Show`.
    pending_hide_reason_ = SidePanelEntryHideReason::kBackgrounded;
  }

  pending_replaced_entry_->OnEntryWillHide(pending_hide_reason_);

  // Now same as above, we set key before populate.
  CHECK(entry->type() == SidePanelType::kToolbar)
      << "Android Side Panel only supports kToolbar entries.";
  SetCurrentKey(unique_key);
  entry->OnEntryShown();

  // When populating the view, we will force there to be no animation,
  // regardless of param.
  //
  // Similar to Case 1, we need to cache the `native_view` here.
  //
  // Note: we don't clear the cached View for `pending_replaced_entry_`,
  // regardless of `pending_hide_reason_`. This mirrors the WML
  // `SidePanelCoordinator` behavior.
  PopulateJavaSidePanel(native_view->view(), /*suppress_animations=*/true);
  entry->CacheView(std::move(native_view));
}

void SidePanelCoordinatorAndroid::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  SPLOG("MaybeShowEntryOnTabStripModelChanged - old_contextual_registry: "
        << old_contextual_registry
        << ", new_contextual_registry: " << new_contextual_registry);

  if (is_window_too_small_) {
    SPLOG(
        "MaybeShowEntryOnTabStripModelChanged - window is too small, "
        "skipping.");
    return;
  }

  // If the side panel is showing, check if we should:
  // (1) replace the current UI content by calling `Show()`, or
  // (2) close the side panel by calling `Close()`.
  //
  // For (1), don't call `Close()` then `Show()`, which will cause janky UI.
  if (IsSidePanelShowing() && state_ != SidePanelState::kClosing) {
    std::optional<UniqueKey> new_active_key = GetNewActiveKeyOnTabChanged();

    if (!new_active_key && key_to_restore_after_window_resize_ &&
        CanShowEntryForKey(*key_to_restore_after_window_resize_)) {
      new_active_key = key_to_restore_after_window_resize_;
      key_to_restore_after_window_resize_.reset();
    }

    if (new_active_key) {
      Show(*new_active_key, SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    } else {
      std::optional<UniqueKey> key = current_key();
      CHECK(key) << "Current key should exist when side panel is showing.";

      if (old_contextual_registry &&
          old_contextual_registry->GetTabInterface().GetHandle() ==
              key->tab_handle) {
        Close(SidePanelEntryHideReason::kBackgrounded,
              /*suppress_animations=*/true);
      }
    }

    return;
  }

  // If the side panel isn't showing, check if we should show it.
  std::optional<SidePanelEntry*> new_active_entry =
      new_contextual_registry ? new_contextual_registry->GetActiveEntry()
                              : nullptr;
  if (new_active_entry) {
    UniqueKey key{new_contextual_registry->GetTabInterface().GetHandle(),
                  (*new_active_entry)->key()};
    Show(key, SidePanelOpenTrigger::kTabChanged, /*suppress_animations=*/true);
  } else if (key_to_restore_after_window_resize_ &&
             CanShowEntryForKey(*key_to_restore_after_window_resize_)) {
    Show(*key_to_restore_after_window_resize_,
         SidePanelOpenTrigger::kTabChanged,
         /*suppress_animations=*/true);
    key_to_restore_after_window_resize_.reset();
  }
}

void SidePanelCoordinatorAndroid::ClearCachedEntryViews() {
  if (auto* window_registry = SidePanelRegistry::From(browser())) {
    window_registry->ClearCachedEntryViews();
  }

  if (auto* tab_list = TabListInterface::From(browser())) {
    for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
      if (auto* registry = SidePanelRegistry::From(tab)) {
        registry->ClearCachedEntryViews();
      }
    }
  }
}

ScopedJavaLocalRef<jobject> SidePanelCoordinatorAndroid::java_coordinator()
    const {
  ScopedJavaLocalRef<jobject> local_ref =
      java_coordinator_.get(AttachCurrentThread());

  CHECK(local_ref) << "Java SidePanelCoordinatorAndroid is the sole owner of "
                      "C++ SidePanelCoordinatorAndroid, so the Java object "
                      "shouldn't be destroyed before the C++ object";
  return local_ref;
}

void SidePanelCoordinatorAndroid::PopulateJavaSidePanel(
    const JavaRef<jobject>& view,
    bool suppress_animations) {
  // Pass the starting bounds to Java. If no bounds were provided (e.g. not a
  // ShowFrom call), we use kNoBounds as a sentinel for JNI.
  gfx::Rect start_bounds = last_starting_bounds_.value_or(kNoBounds);
  last_starting_bounds_.reset();

  Java_SidePanelCoordinatorAndroidImpl_populateSidePanel(
      AttachCurrentThread(), java_coordinator(), view, start_bounds.x(),
      start_bounds.y(), start_bounds.width(), start_bounds.height(),
      suppress_animations);
}

bool SidePanelCoordinatorAndroid::CanShowEntryForKey(
    const UniqueKey& key) const {
  if (!GetEntryForUniqueKey(key)) {
    return false;
  }

  SidePanelRegistry* active_contextual_registry = GetActiveContextualRegistry();
  if (active_contextual_registry &&
      active_contextual_registry->GetTabInterface().GetHandle() ==
          key.tab_handle) {
    return true;
  }

  return !key.tab_handle.has_value();
}

// ----------------------------------------------------------------------------
// Methods called from Java via SidePanelCoordinatorAndroidImpl.Natives:
// ----------------------------------------------------------------------------

// static
static int64_t JNI_SidePanelCoordinatorAndroidImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t nativeBrowserWindowPtr) {
  SPLOG("JNI_SidePanelCoordinatorAndroidImpl_Create - ptr: "
        << nativeBrowserWindowPtr);
  return reinterpret_cast<intptr_t>(new SidePanelCoordinatorAndroid(
      env, caller,
      reinterpret_cast<BrowserWindowInterface*>(nativeBrowserWindowPtr)));
}

DEFINE_JNI(SidePanelCoordinatorAndroidImpl)
