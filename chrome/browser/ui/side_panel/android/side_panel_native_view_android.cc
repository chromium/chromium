// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"

SidePanelNativeViewAndroid::SidePanelNativeViewAndroid(
    base::android::ScopedJavaGlobalRef<jobject> view)
    : view_(std::move(view)) {}

SidePanelNativeViewAndroid::~SidePanelNativeViewAndroid() = default;
