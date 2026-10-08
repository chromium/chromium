// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_EYE_DROPPER_BRIDGE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_EYE_DROPPER_BRIDGE_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"

namespace content {
class WebContents;
}

namespace web_contents_delegate_android {

// Bridge between the Android system EyeDropper intent and content::EyeDropper.
class EyeDropperBridge : public content::EyeDropper {
 public:
  static std::unique_ptr<EyeDropperBridge> Create(
      content::WebContents* web_contents,
      content::EyeDropperListener* listener);

  EyeDropperBridge(const EyeDropperBridge&) = delete;
  EyeDropperBridge& operator=(const EyeDropperBridge&) = delete;
  ~EyeDropperBridge() override;

  // Called from Java via JNI when a color is chosen.
  void OnColorChosen(JNIEnv* env, int32_t color);

  // Called from Java via JNI when color selection is canceled.
  void OnColorSelectionCanceled(JNIEnv* env);

 private:
  explicit EyeDropperBridge(content::EyeDropperListener* listener);

  raw_ptr<content::EyeDropperListener> listener_;
  base::android::ScopedJavaGlobalRef<jobject> j_eye_dropper_;
};

}  // namespace web_contents_delegate_android

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_DELEGATE_EYE_DROPPER_BRIDGE_H_
