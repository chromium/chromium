// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_CONTAINER_INTERNAL_ANDROID_DEV_SIDE_PANEL_WINDOW_SCOPED_DEV_FEATURE_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_CONTAINER_INTERNAL_ANDROID_DEV_SIDE_PANEL_WINDOW_SCOPED_DEV_FEATURE_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "third_party/jni_zero/jni_zero.h"

class BrowserWindowInterface;
class SidePanelEntryScope;

// The native counterpart of the Java `SidePanelWindowScopedDevFeatureImpl`.
class SidePanelWindowScopedDevFeature {
 public:
  SidePanelWindowScopedDevFeature(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& java_dev_feature,
      BrowserWindowInterface* browser_window);
  ~SidePanelWindowScopedDevFeature();

  SidePanelWindowScopedDevFeature(const SidePanelWindowScopedDevFeature&) =
      delete;
  SidePanelWindowScopedDevFeature& operator=(
      const SidePanelWindowScopedDevFeature&) = delete;

  // Implements Java `SidePanelWindowScopedDevFeatureImpl.Natives`.
  void Toggle();
  void Destroy();

 private:
  SidePanelNativeView GetOrCreateView(SidePanelEntryScope& scope) const;
  jni_zero::ScopedJavaLocalRef<jobject> java_dev_feature() const;

  const jni_zero::ScopedJavaGlobalWeakRef java_dev_feature_;
  const raw_ptr<BrowserWindowInterface> browser_window_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_CONTAINER_INTERNAL_ANDROID_DEV_SIDE_PANEL_WINDOW_SCOPED_DEV_FEATURE_H_
