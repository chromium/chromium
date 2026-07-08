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

bool SidePanelCoordinatorAndroid::HasContentToShow(JNIEnv* env) {
  switch (state_) {
    case SidePanelState::kOpening:
    case SidePanelState::kShown:
      return true;
    case SidePanelState::kClosing:
      // Unlike `kClosed`, we shouldn't check whether there is a deferred entry
      // for `kClosing`.
      //
      // This is because a deferred entry is added before `Close()`, so by the
      // time the state is `kClosing`, a deferred entry already exists.
      // For the side panel to be closed, we have to return `false` without
      // checking whether there is a deferred entry.
      return false;
    case SidePanelState::kClosed: {
      // When the side panel is `kClosed`, whether there is content to show
      // depends on whether there is a deferred entry.
      //
      // A deferred entry is an entry that could have been shown, but was
      // deferred due to Android constraints such as narrow window size.
      tabs::TabInterface* active_tab =
          TabListInterface::From(browser())->GetActiveTab();
      return active_tab &&
             deferred_entry_tracker_.GetEntry(active_tab->GetHandle())
                 .has_value();
    }
  }
}

void SidePanelCoordinatorAndroid::OnPanelOpened(JNIEnv* env) {
  SPLOG("OnPanelOpened");
  CHECK(state_ == SidePanelState::kOpening)
      << "Should only receive OnPanelOpened() when side panel is opening.";

  state_ = SidePanelState::kShown;
}

void SidePanelCoordinatorAndroid::OnPanelClosed(JNIEnv* env) {
  SPLOG("OnPanelClosed");
  CHECK(state_ == SidePanelState::kClosing)
      << "Should only receive OnPanelClosed() when side panel is closing.";

  SidePanelEntry* entry = GetEntryForCurrentKeyNonNull();

  SetCurrentKey(/*new_key=*/std::nullopt);

  // Now that the animation has completed, we can update our local state to be
  // closed, and trigger the entry hidden callbacks.
  if (pending_hide_reason_) {
    entry->OnEntryHidden();
    entry->OnEntryHiddenWithReason(*pending_hide_reason_);
    pending_hide_reason_ = std::nullopt;

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
    // TODO(crbug.com/503113522): Consider having `SidePanelRegistry` _reset_
    // the active entry so it's consistent with how the active entry is _set_.
    if (auto* contextual_registry = GetActiveContextualRegistry()) {
      contextual_registry->ResetActiveEntry();
    }
    if (auto* window_registry = SidePanelRegistry::From(browser())) {
      window_registry->ResetActiveEntry();
    }
    ClearCachedEntryViews();

    SidePanelMetrics::RecordSidePanelClosed(opened_timestamp());
  }

  state_ = SidePanelState::kClosed;
}

void SidePanelCoordinatorAndroid::OnPanelContentReplaced(JNIEnv* env) {
  SPLOG("OnPanelContentReplaced");

  CHECK(pending_replaced_entry_);
  CHECK(pending_hide_reason_);

  pending_replaced_entry_->OnEntryHidden();
  pending_replaced_entry_->OnEntryHiddenWithReason(*pending_hide_reason_);
  pending_replaced_entry_ = nullptr;
  pending_hide_reason_ = std::nullopt;
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
  SPLOG("Close - hide_reason: "
        << ToString(hide_reason) << ", suppress_animations: "
        << suppress_animations << ", state: " << ToString(state_));

  // Stop any pending load.
  waiter()->ResetLoadingEntryIfNecessary();

  // If a ShowFrom() was pending, clear the starting bounds.
  last_starting_bounds_.reset();

  // Nothing to do if the side panel is not showing or is already closing.
  if (!IsSidePanelShowing() || state_ == SidePanelState::kClosing) {
    return;
  }

  // We are about to change `state_`, so end ongoing animations to reach a
  // stable `state_` first. This includes notifying SidePanelEntries of the
  // stable state.
  EndAnimations();

  // When we start to close, we will update state to closing, and send a remove
  // request to Java, which will handle animations and call back when done.
  state_ = SidePanelState::kClosing;
  SidePanelEntry* entry = GetEntryForCurrentKeyNonNull();
  entry->OnEntryWillHide(hide_reason);
  pending_hide_reason_ = hide_reason;
  Java_SidePanelCoordinatorAndroidImpl_startClosingPanel(
      AttachCurrentThread(), java_coordinator(), suppress_animations);
}

