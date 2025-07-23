// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_base_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBaseWindow_jni.h"
#include "ui/gfx/geometry/rect.h"

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
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
  NOTREACHED();
}

bool AndroidBaseWindow::IsMaximized() const {
  NOTREACHED();
}

bool AndroidBaseWindow::IsMinimized() const {
  NOTREACHED();
}

bool AndroidBaseWindow::IsFullscreen() const {
  NOTREACHED();
}

gfx::NativeWindow AndroidBaseWindow::GetNativeWindow() const {
  NOTREACHED();
}

gfx::Rect AndroidBaseWindow::GetRestoredBounds() const {
  NOTREACHED();
}

ui::mojom::WindowShowState AndroidBaseWindow::GetRestoredState() const {
  NOTREACHED();
}

gfx::Rect AndroidBaseWindow::GetBounds() const {
  NOTREACHED();
}

void AndroidBaseWindow::Show() {
  NOTREACHED();
}

void AndroidBaseWindow::Hide() {
  NOTREACHED();
}

bool AndroidBaseWindow::IsVisible() const {
  NOTREACHED();
}

void AndroidBaseWindow::ShowInactive() {
  NOTREACHED();
}

void AndroidBaseWindow::Close() {
  NOTREACHED();
}

void AndroidBaseWindow::Activate() {
  NOTREACHED();
}

void AndroidBaseWindow::Deactivate() {
  NOTREACHED();
}

void AndroidBaseWindow::Maximize() {
  NOTREACHED();
}

void AndroidBaseWindow::Minimize() {
  NOTREACHED();
}

void AndroidBaseWindow::Restore() {
  NOTREACHED();
}

void AndroidBaseWindow::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void AndroidBaseWindow::FlashFrame(bool flash) {
  NOTREACHED();
}

ui::ZOrderLevel AndroidBaseWindow::GetZOrderLevel() const {
  NOTREACHED();
}

void AndroidBaseWindow::SetZOrderLevel(ui::ZOrderLevel order) {
  NOTREACHED();
}
