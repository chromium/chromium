// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <optional>

#include "base/android/jni_weak_ref.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// An implementation of ExternalBeginFrameSource which is driven by VSync
// signals coming from org.chromium.ui.VSyncMonitor.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceAndroid
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  ExternalBeginFrameSourceAndroid(uint32_t restart_id,
                                  float refresh_rate,
                                  bool requires_align_with_java);

  ExternalBeginFrameSourceAndroid(const ExternalBeginFrameSourceAndroid&) =
      delete;
  ExternalBeginFrameSourceAndroid& operator=(
      const ExternalBeginFrameSourceAndroid&) = delete;

  ~ExternalBeginFrameSourceAndroid() override;

  void OnVSync(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong time_micros,
               jlong period_micros);
  void UpdateRefreshRate(float refresh_rate) override;

 private:
  class AChoreographerImpl;

  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SetEnabled(bool enabled);
  void OnVSyncImpl(int64_t time_nanos,
                   base::TimeDelta vsync_period,
                   std::optional<PossibleDeadlines> possible_deadlines);

  std::unique_ptr<AChoreographerImpl> achoreographer_;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  BeginFrameArgsGenerator begin_frame_args_generator_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
