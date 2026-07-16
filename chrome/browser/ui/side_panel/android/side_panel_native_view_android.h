// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_SIDE_PANEL_NATIVE_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_SIDE_PANEL_NATIVE_VIEW_ANDROID_H_

#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/class_property.h"

class SidePanelNativeViewAndroid : public ui::PropertyHandler {
 public:
  explicit SidePanelNativeViewAndroid(const jni_zero::JavaRef<jobject>& view);
  ~SidePanelNativeViewAndroid() override;

  const jni_zero::JavaRef<jobject>& view() const { return view_; }

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> view_;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_ANDROID_SIDE_PANEL_NATIVE_VIEW_ANDROID_H_
