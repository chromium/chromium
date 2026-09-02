// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_model_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_waiter.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_enums_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_metrics.h"
#include "chrome/browser/ui/side_panel/side_panel_util.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelCoordinatorAndroidBridge_jni.h"

#define LOG_TAG "SidePanelCoordinatorAndroid"
#define SPLOG(message)                                     \
  if (base::FeatureList::IsEnabled(                        \
          chrome::android::kEnableAndroidSidePanelLogs)) { \
    LOG(ERROR) << LOG_TAG << ": " << message;              \
  }

namespace {
constexpr char kAndroidSidePanelHistogramPrefix[] = "SidePanel.Android";

void RecordAutoCloseOrRestoreMetric(SidePanelEntry* entry, bool is_auto_close) {
  if (!entry) {
    return;
  }
  std::string_view entry_name =
      SidePanelEntryIdToHistogramName(entry->key().id());
  std::string_view action =
      is_auto_close ? ".OnWillAutoClose" : ".OnWillAutoRestore";
  base::UmaHistogramBoolean(
      base::StrCat({kAndroidSidePanelHistogramPrefix, ".", entry_name, action}),
      true);
}
}  // namespace

using jni_zero::AttachCurrentThread;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

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
      tab_model_observer_(
          static_cast<TabModel*>(TabListInterface::From(browser)),
          this) {
  SPLOG("SidePanelCoordinatorAndroid Constructor - browser: " << browser);
}

SidePanelCoordinatorAndroid::~SidePanelCoordinatorAndroid() {
  SPLOG("SidePanelCoordinatorAndroid Destructor");
  ClearCachedEntryViews(/*include_active_entry=*/true);
  Java_SidePanelCoordinatorAndroidBridge_clearNativePtr(AttachCurrentThread(),
                                                        java_coordinator());
}

void SidePanelCoordinatorAndroid::Destroy() {
  SPLOG("Destroy");
  delete this;
}

void SidePanelCoordinatorAndroid::ClosePanel(bool suppress_animations) {
  SPLOG("ClosePanel");
  Close(SidePanelEntryHideReason::kSidePanelClosed, suppress_animations);
}

bool SidePanelCoordinatorAndroid::HasContentToShow(TabAndroid* tab) {
  CHECK(tab);

  // Check if the tab has an active tab-scoped (contextual) entry.
  if (auto* tab_scoped_registry = SidePanelRegistry::From(tab)) {
    if (auto active_entry = tab_scoped_registry->GetActiveEntry()) {
      SPLOG("HasContentToShow - tab-scoped (contextual) entry active for tab ("
            << (*active_entry)->key().ToString() << "), returning true");
      return true;
    }
  }

  // Check if the window registry has an active window-scoped (global) entry.
  if (auto* window_scoped_registry = SidePanelRegistry::From(browser())) {
    if (auto active_entry = window_scoped_registry->GetActiveEntry()) {
      SPLOG("HasContentToShow - window-scoped (global) entry active ("
            << (*active_entry)->key().ToString() << "), returning true");
      return true;
    }
  }

  // We shouldn't check whether there is a deferred entry for `kClosing`
  // (unlike `kClosed`).
  //
  // This is because a deferred entry is added before `Close()`, so by the
  // time the state is `kClosing`, a deferred entry already exists.
  // For the side panel to be closed, we have to return `false` without
  // checking whether there is a deferred entry.
  if (state_ == SidePanelState::kClosing) {
    SPLOG("HasContentToShow - state is kClosing, returning false");
    return false;
  }

  // Check if there is a deferred entry for this tab or window.
  if (auto deferred_entry =
          deferred_entry_tracker_.GetTabOrWindowScopedEntry(tab->GetHandle())) {
    SPLOG("HasContentToShow - deferred entry exists for tab ("
          << deferred_entry->key.ToString() << "), returning true");
    return true;
  }

  SPLOG("HasContentToShow - no entry found, returning false");
  return false;
}

