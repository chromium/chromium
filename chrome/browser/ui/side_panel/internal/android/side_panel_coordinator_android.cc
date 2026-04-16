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

#define LOG_TAG "SidePanelCoordinatorAndroid"
#define SPLOG(message)                                     \
  if (base::FeatureList::IsEnabled(                        \
          chrome::android::kEnableAndroidSidePanelLogs)) { \
    LOG(ERROR) << LOG_TAG << ": " << message;              \
  }

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
  SPLOG("NotifyOpenAnimationFinished - panel_type: "
        << static_cast<int>(panel_type));

  CHECK(state_ == SidePanelState::kOpening)
      << "Should only receive open animation finished callback when side "
         "panel is opening.";

  state_ = SidePanelState::kShown;
}

void SidePanelCoordinatorAndroid::NotifyCloseAnimationFinished(
    JNIEnv* env,
    SidePanelType panel_type) {
  SPLOG("NotifyCloseAnimationFinished - panel_type: "
        << static_cast<int>(panel_type));

  CHECK(IsClosing())
      << "Should only receive close animation finished callback when side "
         "panel is closing.";

  std::optional<UniqueKey> key = current_key(panel_type);
  CHECK(key) << "Current key should exist when side panel animation finishes.";

  SidePanelEntry* entry = GetEntryForUniqueKey(*key);
  CHECK(entry)
      << "Current entry should still exist when side panel is closing.";

  SetCurrentKey(panel_type, /*new_key=*/std::nullopt);
  // Now that the animation has completed, we can update our local state to be
  // closed, and trigger the entry hidden callbacks.
  entry->OnEntryHidden();
  entry->OnEntryHiddenWithReason(pending_hide_reason_);

  // TODO(crbug.com/493931023): Record metrics here
  // (SidePanelMetrics::RecordSidePanelClosed).

  state_ = SidePanelState::kClosed;
}

void SidePanelCoordinatorAndroid::ShowFrom(
    SidePanelEntryKey entry_key,
    gfx::Rect starting_bounds_in_browser_coordinates) {
  SPLOG("ShowFrom - entry_key: "
        << entry_key.ToString() << ", starting_bounds: "
        << starting_bounds_in_browser_coordinates.ToString());
  // TODO(crbug.com/494001629): Implement this.
}

void SidePanelCoordinatorAndroid::Close(SidePanelType panel_type,
                                        SidePanelEntryHideReason hide_reason,
                                        bool suppress_animations) {
  SPLOG("Close - panel_type: "
        << static_cast<int>(panel_type)
        << ", hide_reason: " << static_cast<int>(hide_reason)
        << ", suppress_animations: " << suppress_animations);
  CHECK(state_ == SidePanelState::kOpening || state_ == SidePanelState::kShown)
      << "Close calls should only occur for opening or shown side panels. "
         "Current state: "
      << static_cast<int>(state_);

  // Stop any pending load.
  waiter(panel_type)->ResetLoadingEntryIfNecessary();

  if (!IsSidePanelShowing(panel_type)) {
    return;
  }

  std::optional<UniqueKey> key = current_key(panel_type);
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

void SidePanelCoordinatorAndroid::Toggle(SidePanelEntryKey key,
                                         SidePanelOpenTrigger open_trigger) {
  SPLOG("Toggle - key: " << key.ToString() << ", open_trigger: "
                         << static_cast<int>(open_trigger));

  // TODO(crbug.com/493931022): Implement this.
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
  for (auto type : SidePanelTypes::All()) {
    waiter(type)->SetNoDelaysForTesting(no_delays_for_testing);  // IN-TEST
  }
}

void SidePanelCoordinatorAndroid::Show(
    const UniqueKey& key,
    std::optional<SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  SPLOG("Show - key: " << key << ", open_trigger: "
                       << (open_trigger ? static_cast<int>(*open_trigger) : -1)
                       << ", suppress_animations: " << suppress_animations);
  SidePanelEntry* entry = GetEntryForUniqueKey(key);
  if (!entry) {
    return;
  }

  SidePanelType entry_type = entry->type();
  if (IsSidePanelShowing(entry_type)) {
    std::optional<UniqueKey> current_entry_key = current_key(entry_type);
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
      SPLOG("Show - entry is already visible.");
      waiter(entry_type)->ResetLoadingEntryIfNecessary();

      // TODO(crbug.com/493931047): Handle the case where the current entry is
      // being closed, i.e., when `state_` is `SidePanelState::kClosing`.
      // In this case, we should:
      //   (1) stop the closing animation and keep the side panel open, and
      //   (2) notify the entry of `OnEntryHideCancelled()`.
      return;
    }
  }

  waiter(entry_type)
      ->WaitForEntry(
          entry, base::BindOnce(&SidePanelCoordinatorAndroid::PopulateSidePanel,
                                base::Unretained(this), suppress_animations,
                                key, open_trigger));
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
    return;
  }

  // TOOD(crbug.com/494001968): Handle suppressed animations case.
  state_ = SidePanelState::kShown;

  // If the side panel isn't shown, just show it.
  if (!IsSidePanelShowing(entry->type())) {
    Java_SidePanelCoordinatorAndroidImpl_populateSidePanel(
        AttachCurrentThread(), java_coordinator(), native_view->view());
    SetCurrentKey(entry->type(), unique_key);
    entry->OnEntryShown();
    return;
  }

  // Otherwise, replace the UI contents.
  //
  // Note: when we replace the side panel's UI contents, no animation should be
  // played. However, we can't CHECK(suppress_animations) as the side panel
  // feature calling Show() may not be aware of the current side panel state.
  std::optional<UniqueKey> key = current_key(entry->type());
  CHECK(key) << "Current key should exist when side panel is showing.";

  SidePanelEntry* previous_entry = GetEntryForUniqueKey(*key);
  CHECK(previous_entry)
      << "SidePanelEntry should exist when side panel is showing.";

  auto previous_entry_hide_reason =
      (open_trigger && *open_trigger == SidePanelOpenTrigger::kTabChanged)
          ? SidePanelEntryHideReason::kBackgrounded
          : SidePanelEntryHideReason::kReplaced;

  previous_entry->OnEntryWillHide(previous_entry_hide_reason);
  Java_SidePanelCoordinatorAndroidImpl_populateSidePanel(
      AttachCurrentThread(), java_coordinator(), native_view->view());
  SetCurrentKey(entry->type(), unique_key);
  entry->OnEntryShown();
  previous_entry->OnEntryHidden();
  previous_entry->OnEntryHiddenWithReason(previous_entry_hide_reason);
}

