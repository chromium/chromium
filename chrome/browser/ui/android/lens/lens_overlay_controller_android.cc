// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/lens/lens_overlay_controller_android.h"

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ENABLE_LENS_OVERLAY_BACKEND)
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#endif

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
    : content::WebContentsObserver(web_contents), java_obj_(env, obj) {}

LensOverlayControllerAndroid::~LensOverlayControllerAndroid() = default;

bool LensOverlayControllerAndroid::ShowUI(JNIEnv* env,
                                          int32_t invocation_source,
                                          bool use_webui) {
  // TODO(b/493627069): Pass actual mime type when extracted.
  lens::RecordInvocation(
      static_cast<lens::LensOverlayInvocationSource>(invocation_source),
      lens::MimeType::kHtml);

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!web_contents()) {
    return false;
  }

  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (!rwhv || !rwhv->IsSurfaceAvailableForCopy()) {
    return false;
  }

  // Increment the capturer count to keep the WebContents active and visible
  // during the asynchronous capture process.
  scoped_capturer_ =
      web_contents()->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                             /*stay_awake=*/true,
                                             /*is_activity=*/true);

  CaptureWindowSnapshot(use_webui);
  return true;
}

void LensOverlayControllerAndroid::CaptureWindowSnapshot(bool use_webui) {
  if (!web_contents()) {
    OnCopyFromSurfaceFinished(
        use_webui, base::unexpected(content::CopyFromSurfaceError::kUnknown));
    return;
  }

  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (!rwhv) {
    OnCopyFromSurfaceFinished(
        use_webui, base::unexpected(content::CopyFromSurfaceError::kUnknown));
    return;
  }

  rwhv->CopyFromSurface(
      gfx::Rect(), gfx::Size(), kCaptureTimeout,
      base::BindOnce(&LensOverlayControllerAndroid::OnCopyFromSurfaceFinished,
                     weak_ptr_factory_.GetWeakPtr(), use_webui));
}

void LensOverlayControllerAndroid::OnCopyFromSurfaceFinished(
    bool use_webui,
    const content::CopyFromSurfaceResult& result) {
  // Release the capturer count now that the asynchronous process is complete.
  scoped_capturer_.RunAndReset();

  if (!web_contents()) {
    return;
  }

  if (!result.has_value() || result->bitmap.drawsNothing()) {
    LOG(ERROR) << "Failed to capture screenshot from renderer surface.";
    if (java_obj_) {
      Java_LensOverlayCoordinator_onCaptureError(
          base::android::AttachCurrentThread(), java_obj_);
    }
    return;
  }

#if BUILDFLAG(ENABLE_LENS_OVERLAY_BACKEND)
  if (use_webui) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    lens::GrantLensOverlayNeededPermissions(profile);

    // Destroy the old query controller FIRST, so its destructor can safely use
    // the old gen204_controller.
    query_controller_.reset();

    // Now it's safe to recreate the gen204_controller.
    gen204_controller_ = std::make_unique<lens::LensOverlayGen204Controller>();

    variations::VariationsClient* variations_client =
        profile->GetVariationsClient();

    query_controller_ = std::make_unique<lens::LensOverlayQueryController>(
        base::BindRepeating(&LensOverlayControllerAndroid::OnFullImageResponse,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&LensOverlayControllerAndroid::OnUrlResponse,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(
            &LensOverlayControllerAndroid::OnInteractionResponse,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&LensOverlayControllerAndroid::OnThumbnailCreated,
                            weak_ptr_factory_.GetWeakPtr()),
        /*page_content_upload_progress_callback=*/base::NullCallback(),
        variations_client, identity_manager, profile,
        lens::LensOverlayInvocationSource::kAppMenu,
        /*use_dark_mode=*/false, gen204_controller_.get());

    std::vector<lens::mojom::CenterRotatedBoxPtr> empty_regions;
    query_controller_->StartQueryFlow(
        result->bitmap, /*initial_image=*/result->bitmap,
        web_contents()->GetVisibleURL(), /*page_title=*/std::nullopt,
        std::move(empty_regions),
        /*underlying_page_contents=*/base::span<const lens::PageContent>(),
        /*primary_content_type=*/lens::MimeType::kUnknown,
        /*pdf_current_page=*/std::nullopt, /*ui_scale_factor=*/1.0f,
        /*invocation_time=*/base::TimeTicks::Now());
  }
#endif

  if (java_obj_) {
    Java_LensOverlayCoordinator_onScreenshotCaptured(
        base::android::AttachCurrentThread(), java_obj_, result->bitmap);
  }
}

#if BUILDFLAG(ENABLE_LENS_OVERLAY_BACKEND)
void LensOverlayControllerAndroid::OnFullImageResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {
  if (java_obj_ && !is_error) {
    Java_LensOverlayCoordinator_onObjectsReceived(
        base::android::AttachCurrentThread(), java_obj_, objects.size());
  }
}

void LensOverlayControllerAndroid::OnUrlResponse(
    lens::proto::LensOverlayUrlResponse response) {}

void LensOverlayControllerAndroid::OnInteractionResponse(
    lens::mojom::TextPtr text) {}

void LensOverlayControllerAndroid::OnThumbnailCreated(
    const std::string& thumbnail_bytes,
    const SkBitmap& bitmap) {}
#endif

void LensOverlayControllerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace lens

DEFINE_JNI(LensOverlayCoordinator)
