// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/lens/lens_overlay_controller_android.h"

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/snapshot/snapshot.h"

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

bool LensOverlayControllerAndroid::ShowUI(JNIEnv* env,
                                          int32_t invocation_source) {
  // TODO(b/493627069): Pass actual mime type when extracted.
  lens::RecordInvocation(
      static_cast<lens::LensOverlayInvocationSource>(invocation_source),
      lens::MimeType::kHtml);

  gfx::NativeWindow window = web_contents_->GetTopLevelNativeWindow();
  if (!window) {
    return false;
  }

  // Invalidate any in-flight screenshot requests to ensure we only process the
  // result of the most recent call. (The underlying GPU capture will still run,
  // but the callback will be dropped).
  weak_ptr_factory_.InvalidateWeakPtrs();

  ui::GrabWindowSnapshot(
      window, gfx::Rect(),
      base::BindOnce(&LensOverlayControllerAndroid::OnScreenshotCaptured,
                     weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void LensOverlayControllerAndroid::OnScreenshotCaptured(gfx::Image snapshot) {
  if (snapshot.IsEmpty()) {
    // If the capture fails, we log and return. This silently aborts the flow,
    // which is the intended behavior for the prototype.
    LOG(ERROR) << "Failed to capture window snapshot";
    if (java_obj_) {
      Java_LensOverlayCoordinator_onCaptureError(
          base::android::AttachCurrentThread(), java_obj_);
    }
    return;
  }

  SkBitmap bitmap = snapshot.AsBitmap();
  if (java_obj_) {
    Java_LensOverlayCoordinator_onScreenshotCaptured(
        base::android::AttachCurrentThread(), java_obj_, bitmap);
  }
}

void LensOverlayControllerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace lens

DEFINE_JNI(LensOverlayCoordinator)
