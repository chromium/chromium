// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelCoordinatorAndroidImpl_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

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
    : SidePanelUIBase(browser), java_coordinator_(env, java_coordinator) {}

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

void SidePanelCoordinatorAndroid::Close(SidePanelEntry::PanelType panel_type,
                                        SidePanelEntryHideReason hide_reason,
                                        bool suppress_animations) {
  // TODO(crbug.com/493930383): Implement this.
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
  // TODO(crbug.com/494000480): Implement this.
}

void SidePanelCoordinatorAndroid::Show(
    const UniqueKey& entry,
    std::optional<SidePanelOpenTrigger> open_trigger,
    bool suppress_animations) {
  // TODO(crbug.com/493931047): Implement this.
}

void SidePanelCoordinatorAndroid::PopulateSidePanel(
    bool suppress_animations,
    const UniqueKey& unique_key,
    std::optional<SidePanelOpenTrigger> open_trigger,
    SidePanelEntry* entry,
    std::optional<SidePanelNativeView> content_view) {
  // TODO(crbug.com/494001968): Implement this.
}

void SidePanelCoordinatorAndroid::MaybeShowEntryOnTabStripModelChanged(
    SidePanelRegistry* old_contextual_registry,
    SidePanelRegistry* new_contextual_registry) {
  // TODO(crbug.com/494002625): Implement this.
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
