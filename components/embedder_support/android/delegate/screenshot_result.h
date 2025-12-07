// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_SCREENSHOT_RESULT_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_SCREENSHOT_RESULT_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/stack_allocated.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/ui_android_export.h"

namespace base {
class ScopedClosureRunner;
}  // namespace base

class SkBitmap;

namespace web_contents_delegate_android {

// This class is a mirror of
// org.chromium.components.embedder_support.delegate.ScreenshotResult.
// It uses ScopedJavaLocalRef, meaning that the class is only usable as
// a stack-based object in a single thread.
class ScreenshotResult {
  STACK_ALLOCATED();

 public:
  explicit ScreenshotResult(const jni_zero::JavaRef<jobject>& obj);
  ScreenshotResult(ScreenshotResult&& other) = delete;
  ScreenshotResult(const ScreenshotResult&) = delete;
  ScreenshotResult operator=(const ScreenshotResult&) = delete;
  ~ScreenshotResult();

  // Returns whether there is a non-null result.
  explicit operator bool() const;

  SkBitmap GetBitmap() const;

  base::android::ScopedHardwareBufferHandle GetHardwareBuffer() const;

  base::ScopedClosureRunner GetReleaseCallback() const;

 private:
  base::android::ScopedJavaLocalRef<jobject> java_screenshot_result_;
};

}  // namespace web_contents_delegate_android

namespace jni_zero {

template <>
inline web_contents_delegate_android::ScreenshotResult FromJniType(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  return web_contents_delegate_android::ScreenshotResult(obj);
}

}  // namespace jni_zero

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_SCREENSHOT_RESULT_H_
