// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_NATIVE_VIEW_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_NATIVE_VIEW_H_

#include <memory>

#include "build/build_config.h"
#include "ui/base/class_property.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "third_party/jni_zero/jni_zero.h"
#else
#include "ui/views/view.h"
#endif

#if BUILDFLAG(IS_ANDROID)
class SidePanelNativeViewAndroid : public ui::PropertyHandler {
 public:
  explicit SidePanelNativeViewAndroid(
      base::android::ScopedJavaGlobalRef<jobject> view)
      : view_(std::move(view)) {}
  ~SidePanelNativeViewAndroid() override = default;

  base::android::ScopedJavaGlobalRef<jobject> view() const { return view_; }

 private:
  base::android::ScopedJavaGlobalRef<jobject> view_;
};

using SidePanelNativeView = std::unique_ptr<SidePanelNativeViewAndroid>;
#else
using SidePanelNativeView = std::unique_ptr<views::View>;
#endif

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_NATIVE_VIEW_H_
