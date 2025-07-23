// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BASE_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BASE_WINDOW_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "ui/base/base_window.h"

// Android implementation of |ui::BaseWindow|.
class AndroidBaseWindow final : public ui::BaseWindow {
 public:
  AndroidBaseWindow(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_android_base_window);
  AndroidBaseWindow(const AndroidBaseWindow&) = delete;
  AndroidBaseWindow& operator=(const AndroidBaseWindow&) = delete;
  ~AndroidBaseWindow();

  // Implements Java |AndroidBaseWindow.Natives#destroy|.
  void Destroy(JNIEnv* env);

  // Implements |ui::BaseWindow|.
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  void ShowInactive() override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_android_base_window_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_INTERNAL_ANDROID_ANDROID_BASE_WINDOW_H_
