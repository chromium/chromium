// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "chrome/browser/autofill/android/at_memory_bottom_sheet_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AtMemoryBottomSheetBridge_jni.h"

namespace autofill {

AtMemoryBottomSheetBridge::AtMemoryBottomSheetBridge(
    ui::WindowAndroid* window_android) {
  CHECK(window_android);
  java_object_ = Java_AtMemoryBottomSheetBridge_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      window_android->GetJavaObject());
}

AtMemoryBottomSheetBridge::~AtMemoryBottomSheetBridge() {
  if (java_object_) {
    Java_AtMemoryBottomSheetBridge_destroy(base::android::AttachCurrentThread(),
                                           java_object_);
  }
}

void AtMemoryBottomSheetBridge::RequestShowContent(
    std::unique_ptr<AtMemoryBottomSheetDelegate> delegate) {
  delegate_ = std::move(delegate);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AtMemoryBottomSheetBridge_show(env, java_object_);
}

void AtMemoryBottomSheetBridge::OnDismissed(JNIEnv* env) {
  if (delegate_) {
    delegate_->OnDismissed();
  }
  ResetDelegate();
}

void AtMemoryBottomSheetBridge::ResetDelegate() {
  delegate_.reset();
}

}  // namespace autofill

DEFINE_JNI(AtMemoryBottomSheetBridge)
