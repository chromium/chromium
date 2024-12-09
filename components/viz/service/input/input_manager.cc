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
#include "components/viz/service/input/render_input_router_iterator_impl.h"
#include "components/viz/service/input/render_input_router_support_child_frame.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_input_receiver_compat.h"
#include "components/input/android/input_token_forwarder.h"
#include "components/input/android/scoped_input_receiver.h"
#include "components/input/android/scoped_input_receiver_callbacks.h"
#include "components/input/android/scoped_input_transfer_token.h"
#include "components/viz/service/input/fling_scheduler_android.h"
#include "components/viz/service/input/render_input_router_support_android.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/android/scoped_a_native_window.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace viz {

namespace {

#if BUILDFLAG(IS_ANDROID)

void ForwardVizInputTransferToken(
    const input::ScopedInputTransferToken& viz_input_token,
    const gpu::SurfaceHandle& surface_handle) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> viz_input_token_java(
      env, base::AndroidInputReceiverCompat::GetInstance()
               .AInputTransferToken_toJavaFn(
                   env, viz_input_token.a_input_transfer_token()));

  input::InputTokenForwarder::GetInstance()->ForwardVizInputTransferToken(
      surface_handle, viz_input_token_java);
}

#endif  // BUILDFLAG(IS_ANDROID)

bool IsFrameMetadataAvailable(CompositorFrameSinkSupport* support) {
  return support && support->GetLastActivatedFrameMetadata();
}

}  // namespace

FrameSinkMetadata::FrameSinkMetadata(
    uint32_t grouping_id,
    std::unique_ptr<RenderInputRouterSupportBase> support,
    std::unique_ptr<RenderInputRouterDelegateImpl> delegate)
    : grouping_id(grouping_id),
      rir_support(std::move(support)),
      rir_delegate(std::move(delegate)) {}

FrameSinkMetadata::~FrameSinkMetadata() = default;

FrameSinkMetadata::FrameSinkMetadata(FrameSinkMetadata&& other) = default;
FrameSinkMetadata& FrameSinkMetadata::operator=(FrameSinkMetadata&& other) =
    default;

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr char kInputSCName[] = "ChromeInputSurfaceControl";
constexpr char kParentInputSCName[] = "ChromeParentInputSurfaceControl";

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
  kSuccessfulButNullTransferToken = 6,
  kReuseExistingInputReceiver = 7,
  kMaxValue = kReuseExistingInputReceiver,
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

std::unique_ptr<input::FlingSchedulerBase> InputManager::MakeFlingScheduler(
    input::RenderInputRouter* rir,
    const FrameSinkId& frame_sink_id) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<FlingSchedulerAndroid>(rir, this, frame_sink_id);
#else
  NOTREACHED();
#endif
}

