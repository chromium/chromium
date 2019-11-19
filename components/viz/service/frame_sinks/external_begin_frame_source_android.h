// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// An implementation of ExternalBeginFrameSource which is driven by VSync
// signals coming from org.chromium.ui.VSyncMonitor.
class VIZ_SERVICE_EXPORT ExternalBeginFrameSourceAndroid
    : public ExternalBeginFrameSource,
      public ExternalBeginFrameSourceClient {
 public:
  ExternalBeginFrameSourceAndroid(uint32_t restart_id, float refresh_rate);
  ~ExternalBeginFrameSourceAndroid() override;

  void OnVSync(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jlong time_micros,
               jlong period_micros);
  void UpdateRefreshRate(float refresh_rate) override;

 private:
  // ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SetEnabled(bool enabled);

  uint64_t next_sequence_number_ = BeginFrameArgs::kStartingFrameNumber;
  base::android::ScopedJavaGlobalRef<jobject> j_object_;
  // Used for determining what the sequence number should be on
  // CreateBeginFrameArgs.
  base::TimeTicks next_expected_frame_time_;

  DISALLOW_COPY_AND_ASSIGN(ExternalBeginFrameSourceAndroid);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_EXTERNAL_BEGIN_FRAME_SOURCE_ANDROID_H_
