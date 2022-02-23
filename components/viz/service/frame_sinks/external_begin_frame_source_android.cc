// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include <dlfcn.h>
#include <sys/types.h>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/service/service_jni_headers/ExternalBeginFrameSourceAndroid_jni.h"

extern "C" {
typedef struct AChoreographer AChoreographer;
typedef void (*AChoreographer_frameCallback64)(int64_t, void*);
typedef void (*AChoreographer_refreshRateCallback)(int64_t, void*);

using pAChoreographer_getInstance = AChoreographer* (*)();
using pAChoreographer_postFrameCallback64 =
    void (*)(AChoreographer*, AChoreographer_frameCallback64, void*);
using pAChoreographer_registerRefreshRateCallback =
    void (*)(AChoreographer*, AChoreographer_refreshRateCallback, void*);
using pAChoreographer_unregisterRefreshRateCallback =
    void (*)(AChoreographer*, AChoreographer_refreshRateCallback, void*);
}

namespace {

#define LOAD_FUNCTION(lib, func)                             \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##Fn) {                                         \
      supported = false;                                     \
      LOG(ERROR) << "Unable to load function " << #func;     \
    }                                                        \
  } while (0)

struct AChoreographerMethods {
  static const AChoreographerMethods& Get() {
    static AChoreographerMethods instance;
    return instance;
  }

  bool supported = true;
  pAChoreographer_getInstance AChoreographer_getInstanceFn;
  pAChoreographer_postFrameCallback64 AChoreographer_postFrameCallback64Fn;
  pAChoreographer_registerRefreshRateCallback
      AChoreographer_registerRefreshRateCallbackFn;
  pAChoreographer_unregisterRefreshRateCallback
      AChoreographer_unregisterRefreshRateCallbackFn;

 private:
  AChoreographerMethods() {
    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    if (!main_dl_handle) {
      LOG(ERROR) << "Couldnt load libandroid.so";
      supported = false;
      return;
    }

    LOAD_FUNCTION(main_dl_handle, AChoreographer_getInstance);
    LOAD_FUNCTION(main_dl_handle, AChoreographer_postFrameCallback64);
    LOAD_FUNCTION(main_dl_handle, AChoreographer_registerRefreshRateCallback);
    LOAD_FUNCTION(main_dl_handle, AChoreographer_unregisterRefreshRateCallback);
  }
  ~AChoreographerMethods() = default;
};

}  // namespace

namespace viz {

class ExternalBeginFrameSourceAndroid::AChoreographerImpl {
 public:
  static std::unique_ptr<AChoreographerImpl> Create(
      ExternalBeginFrameSourceAndroid* client);

  AChoreographerImpl(ExternalBeginFrameSourceAndroid* client,
                     AChoreographer* choreographer);
  ~AChoreographerImpl();

  void SetEnabled(bool enabled);

 private:
  static void FrameCallback64(int64_t frame_time_nanos, void* data);
  static void RefershRateCallback(int64_t vsync_period_nanos, void* data);

  void OnVSync(int64_t frame_time_nanos,
               base::WeakPtr<AChoreographerImpl>* self);
  void SetVsyncPeriod(int64_t vsync_period_nanos);
  void RequestVsyncIfNeeded();

  ExternalBeginFrameSourceAndroid* const client_;
  AChoreographer* const achoreographer_;

  base::TimeDelta vsync_period_;
  bool vsync_notification_enabled_ = false;
  // This is a heap-allocated WeakPtr to this object. The WeakPtr is either
  // * passed to `postFrameCallback` if there is one (and exactly one) callback
  //   pending. This is in case this is deleted before a pending callback
  //   fires, in which case the callback is responsible for deleting the
  //   WeakPtr.
  // * or owned by this member variable when there is no pending callback.
  // Thus whether this is nullptr also indicates whether there is a pending
  // frame callback.
  std::unique_ptr<base::WeakPtr<AChoreographerImpl>> self_for_frame_callback_;
  base::WeakPtrFactory<AChoreographerImpl> weak_ptr_factory_{this};
};

// static
std::unique_ptr<ExternalBeginFrameSourceAndroid::AChoreographerImpl>
ExternalBeginFrameSourceAndroid::AChoreographerImpl::Create(
    ExternalBeginFrameSourceAndroid* client) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_R) {
    return nullptr;
  }
  if (!AChoreographerMethods::Get().supported)
    return nullptr;

  AChoreographer* choreographer =
      AChoreographerMethods::Get().AChoreographer_getInstanceFn();
  if (!choreographer)
    return nullptr;

  return std::make_unique<AChoreographerImpl>(client, choreographer);
}

ExternalBeginFrameSourceAndroid::AChoreographerImpl::AChoreographerImpl(
    ExternalBeginFrameSourceAndroid* client,
    AChoreographer* choreographer)
    : client_(client),
      achoreographer_(choreographer),
      vsync_period_(base::Microseconds(16666)) {
  AChoreographerMethods::Get().AChoreographer_registerRefreshRateCallbackFn(
      achoreographer_, &RefershRateCallback, this);
  self_for_frame_callback_ =
      std::make_unique<base::WeakPtr<AChoreographerImpl>>(
          weak_ptr_factory_.GetWeakPtr());
}