void SidePanelCoordinatorAndroid::OnTabReparented(tabs::TabInterface* tab) {
  SPLOG("OnTabReparented - tab: " << tab);

  if (auto* registry = SidePanelRegistry::From(tab)) {
    for (auto const& entry : registry->entries()) {
      entry->ClearCachedView();
    }
  }

  // In multi-tab windows, when the active tab is reparented out, the source
  // window activates another tab first. This triggers
  // `SidePanelTabListObserverAndroid::OnActiveTabChanged()`, which already
  // closes or replaces the side panel before this method runs, making any
  // additional cleanup here unnecessary.
  auto* tab_list = TabListInterface::From(browser());
  if (tab_list && tab_list->GetTabCount() > 0) {
    return;
  }

  // Specifically target the "Single-Tab Window Scenario" (e.g., tearing off
  // the sole tab in a window to create a new window or move it to another
  // window).
  //
  // In this case, because the source window is left with 0 tabs, Android's
  // `TabListInterface` cannot select a new active tab and never fires
  // `SidePanelTabListObserverAndroid::OnActiveTabChanged()`. Thus, the source
  // window's side panel remains open and `current_key()` still matches the
  // reparented tab here.
  //
  // Calling `Close()` here is critical: it synchronously detaches the
  // underlying cached Java view from the source window's view hierarchy. This
  // ensures that when the tab is inserted and activated in the destination
  // window, the Java view has no parent and can be attached safely without
  // throwing an `IllegalStateException: The specified child already has a
  // parent`.
  std::optional<UniqueKey> key = current_key();
  if (key && key->tab_handle && key->tab_handle.value() == tab->GetHandle()) {
    SPLOG("OnTabReparented - closing side panel for reparented tab.");
    Close(SidePanelEntryHideReason::kBackgrounded,
          /*suppress_animations=*/true);
  }
}

void SidePanelCoordinatorAndroid::OnWillAutoClose(JNIEnv* env) {
  SPLOG("OnWillAutoClose");

  if (has_insufficient_space_) {
    return;
  }

  has_insufficient_space_ = true;

  if (IsSidePanelShowing() && state_ != SidePanelState::kClosing) {
    deferred_entry_tracker_.AddActiveEntries();

    // TODO(crbug.com/527985639): Rename `kWindowResized` as
    // `kInsufficientSpace`.
    Close(SidePanelEntryHideReason::kWindowResized,
          /*suppress_animations=*/true);
  }
}

void SidePanelCoordinatorAndroid::OnWillAutoRestore(JNIEnv* env) {
  SPLOG("OnWillAutoRestore");

  if (!has_insufficient_space_) {
    return;
  }

  has_insufficient_space_ = false;

  CHECK(!IsSidePanelShowing() || state_ == SidePanelState::kClosing)
      << "Side panel should not be visible when the available space changes"
         " from insufficient to sufficient.";

  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  if (!active_tab) {
    return;
  }

  // Check if there's a deferred entry tracked explicitly.
  std::optional<UniqueKey> key_to_show =
      deferred_entry_tracker_.GetEntry(active_tab->GetHandle());

  if (key_to_show) {
    Show(*key_to_show, SidePanelOpenTrigger::kWindowResized,
         /*suppress_animations=*/true);
  }
}

