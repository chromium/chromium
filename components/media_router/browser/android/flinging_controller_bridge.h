// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ANDROID_FLINGING_CONTROLLER_BRIDGE_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ANDROID_FLINGING_CONTROLLER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/base/flinging_controller.h"
#include "media/base/media_controller.h"

namespace media_router {

// Allows native code to call into a Java FlingingController.
class FlingingControllerBridge : public media::FlingingController,
                                 public media::MediaController {
 public:
  explicit FlingingControllerBridge(
      base::android::ScopedJavaGlobalRef<jobject> controller);

  FlingingControllerBridge(const FlingingControllerBridge&) = delete;
  FlingingControllerBridge& operator=(const FlingingControllerBridge&) = delete;

  ~FlingingControllerBridge() override;

  // FlingingController implementation.
  media::MediaController* GetMediaController() override;
  void AddMediaStatusObserver(media::MediaStatusObserver* observer) override;
  void RemoveMediaStatusObserver(media::MediaStatusObserver* observer) override;
  base::TimeDelta GetApproximateCurrentTime() override;

  // MediaController implementation.
  void Play() override;
  void Pause() override;
  void SetMute(bool mute) override;
  void SetVolume(float volume) override;
  void Seek(base::TimeDelta time) override;

  // Called by the Java side on status updates.
  void OnMediaStatusUpdated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_bridge,
      const base::android::JavaParamRef<jobject>& j_status);

 private:
  // Java MediaControllerBridge instance.
  base::android::ScopedJavaGlobalRef<jobject> j_flinging_controller_bridge_;

  // Observer to be notified of media status changes from the Java side.
  // NOTE: We don't manage a collection of observers because FlingingRenderer is
  // the only observer that subscribes to |this|, with a 1:1 relationship.
  raw_ptr<media::MediaStatusObserver> observer_ = nullptr;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ANDROID_FLINGING_CONTROLLER_BRIDGE_H_