void InputManager::SetupRenderInputRouter(
    input::RenderInputRouter* render_input_router,
    const FrameSinkId& frame_sink_id) {
  // TODO(382291983): Setup RenderInputRouter's mojo connections to renderer.
  render_input_router->SetFlingScheduler(
      MakeFlingScheduler(render_input_router, frame_sink_id));

  render_input_router->SetupInputRouter(
      GetDeviceScaleFactorForId(frame_sink_id));
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
        FROM_HERE,
        base::BindOnce(&InputManager::CreateOrReuseAndroidInputReceiver,
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
      it->second, *this, frame_sink_id, grouping_id);

  // Sets up RenderInputRouter.
  auto render_input_router = std::make_unique<input::RenderInputRouter>(
      /* host */ nullptr,
      /* fling_scheduler */ nullptr,
      /* delegate */ rir_delegate.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  SetupRenderInputRouter(render_input_router.get(), frame_sink_id);

  frame_sink_metadata_map_.emplace(std::make_pair(
      frame_sink_id,
      FrameSinkMetadata{grouping_id,
                        MakeRenderInputRouterSupport(render_input_router.get(),
                                                     frame_sink_id),
                        std::move(rir_delegate)}));

  rir_map_.emplace(
      std::make_pair(frame_sink_id, std::move(render_input_router)));
}

void InputManager::OnDestroyedCompositorFrameSink(
    const FrameSinkId& frame_sink_id) {
  TRACE_EVENT("viz", "InputManager::OnDestroyedCompositorFrameSink",
              "frame_sink_id", frame_sink_id);
#if BUILDFLAG(IS_ANDROID)
  if (receiver_data_) {
    receiver_data_->OnDestroyedCompositorFrameSink(frame_sink_id);
  }
#endif  // BUILDFLAG(IS_ANDROID)

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

void InputManager::OnFrameSinkDeviceScaleFactorChanged(
    const FrameSinkId& frame_sink_id,
    float device_scale_factor) {
  auto rir_iter = rir_map_.find(frame_sink_id);
  // Return early if |frame_sink_id| is associated with a non layer tree frame
  // sink.
  if (rir_iter == rir_map_.end()) {
    return;
  }

  // Update device scale factor in RenderInputRouter from latest activated
  // compositor frame.
  rir_iter->second->SetDeviceScaleFactor(device_scale_factor);
}

input::TouchEmulator* InputManager::GetTouchEmulator(bool create_if_necessary) {
  return nullptr;
}

const DisplayHitTestQueryMap& InputManager::GetDisplayHitTestQuery() const {
  return frame_sink_manager_->GetDisplayHitTestQuery();
}

float InputManager::GetDeviceScaleFactorForId(
    const FrameSinkId& frame_sink_id) {
  auto* support = frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
  CHECK(support);

  if (!IsFrameMetadataAvailable(support)) {
    // If a CompositorFrame hasn't been submitted yet for a child frame, we fall
    // back to use RootCompositorFrameSink's submitted frame metadata.
    support = frame_sink_manager_->GetFrameSinkForId(
        GetRootCompositorFrameSinkId(frame_sink_id));

    // If there's still no activated frame metadata available, return a default
    // scale factor of 1.0.
    if (!IsFrameMetadataAvailable(support)) {
      return 1.0;
    }
  }

  return support->GetLastActivatedFrameMetadata()->device_scale_factor;
}

FrameSinkId InputManager::GetRootCompositorFrameSinkId(
    const FrameSinkId& child_frame_sink_id) {
  return frame_sink_manager_->GetOldestRootCompositorFrameSinkId(
      child_frame_sink_id);
}

RenderInputRouterSupportBase* InputManager::GetParentRenderInputRouterSupport(
    const FrameSinkId& frame_sink_id) {
  auto parent_id =
      frame_sink_manager_->GetOldestParentByChildFrameId(frame_sink_id);

  CHECK(!frame_sink_manager_->IsFrameSinkIdInRootSinkMap(parent_id));

  auto it = frame_sink_metadata_map_.find(parent_id);
  if (it != frame_sink_metadata_map_.end()) {
    return it->second.rir_support.get();
  }
  DUMP_WILL_BE_NOTREACHED();
  return nullptr;
}

RenderInputRouterSupportBase* InputManager::GetRootRenderInputRouterSupport(
    const FrameSinkId& frame_sink_id) {
  auto parent_frame_sink_id =
      frame_sink_manager_->GetOldestParentByChildFrameId(frame_sink_id);
  FrameSinkId current_id = frame_sink_id;

  while (
      !frame_sink_manager_->IsFrameSinkIdInRootSinkMap(parent_frame_sink_id)) {
    current_id = parent_frame_sink_id;
    parent_frame_sink_id = frame_sink_manager_->GetOldestParentByChildFrameId(
        parent_frame_sink_id);
  }

  auto it = frame_sink_metadata_map_.find(current_id);
  if (it != frame_sink_metadata_map_.end()) {
    return it->second.rir_support.get();
  }

  DUMP_WILL_BE_NOTREACHED();
  return nullptr;
}

const CompositorFrameMetadata* InputManager::GetLastActivatedFrameMetadata(
    const FrameSinkId& frame_sink_id) {
  auto* support = frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
  if (!IsFrameMetadataAvailable(support)) {
    return nullptr;
  }
  return support->GetLastActivatedFrameMetadata();
}

std::unique_ptr<input::RenderInputRouterIterator>
InputManager::GetEmbeddedRenderInputRouters(const FrameSinkId& id) {
  auto rirs = std::make_unique<RenderInputRouterIteratorImpl>(
      *this, frame_sink_manager_->GetChildrenByParent(id));
  return std::move(rirs);
}

void InputManager::NotifyObserversOfInputEvent(
    const FrameSinkId& frame_sink_id,
    uint32_t grouping_id,
    std::unique_ptr<blink::WebCoalescedInputEvent> event) {
  rir_delegate_remote_map_.at(grouping_id)
      ->NotifyObserversOfInputEvent(frame_sink_id, std::move(event));
}

void InputManager::NotifyObserversOfInputEventAcks(
    const FrameSinkId& frame_sink_id,
    uint32_t grouping_id,
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_result,
    std::unique_ptr<blink::WebCoalescedInputEvent> event) {
  rir_delegate_remote_map_.at(grouping_id)
      ->NotifyObserversOfInputEventAcks(frame_sink_id, ack_source, ack_result,
                                        std::move(event));
}

void InputManager::OnInvalidInputEventSource(const FrameSinkId& frame_sink_id,
                                             uint32_t grouping_id) {
  rir_delegate_remote_map_.at(grouping_id)
      ->OnInvalidInputEventSource(frame_sink_id);
}

std::optional<bool> InputManager::IsDelegatedInkHovering(
    const FrameSinkId& frame_sink_id) {
  auto* support = frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
  if (!IsFrameMetadataAvailable(support)) {
    return std::nullopt;
  }
  return support->GetLastActivatedFrameMetadata()
      ->delegated_ink_metadata->is_hovering();
}

void InputManager::SetupRenderInputRouterDelegateConnection(
    uint32_t grouping_id,
    mojo::PendingRemote<input::mojom::RenderInputRouterDelegateClient>
        rir_delegate_remote) {
  rir_delegate_remote_map_[grouping_id].Bind(std::move(rir_delegate_remote));
  rir_delegate_remote_map_[grouping_id].set_disconnect_handler(
      base::BindOnce(&InputManager::OnRIRDelegateClientDisconnected,
                     base::Unretained(this), grouping_id));
}

GpuServiceImpl* InputManager::GetGpuService() {
  return frame_sink_manager_->GetGpuService();
}

input::RenderInputRouter* InputManager::GetRenderInputRouterFromFrameSinkId(
    const FrameSinkId& id) {
  return rir_map_[id].get();
}

std::unique_ptr<RenderInputRouterSupportBase>
InputManager::MakeRenderInputRouterSupport(input::RenderInputRouter* rir,
                                           const FrameSinkId& frame_sink_id) {
  TRACE_EVENT_INSTANT("input", "InputManager::MakeRenderInputRouterSupport");
  auto parent_id =
      frame_sink_manager_->GetOldestParentByChildFrameId(frame_sink_id);
  if (frame_sink_manager_->IsFrameSinkIdInRootSinkMap(parent_id)) {
#if BUILDFLAG(IS_ANDROID)
    return std::make_unique<RenderInputRouterSupportAndroid>(rir, this,
                                                             frame_sink_id);
#else
    // InputVizard only supports Android currently.
    NOTREACHED();
#endif
  }
  return std::make_unique<RenderInputRouterSupportChildFrame>(rir, this,
                                                              frame_sink_id);
}

void InputManager::OnRIRDelegateClientDisconnected(uint32_t grouping_id) {
  rir_delegate_remote_map_.erase(grouping_id);
}

#if BUILDFLAG(IS_ANDROID)
void InputManager::CreateOrReuseAndroidInputReceiver(
    const FrameSinkId& frame_sink_id,
    const gpu::SurfaceHandle& surface_handle) {
  if (receiver_data_ && receiver_data_->root_frame_sink_id().is_valid()) {
    // Only allow input receiver "creation" for single root compositor frame
    // sink.
    return;
  }

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
  scoped_refptr<gfx::SurfaceControl::Surface> parent_input_surface =
      base::MakeRefCounted<gfx::SurfaceControl::Surface>(
          window.a_native_window(), kParentInputSCName);

  if (receiver_data_) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kReuseExistingInputReceiver);

    receiver_data_->AttachToFrameSink(frame_sink_id, parent_input_surface);

    const input::ScopedInputTransferToken& viz_input_token =
        receiver_data_->viz_input_token();
    DCHECK(viz_input_token);

    ForwardVizInputTransferToken(viz_input_token, surface_handle);
    return;
  }

  scoped_refptr<gfx::SurfaceControl::Surface> input_surface =
      base::MakeRefCounted<gfx::SurfaceControl::Surface>(*parent_input_surface,
                                                         kInputSCName);
  if (!parent_input_surface->surface() || !input_surface->surface()) {
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

  std::unique_ptr<input::AndroidInputCallback> android_input_callback =
      std::make_unique<input::AndroidInputCallback>(frame_sink_id, this);
  // Destructor of |ScopedInputReceiverCallbacks| will call
  // |AInputReceiverCallbacks_release|, so we don't have to explicitly unset the
  // motion event callback we set below using
  // |AInputReceiverCallbacks_setMotionEventCallback|.
  input::ScopedInputReceiverCallbacks callbacks(android_input_callback.get());
  if (!callbacks) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullCallbacks);
    return;
  }

  base::AndroidInputReceiverCompat::GetInstance()
      .AInputReceiverCallbacks_setMotionEventCallbackFn(
          callbacks.a_input_receiver_callbacks(),
          input::AndroidInputCallback::OnMotionEventThunk);

  input::ScopedInputReceiver receiver(
      looper, browser_input_token.a_input_transfer_token(),
      input_surface->surface(), callbacks.a_input_receiver_callbacks());

  if (!receiver) {
    UMA_HISTOGRAM_ENUMERATION(kInputReceiverCreationResultHistogram,
                              CreateAndroidInputReceiverResult::kFailedUnknown);
    return;
  }

  input::ScopedInputTransferToken viz_input_token(receiver.a_input_receiver());
  if (!viz_input_token) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kSuccessfulButNullTransferToken);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      kInputReceiverCreationResultHistogram,
      CreateAndroidInputReceiverResult::kSuccessfullyCreated);

  ForwardVizInputTransferToken(viz_input_token, surface_handle);

  receiver_data_ = std::make_unique<input::InputReceiverData>(
      parent_input_surface, input_surface, std::move(browser_input_token),
      std::move(android_input_callback), std::move(callbacks),
      std::move(receiver), std::move(viz_input_token));
}

bool InputManager::OnMotionEvent(base::android::ScopedInputEvent input_event,
                                 const FrameSinkId& root_frame_sink_id) {
  // TODO(370506271): Implement once we do the state transfer from Browser on
  // touch down.

  // Always return true since we are receiving input on Viz after hit testing on
  // Browser already determined that web contents are being hit.
  return true;
}

BeginFrameSource* InputManager::GetBeginFrameSourceForFrameSink(
    const FrameSinkId& id) {
  return frame_sink_manager_->GetFrameSinkForId(id)->begin_frame_source();
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace viz
