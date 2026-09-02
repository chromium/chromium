// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_

#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/lens/buildflags.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(ENABLE_LENS_OVERLAY_BACKEND)
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller_types.h"
#endif

namespace lens {

// TODO(crbug.com/538670163): Once the product direction settles between the
// AGSA intent flow and the WebUI overlay flow, refactor this class into an
// interface with concrete implementations (e.g., Intent vs. Embedded)
// created via a factory to improve encapsulation.
class LensOverlayControllerAndroid : public content::WebContentsObserver {
 public:
  LensOverlayControllerAndroid(JNIEnv* env,
                               const base::android::JavaRef<jobject>& obj,
                               content::WebContents* web_contents);
  LensOverlayControllerAndroid(const LensOverlayControllerAndroid&) = delete;
  LensOverlayControllerAndroid& operator=(const LensOverlayControllerAndroid&) =
      delete;
  ~LensOverlayControllerAndroid() override;

  // Captures a screenshot of the current renderer surface and initiates the
  // overlay UI. Returns true if the capture process was successfully started,
  // or false otherwise (e.g., if the renderer is unavailable).
  bool ShowUI(JNIEnv* env, int32_t invocation_source, bool use_webui);
  void Destroy(JNIEnv* env);

 private:
  void CaptureWindowSnapshot(bool use_webui);
  void OnCopyFromSurfaceFinished(bool use_webui,
                                 const content::CopyFromSurfaceResult& result);

#if BUILDFLAG(ENABLE_LENS_OVERLAY_BACKEND)
  // Callbacks for LensOverlayQueryController
  void OnFullImageResponse(std::vector<lens::mojom::OverlayObjectPtr> objects,
                           lens::mojom::TextPtr text,
                           bool is_error);
  void OnUrlResponse(lens::proto::LensOverlayUrlResponse response);
  void OnInteractionResponse(lens::mojom::TextPtr text);
  void OnThumbnailCreated(const std::string& thumbnail_bytes,
                          const SkBitmap& bitmap);
#endif

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
#if BUILDFLAG(ENABLE_LENS_OVERLAY_BACKEND)
  std::unique_ptr<lens::LensOverlayGen204Controller> gen204_controller_;
  std::unique_ptr<lens::LensOverlayQueryController> query_controller_;
#endif

  base::ScopedClosureRunner scoped_capturer_;
  base::WeakPtrFactory<LensOverlayControllerAndroid> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_ANDROID_LENS_LENS_OVERLAY_CONTROLLER_ANDROID_H_
