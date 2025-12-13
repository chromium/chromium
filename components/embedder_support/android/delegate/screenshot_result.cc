// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/delegate/screenshot_result.h"

#include <android/hardware_buffer_jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/web_contents_delegate_jni_headers/ScreenshotResult_jni.h"

namespace web_contents_delegate_android {

using base::android::ScopedHardwareBufferHandle;
using base::android::ScopedJavaGlobalRef;

ScreenshotResult::ScreenshotResult(const jni_zero::JavaRef<jobject>& obj)
    : java_screenshot_result_(obj) {}

ScreenshotResult::~ScreenshotResult() = default;

ScreenshotResult::operator bool() const {
  return !java_screenshot_result_.is_null();
}

SkBitmap ScreenshotResult::GetBitmap() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  gfx::JavaBitmap j_bitmap(
      Java_ScreenshotResult_getBitmap(env, java_screenshot_result_));
  return gfx::CreateSkBitmapFromJavaBitmap(j_bitmap);
}

ScopedHardwareBufferHandle ScreenshotResult::GetHardwareBuffer() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_hardware_buffer =
      Java_ScreenshotResult_getHardwareBuffer(env, java_screenshot_result_);
  return ScopedHardwareBufferHandle::Create(
      AHardwareBuffer_fromHardwareBuffer(env, j_hardware_buffer.obj()));
}

base::ScopedClosureRunner ScreenshotResult::GetReleaseCallback() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_release_callback = ScopedJavaGlobalRef(
      Java_ScreenshotResult_getReleaseCallback(env, java_screenshot_result_));
  return base::ScopedClosureRunner(base::BindOnce(
      [](ScopedJavaGlobalRef<jobject> j_release_callback) {
        base::android::RunRunnableAndroid(j_release_callback);
      },
      std::move(j_release_callback)));
}

}  // namespace web_contents_delegate_android

DEFINE_JNI(ScreenshotResult)
