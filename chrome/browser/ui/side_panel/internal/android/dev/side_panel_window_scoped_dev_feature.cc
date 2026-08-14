// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/dev/side_panel_window_scoped_dev_feature.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelWindowScopedDevFeatureImpl_jni.h"

namespace {
using jni_zero::AttachCurrentThread;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;
}  // namespace

static int64_t JNI_SidePanelWindowScopedDevFeatureImpl_Init(
    JNIEnv* env,
    const JavaRef<jobject>& caller,
    int64_t nativeBrowserWindowPtr) {
  return reinterpret_cast<intptr_t>(new SidePanelWindowScopedDevFeature(
      env, caller,
      reinterpret_cast<BrowserWindowInterface*>(nativeBrowserWindowPtr)));
}

SidePanelWindowScopedDevFeature::SidePanelWindowScopedDevFeature(
    JNIEnv* env,
    const JavaRef<jobject>& java_dev_feature,
    BrowserWindowInterface* browser_window)
    : java_dev_feature_(env, java_dev_feature),
      browser_window_(browser_window) {
  CHECK(browser_window_);

  auto* registry = SidePanelRegistry::From(browser_window_);
  CHECK(registry);

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntry::Id::kSidePanelDev),
      base::BindRepeating(&SidePanelWindowScopedDevFeature::GetOrCreateView,
                          base::Unretained(this)),
      base::BindRepeating([]() { return 0; }));
  entry->SetProperty(kSidePanelTitleKey,
                     std::u16string(u"Developer Panel (Window Scoped)"));
  registry->Register(std::move(entry));
}

SidePanelWindowScopedDevFeature::~SidePanelWindowScopedDevFeature() {
  if (auto* registry = SidePanelRegistry::From(browser_window_)) {
    registry->Deregister(
        SidePanelEntry::Key(SidePanelEntry::Id::kSidePanelDev));
  }
}

void SidePanelWindowScopedDevFeature::Toggle() {
  auto* side_panel_coordinator =
      SidePanelCoordinatorAndroid::From(browser_window_);
  CHECK(side_panel_coordinator);

  side_panel_coordinator->Toggle(
      SidePanelEntryKey(SidePanelEntry::Id::kSidePanelDev),
      SidePanelOpenTrigger::kToolbarButton);
}

void SidePanelWindowScopedDevFeature::Destroy() {
  delete this;
}

SidePanelNativeView SidePanelWindowScopedDevFeature::GetOrCreateView(
    SidePanelEntryScope& scope) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> view =
      Java_SidePanelWindowScopedDevFeatureImpl_getOrCreateView(
          env, java_dev_feature());
  CHECK(view);
  return std::make_unique<SidePanelNativeViewAndroid>(view);
}

ScopedJavaLocalRef<jobject> SidePanelWindowScopedDevFeature::java_dev_feature()
    const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> local_ref = java_dev_feature_.get(env);
  CHECK(local_ref);
  return local_ref;
}

DEFINE_JNI(SidePanelWindowScopedDevFeatureImpl)
