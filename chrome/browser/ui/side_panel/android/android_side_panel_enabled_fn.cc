// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"

#include "base/android/jni_android.h"
#include "chrome/browser/ui/side_panel/android/jni_headers/AndroidSidePanelEnabledFn_jni.h"

// static
bool AndroidSidePanelEnabledFn::IsEnabled() {
  return Java_AndroidSidePanelEnabledFn_isEnabled(
      base::android::AttachCurrentThread());
}

DEFINE_JNI(AndroidSidePanelEnabledFn)