void SidePanelCoordinatorAndroid::Init(JNIEnv* env) {
  SPLOG("Init");
  // During tab tear-off (multi-window), a new Activity is created and the
  // reparented tab is added to the tab model before this coordinator and
  // its observer are constructed. Consequently, the observer misses the
  // initial active tab change event. We explicitly trigger it here during
  // initialization to restore the side panel state for the active tab.
  if (tabs::TabInterface* active_tab =
          TabListInterface::From(browser())->GetActiveTab()) {
    OnActiveTabChanged(/*old_contents=*/nullptr, active_tab->GetContents(),
                       /*tab_removed_for_deletion=*/false);
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

  if (entry &&
      (state_ == SidePanelState::kShown ||
       state_ == SidePanelState::kOpening) &&
      IsSidePanelShowing() && IsSidePanelEntryShowing(key)) {
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
  // On Android, side panels are built using native Android Views instead of
  // WebContents.
  return nullptr;
}

void SidePanelCoordinatorAndroid::DisableAnimationsForTesting() {  // IN-TEST
  if (java_coordinator()) {
    Java_SidePanelCoordinatorAndroidImpl_disableAnimationsForTesting(  // IN-TEST
        AttachCurrentThread(), java_coordinator());
  }
}

void SidePanelCoordinatorAndroid::SetNoDelaysForTesting(  // IN-TEST
    bool no_delays_for_testing) {
  waiter()->SetNoDelaysForTesting(no_delays_for_testing);  // IN-TEST
}

SidePanelState SidePanelCoordinatorAndroid::GetStateForTesting() {  // IN-TEST
  return state_;
}

int SidePanelCoordinatorAndroid::GetContainerWidthForTesting() {  // IN-TEST
  return Java_SidePanelCoordinatorAndroidImpl_getContainerWidthForTesting(  // IN-TEST
      AttachCurrentThread(), java_coordinator());
}

void SidePanelCoordinatorAndroid::Show(
    const UniqueKey& key,
    std::optional<SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  // TODO(crbug.com/503719405): Remove CHECK once param is non-optional.
  CHECK(open_trigger.has_value());
  SPLOG("Show - key: " << key << ", open_trigger: "
                       << (open_trigger ? ToString(*open_trigger) : "nullopt")
                       << ", suppress_animations: " << suppress_animations
                       << ", state: " << ToString(state_));

  // Defer the show request if there is insufficient space to show the side
  // panel.
  if (has_insufficient_space_) {
    SPLOG("Show - insufficient space, skipping.");
    deferred_entry_tracker_.AddEntry(key);
    return;
  }
  deferred_entry_tracker_.ClearEntry(key);

  SidePanelEntry* entry = GetEntryForUniqueKey(key);
  if (!entry) {
    return;
  }
  CHECK(entry->type() == SidePanelType::kToolbar)
      << "Android Side Panel only supports kToolbar entries.";

  // Check #IsSidePanelShowing() specifically to stay aligned with other
  // platforms.
  if (!IsSidePanelShowing()) {
    SetOpenedTimestamp(base::TimeTicks::Now());
    SidePanelMetrics::RecordSidePanelOpen(open_trigger);
  }
  SidePanelMetrics::RecordSidePanelShowOrChangeEntryTrigger(open_trigger);

  if (IsSidePanelShowing() && GetCurrentKeyNonNull() == key) {
    SPLOG("Show - Requested to show an entry that's already shown.");

    // If the current entry is the same as the new entry we're trying to show,
    // we should cancel loading the new entry and keep the side panel visible.
    waiter()->ResetLoadingEntryIfNecessary();

    // If a ShowFrom() was pending or attempted on a visible entry, clear it.
    last_starting_bounds_.reset();

    if (state_ != SidePanelState::kClosing) {
      return;
    }

    // When we show an entry that's closing, we'll end the closing animation
    // first, then start showing the entry.
    //
    // Also, we should invoke the entry's OnEntryHideCancelled() and skip
    // OnEntryHidden().
    //
    // Therefore, we clear pending_hide_reason_ here so that when the closing
    // animation ends, OnPanelClosed() won't invoke OnEntryHidden().
    SPLOG("Show - Requested to show an entry that's closing");
    pending_hide_reason_ = std::nullopt;
    entry->OnEntryHideCancelled();
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
  // TODO(crbug.com/503719405): Remove CHECK once param is non-optional.
  CHECK(open_trigger.has_value());

  entry->set_last_open_trigger(open_trigger);
  SPLOG("PopulateSidePanel - unique_key: "
        << unique_key << ", suppress_animations: " << suppress_animations);
  std::unique_ptr<SidePanelNativeViewAndroid> native_view =
      content_view ? std::move(*content_view) : entry->GetContent();

  if (!native_view) {
    SPLOG("PopulateSidePanel - No native view found, returning.");
    return;
  }

  // We are about to change `state_`, so end ongoing animations to reach a
  // stable `state_` first. This includes notifying SidePanelEntries of the
  // stable state.
  EndAnimations();

  if (!IsSidePanelShowing()) {
    StartOpeningPanel(entry, unique_key, suppress_animations,
                      std::move(native_view));
  } else {
    // Note: when we replace the side panel's UI contents, no animation should
    // be played. However, we can't CHECK(suppress_animations) as the side panel
    // feature calling Show() may not be aware of the current side panel state.
    StartReplacingPanelContent(entry, unique_key, *open_trigger,
                               std::move(native_view));
  }
}

void SidePanelCoordinatorAndroid::StartOpeningPanel(
    SidePanelEntry* entry,
    const UniqueKey& unique_key,
    bool suppress_animations,
    std::unique_ptr<SidePanelNativeViewAndroid> native_view) {
  SPLOG("StartOpeningPanel.");
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
  gfx::Rect start_bounds = last_starting_bounds_.value_or(kNoBounds);
  last_starting_bounds_.reset();
  Java_SidePanelCoordinatorAndroidImpl_startOpeningPanel(
      AttachCurrentThread(), java_coordinator(), native_view->view(),
      start_bounds.x(), start_bounds.y(), start_bounds.width(),
      start_bounds.height(), suppress_animations);
  entry->CacheView(std::move(native_view));
}

void SidePanelCoordinatorAndroid::StartReplacingPanelContent(
    SidePanelEntry* new_entry,
    const UniqueKey& new_key,
    SidePanelOpenTrigger open_trigger,
    std::unique_ptr<SidePanelNativeViewAndroid> native_view) {
  SPLOG("StartReplacingPanelContent.");

  // Always clear the current tab's active entry before replacing the current
  // entry.
  //
  // This implements requirements for cross-registry entry replacement:
  //
  // (a) If a window-scoped entry replaces a tab-scoped entry, the tab-scoped
  // entry should become _inactive_.
  //
  // (b) If a tab-scoped entry replaces a window-scoped entry, the window-scoped
  // entry should remain _active_.
  //
  // (c) If a tab-scoped entry replaces another tab-scoped entry in a different
  // tab, StartReplacingPanelContent() is called _after_ the active tab has
  // changed, so clearing the active entry for the new active tab's registry is
  // fine: the new entry will be set as the new tab's active entry when
  // `OnEntryShown()` is called below.
  //
  // Please see https://crbug.com/508402076#comment5 for more details on
  // cross-registry entry replacement.
  //
  // Note that this _doesn't_ break same-registry entry replacement:
  //
  // (a) If both the old entry and the new entry belong to the same tab-scoped
  // registry, the new entry's `OnEntryShown()` function will set the new entry
  // as the active entry.
  //
  // (b) If both the old entry and the new entry belong to the same
  // window-scoped registry, clearing the active entry for the active tab's
  // registry is a no-op.
  if (auto* contextual_registry = GetActiveContextualRegistry()) {
    contextual_registry->ResetActiveEntry();
  }

  UniqueKey current_key = GetCurrentKeyNonNull();
  pending_replaced_entry_ = GetEntryForUniqueKey(current_key);
  CHECK(pending_replaced_entry_) << "No SidePanelEntry to replace";

  // The existing panel may have been loading, so we should cancel any load
  // methods as well.
  waiter()->ResetLoadingEntryIfNecessary();

  // The existing panel will receive a hidden event, which needs a reason.
  pending_hide_reason_ = SidePanelEntryHideReason::kReplaced;

  if (open_trigger == SidePanelOpenTrigger::kTabChanged) {
    pending_hide_reason_ = SidePanelEntryHideReason::kBackgrounded;
  }

  pending_replaced_entry_->OnEntryWillHide(*pending_hide_reason_);

  // Set key before replacing the current entry.
  SetCurrentKey(new_key);
  new_entry->OnEntryShown();

  // Similar to StartOpeningPanel(), we need to cache the `native_view` here.
  //
  // Note: we don't clear the cached View for `current_entry`,
  // regardless of `hide_reason`. This mirrors the WML
  // `SidePanelCoordinator` behavior.
  Java_SidePanelCoordinatorAndroidImpl_startReplacingPanelContent(
      AttachCurrentThread(), java_coordinator(), native_view->view());
  new_entry->CacheView(std::move(native_view));
}

void SidePanelCoordinatorAndroid::EndAnimations() {
  Java_SidePanelCoordinatorAndroidImpl_endAnimations(AttachCurrentThread(),
                                                     java_coordinator());
  CHECK(state_ == SidePanelState::kClosed || state_ == SidePanelState::kShown)
      << "Side panel should be in a stable state after ending all animations.";
}

void SidePanelCoordinatorAndroid::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  SPLOG("MaybeShowEntryOnTabStripModelChanged - old_contextual_registry: "
        << old_contextual_registry
        << ", new_contextual_registry: " << new_contextual_registry);

  // If the side panel is showing, check if we should:
  // (1) replace the current UI content by calling `Show()`, or
  // (2) close the side panel by calling `Close()`.
  //
  // For (1), don't call `Close()` then `Show()`, which will cause janky UI.
  if (IsSidePanelShowing() && state_ != SidePanelState::kClosing) {
    std::optional<UniqueKey> new_active_key = GetNewActiveKeyOnTabChanged();

    if (new_active_key) {
      Show(*new_active_key, SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    } else {
      UniqueKey key = GetCurrentKeyNonNull();

      if (old_contextual_registry &&
          old_contextual_registry->GetTabInterface().GetHandle() ==
              key.tab_handle) {
        Close(SidePanelEntryHideReason::kBackgrounded,
              /*suppress_animations=*/true);
      }

      if (new_contextual_registry) {
        // If there is no active entry in the new tab's registry, check if there
        // is a deferred entry saved in the tracker for this tab or this window.
        // This handles cases where a side panel was hidden due to constraints
        // like insufficient space.
        //
        // `Show()` handles `has_insufficient_space_ == true`, and adds the
        // entry to `SidePanelDeferredEntryTracker` if needed.
        std::optional<UniqueKey> key_to_show = deferred_entry_tracker_.GetEntry(
            new_contextual_registry->GetTabInterface().GetHandle());
        if (key_to_show) {
          // Suppress animations to avoid jarring UX during tab switches, and
          // use SidePanelOpenTrigger::kWindowResized as the trigger to match
          // the close reason that originally deferred this entry.
          Show(*key_to_show, SidePanelOpenTrigger::kWindowResized,
               /*suppress_animations=*/true);
        }
      }
    }

    return;
  }

  // If the side panel isn't showing, check if we should show it.
  std::optional<SidePanelEntry*> new_active_entry =
      new_contextual_registry ? new_contextual_registry->GetActiveEntry()
                              : std::nullopt;
  if (new_active_entry) {
    UniqueKey key{new_contextual_registry->GetTabInterface().GetHandle(),
                  (*new_active_entry)->key()};
    Show(key, SidePanelOpenTrigger::kTabChanged, /*suppress_animations=*/true);
  } else if (new_contextual_registry) {
    // If there is no active entry in the new tab's registry, check if there
    // is a deferred entry saved in the tracker for this tab or this window.
    // This handles cases where a side panel was hidden due to constraints
    // like insufficient space.
    // `Show()` handles `has_insufficient_space_ == true`, and adds the entry
    // to `SidePanelDeferredEntryTracker` if needed.
    std::optional<UniqueKey> key_to_show = deferred_entry_tracker_.GetEntry(
        new_contextual_registry->GetTabInterface().GetHandle());
    if (key_to_show) {
      // Suppress animations to avoid jarring UX during tab switches, and use
      // SidePanelOpenTrigger::kWindowResized as the trigger to match the close
      // reason that originally deferred this entry.
      Show(*key_to_show, SidePanelOpenTrigger::kWindowResized,
           /*suppress_animations=*/true);
    }
  }
}

void SidePanelCoordinatorAndroid::ClearDeferredEntryForTab(
    const tabs::TabHandle& tab_handle) {
  deferred_entry_tracker_.ClearEntryForTab(tab_handle);
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

SidePanelUIBase::UniqueKey SidePanelCoordinatorAndroid::GetCurrentKeyNonNull()
    const {
  std::optional<UniqueKey> key = current_key();
  CHECK(key) << "Current entry key is expected to exist.";
  return *key;
}

SidePanelEntry* SidePanelCoordinatorAndroid::GetEntryForCurrentKeyNonNull()
    const {
  SidePanelEntry* entry = GetEntryForUniqueKey(GetCurrentKeyNonNull());
  CHECK(entry) << "SidePanelEntry is expected to exist.";
  return entry;
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
