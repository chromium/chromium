// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_base_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBaseWindow_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
}  // namespace

// Implements Java |AndroidBaseWindow.Natives#create|.
static jlong JNI_AndroidBaseWindow_Create(JNIEnv* env,
                                          const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(new AndroidBaseWindow(env, caller));
}

AndroidBaseWindow::AndroidBaseWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_android_base_window) {
  java_android_base_window_.Reset(env, java_android_base_window);
}

AndroidBaseWindow::~AndroidBaseWindow() {
  Java_AndroidBaseWindow_clearNativePtr(AttachCurrentThread(),
                                        java_android_base_window_);
}

void AndroidBaseWindow::Destroy(JNIEnv* env) {
  delete this;
}

bool AndroidBaseWindow::IsActive() const {
  return Java_AndroidBaseWindow_isActive(AttachCurrentThread(),
                                         java_android_base_window_);
}

bool AndroidBaseWindow::IsMaximized() const {
  return Java_AndroidBaseWindow_isMaximized(AttachCurrentThread(),
                                            java_android_base_window_);
}

bool AndroidBaseWindow::IsMinimized() const {
  return Java_AndroidBaseWindow_isMinimized(AttachCurrentThread(),
                                            java_android_base_window_);
}

bool AndroidBaseWindow::IsFullscreen() const {
  return Java_AndroidBaseWindow_isFullscreen(AttachCurrentThread(),
                                             java_android_base_window_);
}

gfx::NativeWindow AndroidBaseWindow::GetNativeWindow() const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_window_android =
      Java_AndroidBaseWindow_getWindowAndroid(env, java_android_base_window_);
  return ui::WindowAndroid::FromJavaWindowAndroid(j_window_android);
}

gfx::Rect AndroidBaseWindow::GetRestoredBounds() const {
  std::vector<int> sizes = Java_AndroidBaseWindow_getRestoredBounds(
      AttachCurrentThread(), java_android_base_window_);
  gfx::Rect bounds = gfx::Rect(
      /*x=*/sizes[0], /*y=*/sizes[1], /*width=*/sizes[2], /*height=*/sizes[3]);
  return bounds;
}

ui::mojom::WindowShowState AndroidBaseWindow::GetRestoredState() const {
  NOTREACHED();
}

gfx::Rect AndroidBaseWindow::GetBounds() const {
  std::vector<int> sizes = Java_AndroidBaseWindow_getBounds(
      AttachCurrentThread(), java_android_base_window_);
  gfx::Rect bounds = gfx::Rect(
      /*x=*/sizes[0], /*y=*/sizes[1], /*width=*/sizes[2], /*height=*/sizes[3]);
  return bounds;
}

void AndroidBaseWindow::Show() {
  Java_AndroidBaseWindow_show(AttachCurrentThread(), java_android_base_window_);
}

void AndroidBaseWindow::Hide() {
  NOTREACHED();
}

bool AndroidBaseWindow::IsVisible() const {
  return Java_AndroidBaseWindow_isVisible(AttachCurrentThread(),
                                          java_android_base_window_);
}

void AndroidBaseWindow::ShowInactive() {
  Java_AndroidBaseWindow_showInactive(AttachCurrentThread(),
                                      java_android_base_window_);
}

void AndroidBaseWindow::Close() {
  Java_AndroidBaseWindow_close(AttachCurrentThread(),
                               java_android_base_window_);
}

void AndroidBaseWindow::Activate() {
  Java_AndroidBaseWindow_activate(AttachCurrentThread(),
                                  java_android_base_window_);
}

void AndroidBaseWindow::Deactivate() {
  Java_AndroidBaseWindow_deactivate(AttachCurrentThread(),
                                    java_android_base_window_);
}

void AndroidBaseWindow::Maximize() {
  Java_AndroidBaseWindow_maximize(AttachCurrentThread(),
                                  java_android_base_window_);
}

void AndroidBaseWindow::Minimize() {
  Java_AndroidBaseWindow_minimize(AttachCurrentThread(),
                                  java_android_base_window_);
}

void AndroidBaseWindow::Restore() {
  Java_AndroidBaseWindow_restore(AttachCurrentThread(),
                                 java_android_base_window_);
}

void AndroidBaseWindow::SetBounds(const gfx::Rect& bounds) {
  Java_AndroidBaseWindow_setBounds(AttachCurrentThread(),
                                   java_android_base_window_, bounds.x(),
                                   bounds.y(), bounds.right(), bounds.bottom());
}

void AndroidBaseWindow::FlashFrame(bool flash) {
  // As of Sep 16, 2025, Android OS didn't support |FlashFrame|.
}

ui::ZOrderLevel AndroidBaseWindow::GetZOrderLevel() const {
  return ui::ZOrderLevel::kNormal;
}

void AndroidBaseWindow::SetZOrderLevel(ui::ZOrderLevel order) {
  // Android doesn't support |SetZOrderLevel|.
  // Per documentation of |ui::ZOrderLevel|, Android ZOrderLevel should always
  // be |ui::ZOrderLevel::kNormal|.
}

DEFINE_JNI(AndroidBaseWindow)
