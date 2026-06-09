// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/unguessable_token_android.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "cc/slim/surface_layer.h"
#include "chrome/android/chrome_jni_headers/XrModuleBridge_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/overlay/overlay_window_android.h"
#include "content/public/browser/immersive_playback_options.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

// ImmersiveOverlayWindowAndroid provides an implementation of the
// VideoOverlayWindow for Android XR, utilizing a Java-side Activity to display
// the video content in an immersive environment.
class ImmersiveOverlayWindowAndroid : public OverlayWindowAndroid {
 public:
  explicit ImmersiveOverlayWindowAndroid(
      content::VideoPictureInPictureWindowController* controller)
      : OverlayWindowAndroid(controller) {
    CreateJavaActivity();
  }

  ~ImmersiveOverlayWindowAndroid() override { OnWindowDestroyedJava(); }

  // VideoOverlayWindow implementation.
  void SetImmersiveVideoOptions(
      const content::ImmersiveOptions& options) override {
    immersive_options_ = options;
    SetImmersiveVideoOptionsJava(immersive_options_.value());
  }

  void Initialize(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& self,
      const base::android::JavaRef<jobject>& jwindow_android) override {
    OverlayWindowAndroid::Initialize(env, self, jwindow_android);

    if (immersive_options_) {
      SetImmersiveVideoOptionsJava(immersive_options_.value());
    }
  }

  void UpdateNaturalSize(const gfx::Size& natural_size) override {
    video_size_ = natural_size;
    surface_layer_->SetBounds(natural_size);
    UpdateVideoSizeJava(natural_size.width(), natural_size.height());
  }

  void OnActivityStopped() override {
    // No-op. Do not auto-close the overlay when the activity is stopped.
  }

  gfx::Rect GetBounds() override { return gfx::Rect(video_size_); }

 private:
  void CreateJavaActivity() override {
    auto* web_contents = controller_->GetWebContents();
    JNIEnv* env = base::android::AttachCurrentThread();
    auto j_token = base::android::UnguessableTokenAndroid::Create(env, token_);
    Java_XrModuleBridge_createImmersiveVideoPlaybackActivity(
        env, j_token,
        TabAndroid::FromWebContents(web_contents)->GetJavaObject());
  }

  std::optional<content::ImmersiveOptions> immersive_options_;
};

std::unique_ptr<content::VideoOverlayWindow>
CreateImmersiveOverlayWindowAndroid(
    content::VideoPictureInPictureWindowController* controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Immersive sessions are only initialized after the XR module is installed.
  CHECK(Java_XrModuleBridge_isModuleInstalled(env));

  return std::make_unique<ImmersiveOverlayWindowAndroid>(controller);
}
