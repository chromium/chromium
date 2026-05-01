// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/lens/lens_overlay_controller_android.h"

#include "base/android/jni_android.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/lens/jni_headers/LensOverlayCoordinator_jni.h"

using jni_zero::JavaRef;

int64_t JNI_LensOverlayCoordinator_Init(JNIEnv* env,
                                        const JavaRef<jobject>& obj,
                                        content::WebContents* web_contents) {
  return reinterpret_cast<intptr_t>(
      new lens::LensOverlayControllerAndroid(env, obj, web_contents));
}

namespace lens {

LensOverlayControllerAndroid::LensOverlayControllerAndroid(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    content::WebContents* web_contents)
    : java_obj_(env, obj), web_contents_(web_contents) {}

LensOverlayControllerAndroid::~LensOverlayControllerAndroid() = default;

void LensOverlayControllerAndroid::ShowUI(JNIEnv* env,
                                          int32_t invocation_source) {
  // TODO(b/493627069): Pass actual mime type when extracted.
  lens::RecordInvocation(
      static_cast<lens::LensOverlayInvocationSource>(invocation_source),
      lens::MimeType::kHtml);
}

void LensOverlayControllerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace lens

DEFINE_JNI(LensOverlayCoordinator)