ExternalBeginFrameSourceAndroid::AChoreographerImpl::~AChoreographerImpl() {
  AChoreographerMethods::Get().AChoreographer_unregisterRefreshRateCallbackFn(
      achoreographer_, &RefershRateCallback, this);
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::SetEnabled(
    bool enabled) {
  if (vsync_notification_enabled_ == enabled)
    return;
  vsync_notification_enabled_ = enabled;
  RequestVsyncIfNeeded();
}

// static
void ExternalBeginFrameSourceAndroid::AChoreographerImpl::FrameCallback64(
    int64_t frame_time_nanos,
    void* data) {
  TRACE_EVENT0("toplevel,viz", "VSync");
  auto* self = static_cast<base::WeakPtr<AChoreographerImpl>*>(data);
  if (!(*self)) {
    delete self;
    return;
  }
  (*self)->OnVSync(frame_time_nanos, self);
}

// static
void ExternalBeginFrameSourceAndroid::AChoreographerImpl::RefershRateCallback(
    int64_t vsync_period_nanos,
    void* data) {
  static_cast<AChoreographerImpl*>(data)->SetVsyncPeriod(vsync_period_nanos);
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::OnVSync(
    int64_t frame_time_nanos,
    base::WeakPtr<AChoreographerImpl>* self) {
  DCHECK(!self_for_frame_callback_);
  DCHECK(self);
  self_for_frame_callback_.reset(self);
  if (vsync_notification_enabled_) {
    client_->OnVSyncImpl(frame_time_nanos, vsync_period_);
    RequestVsyncIfNeeded();
  }
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::SetVsyncPeriod(
    int64_t vsync_period_nanos) {
  vsync_period_ = base::Nanoseconds(vsync_period_nanos);
}

void ExternalBeginFrameSourceAndroid::AChoreographerImpl::
    RequestVsyncIfNeeded() {
  if (!vsync_notification_enabled_ || !self_for_frame_callback_)
    return;
  AChoreographerMethods::Get().AChoreographer_postFrameCallback64Fn(
      achoreographer_, &FrameCallback64, self_for_frame_callback_.release());
}

// ============================================================================

ExternalBeginFrameSourceAndroid::ExternalBeginFrameSourceAndroid(
    uint32_t restart_id,
    float refresh_rate,
    bool requires_align_with_java)
    : ExternalBeginFrameSource(this, restart_id) {
  // Android WebView requires begin frame to be inside the "animate" stage of
  // input-animate-draw stages of the java Choreographer, which requires using
  // java Choreographer.
  if (requires_align_with_java) {
    achoreographer_ = nullptr;
  } else {
    achoreographer_ = AChoreographerImpl::Create(this);
  }
  if (!achoreographer_) {
    j_object_ = Java_ExternalBeginFrameSourceAndroid_Constructor(
        base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this),
        refresh_rate);
  }
}

ExternalBeginFrameSourceAndroid::~ExternalBeginFrameSourceAndroid() {
  SetEnabled(false);
}

void ExternalBeginFrameSourceAndroid::OnVSync(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong time_micros,
    jlong period_micros) {
  OnVSyncImpl(time_micros * 1000, base::Microseconds(period_micros));
}

void ExternalBeginFrameSourceAndroid::OnVSyncImpl(
    int64_t time_nanos,
    base::TimeDelta vsync_period) {
  // Warning: It is generally unsafe to manufacture TimeTicks values. The
  // following assumption is being made, AND COULD EASILY BREAK AT ANY TIME:
  // Upstream, Java code is providing "System.nanos() / 1000," and this is the
  // same timestamp that would be provided by the CLOCK_MONOTONIC POSIX clock.
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  base::TimeTicks frame_time =
      base::TimeTicks() + base::Nanoseconds(time_nanos);
  // Calculate the next frame deadline:
  base::TimeTicks deadline = frame_time + vsync_period;

  auto begin_frame_args = begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, deadline, vsync_period);
  OnBeginFrame(begin_frame_args);
}

void ExternalBeginFrameSourceAndroid::UpdateRefreshRate(float refresh_rate) {
  if (j_object_) {
    Java_ExternalBeginFrameSourceAndroid_updateRefreshRate(
        base::android::AttachCurrentThread(), j_object_, refresh_rate);
  }
}

void ExternalBeginFrameSourceAndroid::SetDynamicBeginFrameDeadlineOffsetSource(
    DynamicBeginFrameDeadlineOffsetSource*
        dynamic_begin_frame_deadline_offset_source) {
  begin_frame_args_generator_.set_dynamic_begin_frame_deadline_offset_source(
      dynamic_begin_frame_deadline_offset_source);
}

void ExternalBeginFrameSourceAndroid::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  SetEnabled(needs_begin_frames);
}

void ExternalBeginFrameSourceAndroid::SetEnabled(bool enabled) {
  if (achoreographer_) {
    achoreographer_->SetEnabled(enabled);
  } else {
    DCHECK(j_object_);
    Java_ExternalBeginFrameSourceAndroid_setEnabled(
        base::android::AttachCurrentThread(), j_object_, enabled);
  }
}

}  // namespace viz
