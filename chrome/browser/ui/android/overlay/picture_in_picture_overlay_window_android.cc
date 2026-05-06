// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/unguessable_token_android.h"
#include "base/no_destructor.h"
#include "chrome/android/chrome_jni_headers/PictureInPictureActivity_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/overlay/overlay_window_android.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

// PictureInPictureOverlayWindowAndroid provides an implementation of the
// VideoOverlayWindow for Android, utilizing a Java-side Activity to display
// the video content in a Picture-in-Picture window.
class PictureInPictureOverlayWindowAndroid : public OverlayWindowAndroid {
 public:
  explicit PictureInPictureOverlayWindowAndroid(
      content::VideoPictureInPictureWindowController* controller)
      : OverlayWindowAndroid(controller) {
    CreateJavaActivity();
  }

  ~PictureInPictureOverlayWindowAndroid() override { OnWindowDestroyedJava(); }

 private:
  void CreateJavaActivity() override {
    auto* web_contents = controller_->GetWebContents();

    // Compute the screen position of the video, and see if it fits inside the
    // WebContents or if it's clipped / off-screen.  If it's onscreen, then T
    // and later, Android can do a nicer animated transition to PiP with a
    // screen capture of the video.  However, if the video is clipped /
    // offscreen, then it'll look nicer to use the default light grey
    // transition.
    // We provide a small buffer for what "clipped" means, rather than enforcing
    // it strictly.  It'll still look fine while allowing small positioning
    // errors that sites sometimes make.  See https://crbug.com/40254849 for an
    // example.
    // The java side will ignore any source bounds that are not on the screen
    // for the source rect hint. It will use the aspect ratio only in that case.
    // We set the x position to be <0 to ensure this, to skip the transition.
    // Get the size of the video, and inset it to provide some slack.
    gfx::Rect source_bounds = controller_->GetSourceBounds();
    gfx::Rect smaller_source_bounds = source_bounds;
    constexpr int inset_size = 4;  // pixels on each side
    smaller_source_bounds.Inset(inset_size);

    // Get the size of the WebContents, and convert to pixels.
    gfx::Rect unscaled_content_bounds = web_contents->GetContainerBounds();
    auto* native_view = web_contents->GetNativeView();
    const float dip_scale = native_view->GetDipScale();
    gfx::Rect content_bounds(unscaled_content_bounds.x() * dip_scale,
                             unscaled_content_bounds.y() * dip_scale,
                             unscaled_content_bounds.width() * dip_scale,
                             unscaled_content_bounds.height() * dip_scale);
    const bool out_of_bounds = !content_bounds.Contains(smaller_source_bounds);
    // Use the new source location based transition when the source is not out
    // of bound and the AllowEnhancedPipTransition feature is enabled.
    // TODO(crbug.com/440384447): remove AllowEnhancedPipTransition check once
    // the new transition works properly on desktop Android.
    const bool use_source_hint_transition =
        !out_of_bounds &&
        base::FeatureList::IsEnabled(media::kAllowEnhancedPipTransition);

    if (use_source_hint_transition) {
      // Use the newer transition, if available.
      // Convert to screen space.  Since the comparison was with the inset
      // source bounds, clamp the real source bounds to the container.
      source_bounds.Intersect(content_bounds);
      gfx::PointF offset = native_view->GetLocationOnScreen(0, 0);
      source_bounds.Offset(
          static_cast<int>(offset.x()),
          static_cast<int>(offset.y()) +
              native_view->content_offset() * native_view->GetDipScale());
    } else {
      // Use the old transition.
      // Slide this offscreen, while keeping the aspect ratio the same.
      source_bounds.set_x(-1);
    }

    JNIEnv* env = base::android::AttachCurrentThread();
    auto j_token = base::android::UnguessableTokenAndroid::Create(env, token_);
    Java_PictureInPictureActivity_createActivity(
        env, j_token,
        TabAndroid::FromWebContents(web_contents)->GetJavaObject(),
        source_bounds.x(), source_bounds.y(), source_bounds.width(),
        source_bounds.height());
  }
};

std::unique_ptr<content::VideoOverlayWindow>
CreatePictureInPictureOverlayWindowAndroid(
    content::VideoPictureInPictureWindowController* controller) {
  return std::make_unique<PictureInPictureOverlayWindowAndroid>(controller);
}

DEFINE_JNI(PictureInPictureActivity)