void SidePanelCoordinatorAndroid::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  SPLOG("MaybeShowEntryOnTabStripModelChanged - old_contextual_registry: "
        << old_contextual_registry
        << ", new_contextual_registry: " << new_contextual_registry);

  // We only have the toolbar type on Android.
  auto panel_type = SidePanelType::kToolbar;

  // If the side panel is showing, check if we should:
  // (1) replace the current UI content by calling `Show()`, or
  // (2) close the side panel by calling `Close()`.
  //
  // For (1), don't call `Close()` then `Show()`, which will cause janky UI.
  if (IsSidePanelShowing(panel_type) && state_ != SidePanelState::kClosing) {
    std::optional<UniqueKey> new_active_key =
        GetNewActiveKeyOnTabChanged(panel_type);

    if (new_active_key) {
      Show(*new_active_key, SidePanelOpenTrigger::kTabChanged,
           /*suppress_animations=*/true);
    } else {
      std::optional<UniqueKey> key = current_key(panel_type);
      CHECK(key) << "Current key should exist when side panel is showing.";

      if (old_contextual_registry &&
          old_contextual_registry->GetTabInterface().GetHandle() ==
              key->tab_handle) {
        Close(panel_type, SidePanelEntryHideReason::kBackgrounded,
              /*suppress_animations=*/true);
      }
    }

    return;
  }

  // If the side panel isn't showing, check if we should show it.
  std::optional<SidePanelEntry*> new_active_entry =
      new_contextual_registry
          ? new_contextual_registry->GetActiveEntryFor(panel_type)
          : nullptr;
  if (new_active_entry) {
    UniqueKey key{new_contextual_registry->GetTabInterface().GetHandle(),
                  (*new_active_entry)->key()};
    Show(key, SidePanelOpenTrigger::kTabChanged, /*suppress_animations=*/true);
  }
}

ScopedJavaLocalRef<jobject> SidePanelCoordinatorAndroid::java_coordinator()
    const {
  SPLOG("java_coordinator()");
  ScopedJavaLocalRef<jobject> local_ref =
      java_coordinator_.get(AttachCurrentThread());

  CHECK(local_ref) << "Java SidePanelCoordinatorAndroid is the sole owner of "
                      "C++ SidePanelCoordinatorAndroid, so the Java object "
                      "shouldn't be destroyed before the C++ object";
  return local_ref;
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
