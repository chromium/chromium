// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelCoordinatorAndroidImpl_jni.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_tab_list_observer_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_waiter.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

DEFINE_USER_DATA(SidePanelCoordinatorAndroid);

// static
SidePanelCoordinatorAndroid* SidePanelCoordinatorAndroid::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

// Implements Java `SidePanelCoordinatorAndroidImpl.Natives#create`.
static int64_t JNI_SidePanelCoordinatorAndroidImpl_Create(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t nativeBrowserWindowPtr) {
  return reinterpret_cast<intptr_t>(new SidePanelCoordinatorAndroid(
      env, caller,
      reinterpret_cast<BrowserWindowInterface*>(nativeBrowserWindowPtr)));
}

SidePanelCoordinatorAndroid::SidePanelCoordinatorAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& java_coordinator,
    BrowserWindowInterface* browser)
    : SidePanelUIBase(browser),
      java_coordinator_(env, java_coordinator),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this),
      tab_list_observer_(TabListInterface::From(browser), this) {}

SidePanelCoordinatorAndroid::~SidePanelCoordinatorAndroid() {
  Java_SidePanelCoordinatorAndroidImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_coordinator());
}

void SidePanelCoordinatorAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void SidePanelCoordinatorAndroid::ShowFrom(
    SidePanelEntryKey entry_key,
    gfx::Rect starting_bounds_in_browser_coordinates) {
  // TODO(crbug.com/494001629): Implement this.
}

void SidePanelCoordinatorAndroid::Close(SidePanelType panel_type,
                                        SidePanelEntryHideReason hide_reason,
                                        bool suppress_animations) {
  if (!IsSidePanelShowing(panel_type)) {
    return;
  }

  std::optional<UniqueKey> key = current_key(panel_type);
  CHECK(key) << "Current key should exist when side panel is showing.";

  SidePanelEntry* entry = GetEntryForUniqueKey(*key);
  CHECK(entry) << "SidePanelEntry should exist when side panel is showing.";

  entry->OnEntryWillHide(hide_reason);
  Java_SidePanelCoordinatorAndroidImpl_removeContent(
      base::android::AttachCurrentThread(), java_coordinator());

  // TODO(crbug.com/493930383): Clear current key and trigger OnEntryHidden()
  // when animation ends.
  SetCurrentKey(panel_type, /*new_key=*/std::nullopt);
  entry->OnEntryHidden();  // TODO(crbug.com/496962614): Remove OnEntryHidden().
  entry->OnEntryHiddenWithReason(hide_reason);
}

void SidePanelCoordinatorAndroid::Toggle(SidePanelEntryKey key,
                                         SidePanelOpenTrigger open_trigger) {
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
  SidePanelEntry* entry = GetEntryForUniqueKey(key);
  if (!entry) {
    return;
  }

  waiter(entry->type())
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
  std::unique_ptr<SidePanelNativeViewAndroid> native_view =
      content_view ? std::move(*content_view) : entry->GetContent();
  if (!native_view) {
    return;
  }

  // If the side panel isn't shown, just show it.
  if (!IsSidePanelShowing(entry->type())) {
    Java_SidePanelCoordinatorAndroidImpl_populateSidePanel(
        base::android::AttachCurrentThread(), java_coordinator(),
        native_view->view());
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
      base::android::AttachCurrentThread(), java_coordinator(),
      native_view->view());
  SetCurrentKey(entry->type(), unique_key);
  entry->OnEntryShown();
  previous_entry->OnEntryHidden();
  previous_entry->OnEntryHiddenWithReason(previous_entry_hide_reason);
}

void SidePanelCoordinatorAndroid::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  // TODO(crbug.com/494002625): Complete the full implementation.
  // The full implementation will need to:
  // consider all SidePanelTypes,
  // consider fallback logic, such as falling back to global entries,
  // etc.
  auto panel_type = SidePanelType::kContent;

  // If the side panel is showing, check if we should:
  // (1) replace the current UI content by calling `Show()`, or
  // (2) close the side panel by calling `Close()`.
  //
  // For (1), don't call `Close()` then `Show()`, which will cause janky UI.
  if (IsSidePanelShowing(panel_type)) {
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
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_coordinator_.get(env);

  CHECK(local_ref) << "Java SidePanelCoordinatorAndroid is the sole owner of "
                      "C++ SidePanelCoordinatorAndroid, so the Java object "
                      "shouldn't be destroyed before the C++ object";
  return local_ref;
}

DEFINE_JNI(SidePanelCoordinatorAndroidImpl)
