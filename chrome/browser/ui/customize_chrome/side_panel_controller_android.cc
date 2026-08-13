// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/customize_chrome/side_panel_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all other headers.
#include "chrome/browser/ui/customize_chrome/jni_headers/CustomizeChromeSidePanelHelper_jni.h"

namespace customize_chrome {

SidePanelControllerAndroid::SidePanelControllerAndroid(tabs::TabInterface& tab)
    : SidePanelControllerBase(tab) {}

SidePanelControllerAndroid::~SidePanelControllerAndroid() = default;

SidePanelNativeView SidePanelControllerAndroid::CreateCustomizeChromeView(
    SidePanelEntryScope& scope) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::WindowAndroid* window_android =
      tab_->GetContents()->GetTopLevelNativeWindow();
  CHECK(window_android);

  base::android::ScopedJavaLocalRef<jobject> j_view =
      chrome::android::Java_CustomizeChromeSidePanelHelper_createPlaceholder(
          env, window_android->GetJavaObject());
  CHECK(j_view);

  return std::make_unique<SidePanelNativeViewAndroid>(j_view);
}
}  // namespace customize_chrome
