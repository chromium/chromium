// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/delegate/eye_dropper_bridge.h"

#include "base/android/android_info.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/ui_base_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/web_contents_delegate_jni/EyeDropperBridge_jni.h"

namespace web_contents_delegate_android {

// static
std::unique_ptr<EyeDropperBridge> EyeDropperBridge::Create(
    content::WebContents* web_contents,
    content::EyeDropperListener* listener) {
  if (!features::IsEyeDropperEnabled()) {
    return nullptr;
  }

  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_CINNAMON_BUN) {
    // Eyedropper is only supported on cinnamon bun or newer.
    return nullptr;
  }

  if (!web_contents ||
      web_contents->GetVisibility() != content::Visibility::VISIBLE ||
      !web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->HasFocus()) {
    return nullptr;
  }

  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  if (!window_android) {
    return nullptr;
  }

  auto bridge = base::WrapUnique(new EyeDropperBridge(listener));
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_eye_dropper =
      Java_EyeDropperBridge_create(
          env, reinterpret_cast<intptr_t>(bridge.get()), window_android);
  if (j_eye_dropper.is_null()) {
    return nullptr;
  }

  bridge->j_eye_dropper_.Reset(j_eye_dropper);
  return bridge;
}

EyeDropperBridge::EyeDropperBridge(content::EyeDropperListener* listener)
    : listener_(listener) {}

EyeDropperBridge::~EyeDropperBridge() {
  if (!j_eye_dropper_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_EyeDropperBridge_destroy(env, j_eye_dropper_);
  }
}

void EyeDropperBridge::OnColorChosen(JNIEnv* env, int32_t color) {
  if (listener_) {
    content::EyeDropperListener* listener = listener_;
    listener_ = nullptr;
    listener->ColorSelected(static_cast<SkColor>(color));
    // `EyeDropperChooserImpl::ColorSelected()` resets its `eye_dropper_`
    // unique_ptr, so `this` may be deleted here.
  }
}

void EyeDropperBridge::OnColorSelectionCanceled(JNIEnv* env) {
  if (listener_) {
    content::EyeDropperListener* listener = listener_;
    listener_ = nullptr;
    listener->ColorSelectionCanceled();
    // `EyeDropperChooserImpl::ColorSelectionCanceled()` resets its
    // `eye_dropper_` unique_ptr, so `this` may be deleted here.
  }
}

}  // namespace web_contents_delegate_android

DEFINE_JNI(EyeDropperBridge)
