// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/input_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/looper.h>
#endif  // BUILDFLAG(IS_ANDROID)

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/service/input/render_input_router_delegate_impl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/input/android/scoped_input_receiver.h"
#include "components/input/android/scoped_input_receiver_callbacks.h"
#include "components/input/android/scoped_input_transfer_token.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/android/scoped_a_native_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace viz {

FrameSinkMetadata::FrameSinkMetadata(
    uint32_t grouping_id,
    std::unique_ptr<RenderInputRouterDelegateImpl> delegate)
    : grouping_id(grouping_id), rir_delegate(std::move(delegate)) {}
FrameSinkMetadata::~FrameSinkMetadata() = default;

FrameSinkMetadata::FrameSinkMetadata(FrameSinkMetadata&& other) = default;
FrameSinkMetadata& FrameSinkMetadata::operator=(FrameSinkMetadata&& other) =
    default;

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr char kInputSurfaceControlName[] = "ChromeInputSurfaceControl";

constexpr char kInputReceiverCreationResultHistogram[] =
    "Android.InputOnViz.InputReceiverCreationResult";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreateAndroidInputReceiverResult {
  kSuccessfullyCreated = 0,
  kFailedUnknown = 1,
  kFailedNullSurfaceControl = 2,
  kFailedNullLooper = 3,
  kFailedNullInputTransferToken = 4,
  kFailedNullCallbacks = 5,
  kMaxValue = kFailedNullCallbacks,
};
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

InputManager::~InputManager() {
  frame_sink_manager_->RemoveObserver(this);
}

InputManager::InputManager(FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager) {
  TRACE_EVENT("viz", "InputManager::InputManager");
  DCHECK(frame_sink_manager_);
  frame_sink_manager_->AddObserver(this);
}

void InputManager::OnCreateCompositorFrameSink(
    const FrameSinkId& frame_sink_id,
    bool is_root,
    input::mojom::RenderInputRouterConfigPtr render_input_router_config,
    bool create_input_receiver,
    gpu::SurfaceHandle surface_handle) {
  TRACE_EVENT("viz", "InputManager::OnCreateCompositorFrameSink",
              "config_is_null", !render_input_router_config, "frame_sink_id",
              frame_sink_id);
#if BUILDFLAG(IS_ANDROID)
  if (create_input_receiver) {
    CHECK(is_root);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&InputManager::CreateAndroidInputReceiver,
                                  weak_ptr_factory_.GetWeakPtr(), frame_sink_id,
                                  surface_handle));
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // `render_input_router_config` is non null only when layer tree frame sinks
  // for renderer are being requested.
  if (!render_input_router_config) {
    return;
  }

  DCHECK(render_input_router_config->rir_client.is_valid());
  DCHECK(input::IsTransferInputToVizSupported() && !is_root);

  uint32_t grouping_id = render_input_router_config->grouping_id;

  auto [it, inserted] = rwhier_map_.try_emplace(
      grouping_id,
      base::MakeRefCounted<input::RenderWidgetHostInputEventRouter>(
          frame_sink_manager_, this));

  if (inserted) {
    TRACE_EVENT_INSTANT("viz", "RenderWidgetHostInputEventRouterCreated",
                        "grouping_id", grouping_id);
  }

  // |rir_delegate| should outlive |render_input_router|.
  auto rir_delegate = std::make_unique<RenderInputRouterDelegateImpl>(
      it->second, frame_sink_id);

  auto render_input_router = std::make_unique<input::RenderInputRouter>(
      /* host */ nullptr,
      /* fling_scheduler */ nullptr,
      /* delegate */ rir_delegate.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  frame_sink_metadata_map_.emplace(std::make_pair(
      frame_sink_id, FrameSinkMetadata{grouping_id, std::move(rir_delegate)}));

  rir_map_.emplace(
      std::make_pair(frame_sink_id, std::move(render_input_router)));
}

void InputManager::OnDestroyedCompositorFrameSink(
    const FrameSinkId& frame_sink_id) {
  TRACE_EVENT("viz", "InputManager::OnDestroyedCompositorFrameSink",
              "frame_sink_id", frame_sink_id);
  auto rir_iter = rir_map_.find(frame_sink_id);
  // Return early if |frame_sink_id| is associated with a non layer tree frame
  // sink.
  if (rir_iter == rir_map_.end()) {
    return;
  }

  rir_map_.erase(rir_iter);

  uint32_t grouping_id =
      frame_sink_metadata_map_.find(frame_sink_id)->second.grouping_id;
  // Deleting FrameSinkMetadata for |frame_sink_id| decreases the refcount for
  // RenderWidgetHostInputEventRouter in |rwhier_map_|(associated with the
  // RenderInputRouterDelegateImpl), for this |frame_sink_id|.
  frame_sink_metadata_map_.erase(frame_sink_id);

  auto it = rwhier_map_.find(grouping_id);
  if (it != rwhier_map_.end()) {
    if (it->second->HasOneRef()) {
      // There are no CompositorFrameSinks associated with this
      // RenderWidgetHostInputEventRouter, delete it.
      rwhier_map_.erase(it);
    }
  }
}

input::TouchEmulator* InputManager::GetTouchEmulator(bool create_if_necessary) {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
void InputManager::CreateAndroidInputReceiver(
    const FrameSinkId& frame_sink_id,
    const gpu::SurfaceHandle& surface_handle) {
  // This results in a sync binder to Browser, the same call is made on
  // CompositorGpu thread as well but to keep the code simple and not having to
  // plumb through Android SurfaceControl and InputTransferToken, this duplicate
  // call is made from here.
  auto surface_record =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(surface_handle);

  CHECK(absl::holds_alternative<gl::ScopedJavaSurface>(
      surface_record.surface_variant));
  gl::ScopedJavaSurface& scoped_java_surface =
      absl::get<gl::ScopedJavaSurface>(surface_record.surface_variant);

  gl::ScopedANativeWindow window(scoped_java_surface);
  scoped_refptr<gfx::SurfaceControl::Surface> surface =
      base::MakeRefCounted<gfx::SurfaceControl::Surface>(
          window.a_native_window(), kInputSurfaceControlName);
  if (!surface->surface()) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullSurfaceControl);
    return;
  }

  ALooper* looper = ALooper_prepare(0);
  if (!looper) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullLooper);
    return;
  }

  CHECK(surface_record.host_input_token);
  input::ScopedInputTransferToken browser_input_token(
      surface_record.host_input_token.obj());
  if (!browser_input_token) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullInputTransferToken);
    return;
  }

  // Creation of InputReceiverCallbacks with a null context is supported.
  // TODO(b/364201006): Implement InputReceiverCallbacks for passing input
  // events to InputManager and pass a non-null context while creation.
  input::ScopedInputReceiverCallbacks callbacks(/*context=*/nullptr);
  if (!callbacks) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullCallbacks);
    return;
  }

  input::ScopedInputReceiver receiver(
      looper, browser_input_token.a_input_transfer_token(), surface->surface(),
      callbacks.a_input_receiver_callbacks());

  if (receiver) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kSuccessfullyCreated);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kInputReceiverCreationResultHistogram,
                              CreateAndroidInputReceiverResult::kFailedUnknown);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace viz