void SidePanelCoordinatorAndroid::OnPanelContainerUpdated(int old_width,
                                                          int new_width) {
  SPLOG("OnPanelContainerUpdated - old_width: "
        << old_width << ", new_width: " << new_width
        << ", state: " << ToString(state_));

  if (old_width == new_width) {
    return;
  }

  // The side panel has finished opening.
  if (old_width == 0 && new_width > 0) {
    CHECK_EQ(state_, SidePanelState::kOpening);
    FinishOpeningPanel();
    return;
  }

  // The side panel has finished closing.
  if (old_width > 0 && new_width == 0) {
    CHECK_EQ(state_, SidePanelState::kClosing);
    FinishClosingPanel();
  }
}

void SidePanelCoordinatorAndroid::OnPanelContentReplaced() {
  SPLOG("OnPanelContentReplaced");

  CHECK(pending_replaced_entry_);
  CHECK(pending_hide_reason_);

  pending_replaced_entry_->OnEntryHidden();
  pending_replaced_entry_->OnEntryHiddenWithReason(*pending_hide_reason_);
  pending_replaced_entry_ = nullptr;
  pending_hide_reason_ = std::nullopt;
}

void SidePanelCoordinatorAndroid::OnActiveChanged(bool active) {
  SPLOG("OnActiveChanged - active: " << active);
  if (!active) {
    deferred_entry_tracker_.AddActiveEntries();
    Close(SidePanelEntryHideReason::kBackgrounded,
          /*suppress_animations=*/true);
    return;
  }

  if (tabs::TabInterface* active_tab =
          TabListInterface::From(browser())->GetActiveTab()) {
    std::optional<UniqueKey> key_to_show =
        deferred_entry_tracker_.GetTabOrWindowScopedEntry(
            active_tab->GetHandle());
    if (key_to_show) {
      Show(*key_to_show, SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    }
  }
}

void SidePanelCoordinatorAndroid::ShowFrom(
    SidePanelEntryKey entry_key,
    gfx::Rect starting_bounds_in_browser_coordinates) {
  // On WML, ShowFrom() is for content morph animations, such as when side panel
  // content originates from a tab's WebContents and smoothly transitions into
  // its final side panel position.
  //
  // As of Aug 31, 2026, the only use case on WML is the "Tab-to-Panel"
  // transition for contextual tasks (AI Mode) and it is disabled (verified on
  // Canary M154.0.8036.0).
  //
  // Android doesn't support this UX.
  NOTREACHED() << "Not supported by Android UI";
}

void SidePanelCoordinatorAndroid::Close(SidePanelEntryHideReason hide_reason,
                                        bool suppress_animations) {
  SPLOG("Close - hide_reason: "
        << ToString(hide_reason) << ", suppress_animations: "
        << suppress_animations << ", state: " << ToString(state_));

  // Stop any pending load.
  waiter()->ResetLoadingEntryIfNecessary();

  // Nothing to do if the side panel is not showing or is already closing.
  if (!IsSidePanelShowing() || state_ == SidePanelState::kClosing) {
    return;
  }

  // We are about to change `state_`, so end ongoing animations to reach a
  // stable `state_` first. This includes notifying SidePanelEntries of the
  // stable state.
  EndAnimations();

  StartClosingPanel(hide_reason, suppress_animations);
}

void SidePanelCoordinatorAndroid::OnTabClosed(TabAndroid* tab) {
  SPLOG("OnTabClosed - tab: " << tab);
  CHECK(tab);

  deferred_entry_tracker_.ClearTabScopedEntry(tab->GetHandle());

  // During a tab switch (tab_1 -> tab_2), if tab_2's side panel View
  // contains a ThinWebView, the Java side will delay removing tab_1's side
  // panel View until tab_2's ThinWebView has rendered the first frame. This is
  // to prevent UI flickers.
  //
  // This also means `OnPanelContentReplaced()` is called when tab_1 has become
  // inactive.
  //
  // The following logic targets the case where a tab switch is triggered by
  // _closing_ tab_1.
  //
  // In this case, we must _not_ delay removing tab_1's side panel View.
  // Otherwise, when `OnPanelContentReplaced()` is called, the
  // `pending_replaced_entry_` will be an invalid pointer since tab_1 is already
  // destroyed.
  CompletePendingContentReplacementForTab(tab);
}

void SidePanelCoordinatorAndroid::OnTabReparented(TabAndroid* tab) {
  SPLOG("OnTabReparented - tab: " << tab);
  CHECK(tab);

  // `OnTabReparented()` is triggered when the `tab` is removed from this
  // SidePanelCoordinatorAndroid's window and has become the active tab in a
  // new window.
  //
  // If this coordinator (window) has a deferred entry for the `tab`, we should
  // set it as the tab's active entry so the side panel appears in the tab's new
  // host window.
  //
  // A scenario where this logic is necessary:
  // (1) Open a tab-scoped side panel.
  // (2) Make the window narrow enough so the side panel is auto-closed. The
  // tab's active entry will become a deferred entry.
  // (3) Move the tab to a new window that's wide enough for the side panel.
  // (4) The side panel should appear in the new window.
  if (std::optional<UniqueKey> deferred_entry =
          deferred_entry_tracker_.GetTabScopedEntry(tab->GetHandle())) {
    if (SidePanelEntry* entry = GetEntryForUniqueKey(*deferred_entry)) {
      if (auto* registry = SidePanelRegistry::From(tab)) {
        registry->SetActiveEntry(entry);
      }
    }
  }

  deferred_entry_tracker_.ClearTabScopedEntry(tab->GetHandle());

  // During a tab switch (tab_1 -> tab_2), if tab_2's side panel View
  // contains a ThinWebView, the Java side will delay removing tab_1's side
  // panel View until tab_2's ThinWebView has rendered the first frame. This is
  // to prevent UI flickers.
  //
  // The following logic targets the case where a tab switch is triggered by
  // _reparenting_ tab_1 to another window.
  //
  // In this case, we must _not_ delay removing tab_1's side panel View.
  // Otherwise, the destination window will activate tab_1 and show its
  // side panel, only for the async delay in the source window to finish
  // later and unexpectedly call OnEntryHidden(), permanently freezing tab_1's
  // UI in the destination window.
  CompletePendingContentReplacementForTab(tab);

  if (auto* registry = SidePanelRegistry::From(tab)) {
    for (auto const& entry : registry->entries()) {
      entry->ClearCachedView();
    }
  }

  // In multi-tab windows, when the active tab is reparented out, the source
  // window activates another tab first. This triggers
  // `SidePanelTabModelObserver::DidSelectTab()`, which already
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
  // `SidePanelTabModelObserver::DidSelectTab()`. Thus, the source
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

void SidePanelCoordinatorAndroid::OnWillAutoClose() {
  SPLOG("OnWillAutoClose");

  if (has_insufficient_space_) {
    return;
  }

  has_insufficient_space_ = true;

  if (IsSidePanelShowing() && state_ != SidePanelState::kClosing) {
    RecordAutoCloseOrRestoreMetric(GetEntryForCurrentKeyNonNull(),
                                   /*is_auto_close=*/true);

    deferred_entry_tracker_.AddActiveEntries();

    // TODO(crbug.com/527985639): Rename `kWindowResized` as
    // `kInsufficientSpace`.
    Close(SidePanelEntryHideReason::kWindowResized,
          /*suppress_animations=*/true);
  }
}

void SidePanelCoordinatorAndroid::OnWillAutoRestore() {
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
      deferred_entry_tracker_.GetTabOrWindowScopedEntry(
          active_tab->GetHandle());

  if (key_to_show) {
    RecordAutoCloseOrRestoreMetric(GetEntryForUniqueKey(*key_to_show),
                                   /*is_auto_close=*/false);
    Show(*key_to_show, SidePanelOpenTrigger::kWindowResized,
         /*suppress_animations=*/true);
  }
}

void SidePanelCoordinatorAndroid::Init() {
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
    Java_SidePanelCoordinatorAndroidBridge_disableAnimationsForTesting(  // IN-TEST
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
  return Java_SidePanelCoordinatorAndroidBridge_getContainerWidthForTesting(  // IN-TEST
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
}

void SidePanelCoordinatorAndroid::
    ConfigDeferredViewReplacementForTesting(  // IN-TEST
        bool enable) {
  Java_SidePanelCoordinatorAndroidBridge_configDeferredViewReplacementForTesting(  // IN-TEST
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile(),
      enable);
}

void SidePanelCoordinatorAndroid::
    SimulateAutoCloseConditionForTesting() {  // IN-TEST
  Java_SidePanelCoordinatorAndroidBridge_simulateAutoCloseConditionForTesting(  // IN-TEST
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
}

void SidePanelCoordinatorAndroid::
    SimulateAutoRestoreConditionForTesting() {  // IN-TEST
  Java_SidePanelCoordinatorAndroidBridge_simulateAutoRestoreConditionForTesting(  // IN-TEST
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
}

bool SidePanelCoordinatorAndroid::
    HasPendingReplacedEntryForTesting()  // IN-TEST
    const {
  return pending_replaced_entry_ != nullptr;
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

  SidePanelEntry* entry = GetEntryForUniqueKey(key);
  if (!entry) {
    return;
  }

  // Defer the show request if there is insufficient space to show the side
  // panel.
  //
  // Note that `Show()` can be called when
  // (1) There isn't sufficient space to show the side panel, and
  // (2) `has_insufficient_space_` hasn't been updated by `onWillAutoClose()`
  // or `onWillAutoRestore()`.
  //
  // One such case is tab reparenting: moving a tab with a tab-scoped side panel
  // to a narrow window.
  //
  // So we call into Java to update `has_insufficient_space_`.
  has_insufficient_space_ = !Java_SidePanelCoordinatorAndroidBridge_canShow(
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
  if (has_insufficient_space_) {
    SPLOG("Show - insufficient space; defer showing the entry.");
    deferred_entry_tracker_.AddEntry(key);
    entry->OnEntryShowDeferred();
    return;
  }
  deferred_entry_tracker_.ClearEntry(key);

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
  SPLOG("StartOpeningPanel - suppress_animations: " << suppress_animations);
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
  std::u16string_view title = SidePanelUtil::GetTitleText(entry, browser());

  JNIEnv* env = AttachCurrentThread();
  Java_SidePanelCoordinatorAndroidBridge_startOpeningPanel(
      env, java_coordinator(), browser()->GetProfile(), native_view->view(),
      title, entry->should_show_header(), suppress_animations);
  entry->CacheView(std::move(native_view));
}

void SidePanelCoordinatorAndroid::FinishOpeningPanel() {
  SPLOG("FinishOpeningPanel");
  CHECK(state_ == SidePanelState::kOpening)
      << "Should only call FinishOpeningPanel() when side panel is opening.";
  state_ = SidePanelState::kShown;
}

void SidePanelCoordinatorAndroid::StartClosingPanel(
    SidePanelEntryHideReason hide_reason,
    bool suppress_animations) {
  SPLOG("StartClosingPanel - hide_reason: " << ToString(hide_reason)
                                            << ", suppress_animations: "
                                            << suppress_animations);

  state_ = SidePanelState::kClosing;
  SidePanelEntry* entry = GetEntryForCurrentKeyNonNull();
  entry->OnEntryWillHide(hide_reason);
  pending_hide_reason_ = hide_reason;

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

  Java_SidePanelCoordinatorAndroidBridge_startClosingPanel(
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile(),
      suppress_animations);
}

void SidePanelCoordinatorAndroid::FinishClosingPanel() {
  SPLOG("FinishClosingPanel");
  CHECK(state_ == SidePanelState::kClosing)
      << "Should only call FinishClosingPanel() when side panel is closing.";

  SidePanelEntry* entry = GetEntryForCurrentKeyNonNull();

  SetCurrentKey(/*new_key=*/std::nullopt);

  // Now that the animation has completed, we can update our local state to be
  // closed, and trigger the entry hidden callbacks.
  if (pending_hide_reason_) {
    entry->OnEntryHidden();
    entry->OnEntryHiddenWithReason(*pending_hide_reason_);
    pending_hide_reason_ = std::nullopt;

    SidePanelMetrics::RecordSidePanelClosed(opened_timestamp());
  }

  state_ = SidePanelState::kClosed;
}

void SidePanelCoordinatorAndroid::StartReplacingPanelContent(
    SidePanelEntry* new_entry,
    const UniqueKey& new_key,
    SidePanelOpenTrigger open_trigger,
    std::unique_ptr<SidePanelNativeViewAndroid> native_view) {
  SPLOG("StartReplacingPanelContent.");

  // If there is already a pending replacement waiting for Java to finish, we
  // MUST synchronously complete it right now before we overwrite
  // `pending_replaced_entry_`. Otherwise, Java will synchronously complete it
  // later during this function call, but it will incorrectly invoke
  // OnEntryHidden() on the NEW pending_replaced_entry_ instead of the OLD one,
  // permanently breaking state!
  if (pending_replaced_entry_) {
    Java_SidePanelCoordinatorAndroidBridge_completePendingContentReplacement(
        AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
  }

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
  CHECK(!pending_replaced_entry_) << "Another entry is waiting to be replaced";
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
  std::u16string_view title = SidePanelUtil::GetTitleText(new_entry, browser());

  JNIEnv* env = AttachCurrentThread();
  Java_SidePanelCoordinatorAndroidBridge_startReplacingPanelContent(
      env, java_coordinator(), browser()->GetProfile(), native_view->view(),
      title, new_entry->should_show_header());
  new_entry->CacheView(std::move(native_view));
}

void SidePanelCoordinatorAndroid::EndAnimations() {
  Java_SidePanelCoordinatorAndroidBridge_endAnimations(
      AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
  CHECK(state_ == SidePanelState::kClosed || state_ == SidePanelState::kShown)
      << "Side panel should be in a stable state after ending all animations.";
}

void SidePanelCoordinatorAndroid::CompletePendingContentReplacementForTab(
    TabAndroid* tab) {
  if (auto* registry = SidePanelRegistry::From(tab)) {
    if (pending_replaced_entry_ &&
        registry->GetActiveEntry() == pending_replaced_entry_) {
      Java_SidePanelCoordinatorAndroidBridge_completePendingContentReplacement(
          AttachCurrentThread(), java_coordinator(), browser()->GetProfile());
    }
  }
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
        std::optional<UniqueKey> key_to_show =
            deferred_entry_tracker_.GetTabOrWindowScopedEntry(
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
    std::optional<UniqueKey> key_to_show =
        deferred_entry_tracker_.GetTabOrWindowScopedEntry(
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

void SidePanelCoordinatorAndroid::ClearCachedEntryViews(
    bool include_active_entry) {
  if (auto* window_registry = SidePanelRegistry::From(browser())) {
    window_registry->ClearCachedEntryViews(include_active_entry);
  }

  if (auto* tab_list = TabListInterface::From(browser())) {
    for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
      if (auto* registry = SidePanelRegistry::From(tab)) {
        registry->ClearCachedEntryViews(include_active_entry);
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
// Methods called from Java via SidePanelCoordinatorAndroidBridge.Natives:
// ----------------------------------------------------------------------------

// static
static int64_t JNI_SidePanelCoordinatorAndroidBridge_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t nativeBrowserWindowPtr) {
  SPLOG("JNI_SidePanelCoordinatorAndroidBridge_Create - ptr: "
        << nativeBrowserWindowPtr);
  return reinterpret_cast<intptr_t>(new SidePanelCoordinatorAndroid(
      env, caller,
      reinterpret_cast<BrowserWindowInterface*>(nativeBrowserWindowPtr)));
}

DEFINE_JNI(SidePanelCoordinatorAndroidBridge)
