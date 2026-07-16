// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/lens/lens_overlay_controller_android.h"

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/lens/jni_headers/LensOverlayCoordinator_jni.h"

using jni_zero::JavaRef;

namespace {
// Maximum time to wait for the renderer surface to produce a result.
constexpr base::TimeDelta kCaptureTimeout = base::Seconds(2);
}  // namespace

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

  weak_ptr_factory_.InvalidateWeakPtrs();

  content::RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (!rwhv || !rwhv->IsSurfaceAvailableForCopy()) {
    return false;
  }

  // Increment the capturer count to keep the WebContents active and visible
  // during the asynchronous capture process.
  scoped_capturer_ =
      web_contents_->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                            /*stay_awake=*/true,
                                            /*is_activity=*/true);

  CaptureWindowSnapshot();
  return true;
}

void LensOverlayControllerAndroid::CaptureWindowSnapshot() {
  content::RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    OnCopyFromSurfaceFinished(
        base::unexpected(content::CopyFromSurfaceError::kUnknown));
    return;
  }

  rwhv->CopyFromSurface(
      gfx::Rect(), gfx::Size(), kCaptureTimeout,
      base::BindOnce(&LensOverlayControllerAndroid::OnCopyFromSurfaceFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LensOverlayControllerAndroid::OnCopyFromSurfaceFinished(
    const content::CopyFromSurfaceResult& result) {
  // Release the capturer count now that the asynchronous process is complete.
  scoped_capturer_.RunAndReset();

  if (!result.has_value() || result->bitmap.drawsNothing()) {
    LOG(ERROR) << "Failed to capture screenshot from renderer surface.";
    if (java_obj_) {
      Java_LensOverlayCoordinator_onCaptureError(
          base::android::AttachCurrentThread(), java_obj_);
    }
    return;
  }

  if (java_obj_) {
    Java_LensOverlayCoordinator_onScreenshotCaptured(
        base::android::AttachCurrentThread(), java_obj_, result->bitmap);
  }
}

void LensOverlayControllerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace lens

DEFINE_JNI(LensOverlayCoordinator)
