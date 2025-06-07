// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/input_manager.h"

#include <variant>

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
#include "components/input/android/android_input_callback.h"
#include "components/input/android/input_token_forwarder.h"
#include "components/input/android/scoped_input_receiver.h"
#include "components/input/android/scoped_input_receiver_callbacks.h"
#include "components/input/android/scoped_input_transfer_token.h"
#include "components/input/features.h"
#include "components/viz/service/input/fling_scheduler_android.h"
#include "components/viz/service/input/render_input_router_support_android.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ui/gfx/android/achoreographer_compat.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gl/android/scoped_a_native_window.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/viz/service/service_jni_headers/InputTransferHandlerViz_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace viz {

namespace {

#if BUILDFLAG(IS_ANDROID)

void ForwardVizInputTransferToken(
    const input::ScopedInputTransferToken& viz_input_token,
    const gpu::SurfaceHandle& surface_handle) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
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
    base::UnguessableToken grouping_id,
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
constexpr char kStateProcessingResultHistogram[] =
    "Android.InputOnViz.Viz.StateProcessingResult";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CreateAndroidInputReceiverResult)
enum class CreateAndroidInputReceiverResult {
  kSuccessfullyCreated = 0,
  kFailedUnknown = 1,
  // kFailedNullSurfaceControl = 2,
  kFailedNullLooper = 3,
  kFailedNullInputTransferToken = 4,
  kFailedNullCallbacks = 5,
  kSuccessfulButNullTransferToken = 6,
  kReuseExistingInputReceiver = 7,
  kNullBrowserInputToken = 8,
  kNotCreatingMoreThanOneReceiver = 9,
  kRootCompositorFrameSinkDestroyed = 10,
  kFailedChoreographerNotSupported = 11,
  kFailedNullChoreographer = 12,
  kFailedNullParentSurfaceControl = 13,
  kFailedNullChildSurfaceControl = 14,
  kMaxValue = kFailedNullChildSurfaceControl,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:CreateAndroidInputReceiverResult)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InputOnVizStateProcessingResult {
  kProcessedSuccessfully = 0,
  kCouldNotFindViewForFrameSinkId = 1,
  kFrameSinkIdCorrespondsToChildView = 2,
  kFrameSinkIdNotAttachedToRootCFS = 3,
  kMaxValue = kFrameSinkIdNotAttachedToRootCFS,
};
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

InputManager::~InputManager() {
  frame_sink_manager_->RemoveObserver(this);
}

InputManager::InputManager(FrameSinkManagerImpl* frame_sink_manager)
    :
#if BUILDFLAG(IS_ANDROID)
      android_state_transfer_handler_(*this),
#endif
      frame_sink_manager_(frame_sink_manager) {
  TRACE_EVENT("viz", "InputManager::InputManager");
  DCHECK(frame_sink_manager_);
  frame_sink_manager_->AddObserver(this);
}

std::unique_ptr<input::FlingSchedulerBase> InputManager::MakeFlingScheduler(
    input::RenderInputRouter* rir,
    const FrameSinkId& frame_sink_id) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<FlingSchedulerAndroid>(rir, frame_sink_id);
#else
  NOTREACHED();
#endif
}

void InputManager::SetupRenderInputRouter(
    input::RenderInputRouter* render_input_router,
    const FrameSinkId& frame_sink_id,
    mojo::PendingRemote<blink::mojom::RenderInputRouterClient> rir_client,
    bool force_enable_zoom) {
  // TODO(382291983): Setup RenderInputRouter's mojo connections to renderer.
  render_input_router->SetFlingScheduler(
      MakeFlingScheduler(render_input_router, frame_sink_id));

  render_input_router->SetupInputRouter(
      GetDeviceScaleFactorForId(frame_sink_id));
  render_input_router->SetForceEnableZoom(force_enable_zoom);
  render_input_router->BindRenderInputRouterInterfaces(std::move(rir_client));
  render_input_router->RendererWidgetCreated(/*for_frame_widget=*/true,
                                             /*is_in_viz=*/true);
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
  if (is_root) {
    MaybeRecreateRootRenderInputRouterSupports(frame_sink_id);
  }
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
  DCHECK(input::InputUtils::IsTransferInputToVizSupported() && !is_root);

  const base::UnguessableToken grouping_id =
      render_input_router_config->grouping_id;

  auto [it, inserted] = rwhier_map_.try_emplace(
      grouping_id,
      base::MakeRefCounted<input::RenderWidgetHostInputEventRouter>(
          frame_sink_manager_, this));

  if (inserted) {
    TRACE_EVENT_INSTANT("viz", "RenderWidgetHostInputEventRouterCreated",
                        "grouping_id", grouping_id.ToString());
  }

  // |rir_delegate| should outlive |render_input_router|.
  auto rir_delegate = std::make_unique<RenderInputRouterDelegateImpl>(
      it->second, *this, frame_sink_id);

  // Sets up RenderInputRouter.
  auto render_input_router = std::make_unique<input::RenderInputRouter>(
      /* host */ nullptr,
      /* fling_scheduler */ nullptr,
      /* delegate */ rir_delegate.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  SetupRenderInputRouter(render_input_router.get(), frame_sink_id,
                         std::move(render_input_router_config->rir_client),
                         render_input_router_config->force_enable_zoom);

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

  auto frame_sink_metadata_map_iter =
      frame_sink_metadata_map_.find(frame_sink_id);

  // Return early if |frame_sink_id| is associated with a non layer tree frame
  // sink.
  if (frame_sink_metadata_map_iter == frame_sink_metadata_map_.end()) {
    return;
  }

  // RenderInputRouterSupportBase must be destroyed first since it holds a
  // reference to RenderInputRouter, otherwise, it could lead to dangling
  // references.
  frame_sink_metadata_map_iter->second.rir_support.reset();

  auto rir_iter = rir_map_.find(frame_sink_id);
  CHECK(rir_iter != rir_map_.end());
  rir_map_.erase(rir_iter);

  base::UnguessableToken grouping_id =
      frame_sink_metadata_map_iter->second.grouping_id;
  // Deleting FrameSinkMetadata for |frame_sink_id| decreases the refcount for
  // RenderWidgetHostInputEventRouter in |rwhier_map_|(associated with the
  // RenderInputRouterDelegateImpl), for this |frame_sink_id|.
  frame_sink_metadata_map_.erase(frame_sink_metadata_map_iter);

  auto it = rwhier_map_.find(grouping_id);
  if (it != rwhier_map_.end()) {
    if (it->second->HasOneRef()) {
      // There are no CompositorFrameSinks associated with this
      // RenderWidgetHostInputEventRouter, delete it.
      rwhier_map_.erase(it);
    }
  }
}

void InputManager::OnRegisteredFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  // Either the `child_frame_sink_id` corresponds to a layer tree frame sink, or
  // the OnCreateCompositorFrameSink call hasn't came in yet. We don't care
  // about the former case in InputManager, for the later correct construction
  // will take place when `OnCreateCompositorFrameSink` call will come.
  auto it = frame_sink_metadata_map_.find(child_frame_sink_id);
  if (it == frame_sink_metadata_map_.end()) {
    return;
  }

  const int num_parents =
      frame_sink_manager_->GetNumParents(child_frame_sink_id);
  if (num_parents > 1) {
    // Let UnregisterFrameSinkHierarchy do the reconstruction for this
    // RenderInputRouterSupport.
    return;
  }
  // `child_frame_sink_id` just got registered to `parent_frame_sink_id`,
  // `num_parents` should not be zero.
  CHECK_EQ(num_parents, 1);

  RecreateRenderInputRouterSupport(child_frame_sink_id,
                                   /* frame_sink_metadata= */ it->second);
}

void InputManager::OnUnregisteredFrameSinkHierarchy(
    const FrameSinkId& parent_frame_sink_id,
    const FrameSinkId& child_frame_sink_id) {
  auto it = frame_sink_metadata_map_.find(child_frame_sink_id);
  if (it == frame_sink_metadata_map_.end()) {
    return;
  }

  if (frame_sink_manager_->GetNumParents(child_frame_sink_id) != 1) {
    return;
  }

  RecreateRenderInputRouterSupport(child_frame_sink_id,
                                   /* frame_sink_metadata= */ it->second);
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

void InputManager::OnFrameSinkMobileOptimizedChanged(
    const FrameSinkId& frame_sink_id,
    bool is_mobile_optimized) {
  auto rir_itr = rir_map_.find(frame_sink_id);
  if (rir_itr == rir_map_.end()) {
    return;
  }
  rir_itr->second->input_router()->NotifySiteIsMobileOptimized(
      is_mobile_optimized);

  auto metadata_itr = frame_sink_metadata_map_.find(frame_sink_id);
  CHECK(metadata_itr != frame_sink_metadata_map_.end());
  FrameSinkMetadata& frame_sink_metadata = metadata_itr->second;
  CHECK(frame_sink_metadata.is_mobile_optimized != is_mobile_optimized);
  frame_sink_metadata.is_mobile_optimized = is_mobile_optimized;
  frame_sink_metadata.rir_support->NotifySiteIsMobileOptimized(
      is_mobile_optimized);
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

input::mojom::RenderInputRouterDelegateClient*
InputManager::GetRIRDelegateClientRemote(const FrameSinkId& frame_sink_id) {
  auto itr = rir_delegate_remote_map_.find(frame_sink_id);
  if (itr == rir_delegate_remote_map_.end()) {
    return nullptr;
  }
  return itr->second.get();
}

std::optional<bool> InputManager::IsDelegatedInkHovering(
    const FrameSinkId& frame_sink_id) {
  auto* support = frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
  if (!IsFrameMetadataAvailable(support) ||
      !support->GetLastActivatedFrameMetadata()->delegated_ink_metadata) {
    return std::nullopt;
  }
  return support->GetLastActivatedFrameMetadata()
      ->delegated_ink_metadata->is_hovering();
}


void InputManager::StateOnTouchTransfer(
    input::mojom::TouchTransferStatePtr state) {
#if BUILDFLAG(IS_ANDROID)
  auto iter = frame_sink_metadata_map_.find(state->root_widget_frame_sink_id);
  if (iter == frame_sink_metadata_map_.end()) {
    UMA_HISTOGRAM_ENUMERATION(
        kStateProcessingResultHistogram,
        InputOnVizStateProcessingResult::kCouldNotFindViewForFrameSinkId);
    android_state_transfer_handler_.StateOnTouchTransfer(
        std::move(state), /* rir_support= */ nullptr);
    return;
  }

  if (!GetRootCompositorFrameSinkId(state->root_widget_frame_sink_id)
           .is_valid()) {
    UMA_HISTOGRAM_ENUMERATION(
        kStateProcessingResultHistogram,
        InputOnVizStateProcessingResult::kFrameSinkIdNotAttachedToRootCFS);
    android_state_transfer_handler_.StateOnTouchTransfer(
        std::move(state), /* rir_support= */ nullptr);
    return;
  }

  RenderInputRouterSupportBase* support_base = iter->second.rir_support.get();
  CHECK(support_base);
  // TODO(crbug.com/404741207): Convert this to CHECK once the underlying
  // reason for crash is fixed.
  if (support_base->IsRenderInputRouterSupportChildFrame()) {
    UMA_HISTOGRAM_ENUMERATION(
        kStateProcessingResultHistogram,
        InputOnVizStateProcessingResult::kFrameSinkIdCorrespondsToChildView);
    android_state_transfer_handler_.StateOnTouchTransfer(
        std::move(state), /* rir_support= */ nullptr);
    return;
  }

  auto* support_android = static_cast<RenderInputRouterSupportAndroid*>(
      iter->second.rir_support.get());
  UMA_HISTOGRAM_ENUMERATION(
      kStateProcessingResultHistogram,
      InputOnVizStateProcessingResult::kProcessedSuccessfully);
  android_state_transfer_handler_.StateOnTouchTransfer(
      std::move(state), support_android->GetWeakPtr());
#endif
}

void InputManager::ForceEnableZoomStateChanged(
    bool force_enable_zoom,
    const FrameSinkId& frame_sink_id) {
  auto itr = rir_map_.find(frame_sink_id);
  if (itr != rir_map_.end()) {
    itr->second->SetForceEnableZoom(force_enable_zoom);
  }
}

void InputManager::StopFlingingOnViz(const FrameSinkId& frame_sink_id) {
  auto iter = frame_sink_metadata_map_.find(frame_sink_id);
  if (iter != frame_sink_metadata_map_.end()) {
    iter->second.rir_support->StopFlingingOnViz();
  }
}

void InputManager::RestartInputEventAckTimeoutIfNecessary(
    const FrameSinkId& frame_sink_id) {
  auto itr = rir_map_.find(frame_sink_id);
  if (itr == rir_map_.end()) {
    return;
  }
  itr->second->RestartInputEventAckTimeoutIfNecessary();
}

void InputManager::NotifyVisibilityChanged(const FrameSinkId& frame_sink_id,
                                           bool is_hidden) {
  auto itr = frame_sink_metadata_map_.find(frame_sink_id);
  if (itr == frame_sink_metadata_map_.end()) {
    return;
  }
  itr->second.rir_delegate->SetIsHidden(is_hidden);
}

void InputManager::ResetGestureDetection(
    const FrameSinkId& root_widget_frame_sink_id) {
#if BUILDFLAG(IS_ANDROID)
  auto iter = frame_sink_metadata_map_.find(root_widget_frame_sink_id);
  if (iter == frame_sink_metadata_map_.end()) {
    return;
  }

  RenderInputRouterSupportBase* support_base = iter->second.rir_support.get();
  CHECK(support_base);
  if (support_base->IsRenderInputRouterSupportChildFrame()) {
    // In case, ResetGestureDetection comes in before Viz side had a chance to
    // reconstruct RenderInputRouterSupport of correct type, just return without
    // doing anything, since there's no ongoing gesture anyways to reset.
    return;
  }

  auto* support_android =
      static_cast<RenderInputRouterSupportAndroid*>(support_base);
  support_android->ResetGestureDetection();
#endif
}

void InputManager::SetupRendererInputRouterDelegateRegistry(
    mojo::PendingReceiver<mojom::RendererInputRouterDelegateRegistry>
        receiver) {
  TRACE_EVENT("viz", "InputManager::SetupRendererInputRouterDelegateRegistry");
  registry_receiver_.Bind(std::move(receiver));
}

void InputManager::SetupRenderInputRouterDelegateConnection(
    const FrameSinkId& frame_sink_id,
    mojo::PendingAssociatedRemote<input::mojom::RenderInputRouterDelegateClient>
        rir_delegate_remote,
    mojo::PendingAssociatedReceiver<input::mojom::RenderInputRouterDelegate>
        rir_delegate_receiver) {
  TRACE_EVENT("viz", "InputManager::SetupRenderInputRouterDelegateConnection");
  rir_delegate_remote_map_[frame_sink_id].Bind(std::move(rir_delegate_remote));
  rir_delegate_remote_map_[frame_sink_id].set_disconnect_handler(
      base::BindOnce(&InputManager::OnRIRDelegateClientDisconnected,
                     base::Unretained(this), frame_sink_id));

  rir_delegate_receivers_.Add(this, std::move(rir_delegate_receiver));
}

void InputManager::NotifyRendererBlockStateChanged(
    bool blocked,
    const std::vector<FrameSinkId>& rirs) {
  for (auto& frame_sink_id : rirs) {
    auto itr = rir_map_.find(frame_sink_id);
    if (itr == rir_map_.end()) {
      continue;
    }
    itr->second->RenderProcessBlockedStateChanged(blocked);
  }
}

GpuServiceImpl* InputManager::GetGpuService() {
  return frame_sink_manager_->GetGpuService();
}

input::RenderInputRouter* InputManager::GetRenderInputRouterFromFrameSinkId(
    const FrameSinkId& id) {
  auto itr = rir_map_.find(id);
  if (itr == rir_map_.end()) {
    return nullptr;
  }
  return itr->second.get();
}

bool InputManager::ReturnInputBackToBrowser() {
#if BUILDFLAG(IS_ANDROID)
  if (!receiver_data_) {
    return false;
  }
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> viz_input_token_java(
      env,
      base::AndroidInputReceiverCompat::GetInstance()
          .AInputTransferToken_toJavaFn(
              env, receiver_data_->viz_input_token().a_input_transfer_token()));
  base::android::ScopedJavaGlobalRef<jobject> browser_input_token_java(
      env,
      base::AndroidInputReceiverCompat::GetInstance()
          .AInputTransferToken_toJavaFn(
              env,
              receiver_data_->browser_input_token().a_input_transfer_token()));

  return static_cast<bool>(Java_InputTransferHandlerViz_transferInput(
      env, viz_input_token_java, browser_input_token_java));
#endif  // BUILDFLAG(IS_ANDROID)

  // `ReturnInputBackToBrowser` is only being called from Android specific
  // usecases currently with InputVizard.
  NOTREACHED();
}

void InputManager::SetBeginFrameSource(const FrameSinkId& frame_sink_id,
                                       BeginFrameSource* begin_frame_source) {
  TRACE_EVENT("input", "InputManager::SetBeginFrameSource", "frame_sink_id",
              frame_sink_id);
  // Return early if |frame_sink_id| is associated with non layer tree frame
  // sink.
  auto itr = rir_map_.find(frame_sink_id);
  if (itr == rir_map_.end()) {
    return;
  }
  CHECK(itr->second.get());
  itr->second->SetBeginFrameSourceForFlingScheduler(begin_frame_source);
}

void InputManager::MaybeRecreateRootRenderInputRouterSupports(
    const FrameSinkId& root_frame_sink_id) {
  TRACE_EVENT_INSTANT(
      "input", "InputManager::MaybeRecreateRootRenderInputRouterSupports");

  auto children = frame_sink_manager_->GetChildrenByParent(root_frame_sink_id);
  for (auto& frame_sink_id : children) {
    auto iter = frame_sink_metadata_map_.find(frame_sink_id);
    // Only attempt to recreate RenderInputRouterSupport for `frame_sink_id`
    // associated with layer tree frame sinks.
    if (iter != frame_sink_metadata_map_.end() &&
        iter->second.rir_support->IsRenderInputRouterSupportChildFrame()) {
      FrameSinkMetadata& metadata = iter->second;
      metadata.rir_support.reset();
      auto* rir = rir_map_.find(frame_sink_id)->second.get();
      metadata.rir_support = MakeRenderInputRouterSupport(rir, frame_sink_id);
      metadata.rir_support->NotifySiteIsMobileOptimized(
          metadata.is_mobile_optimized);
    }
  }
}

void InputManager::RecreateRenderInputRouterSupport(
    const FrameSinkId& child_frame_sink_id,
    FrameSinkMetadata& frame_sink_metadata) {
  auto rir_map_it = rir_map_.find(child_frame_sink_id);
  CHECK(rir_map_it != rir_map_.end());
  input::RenderInputRouter* rir = rir_map_it->second.get();

  frame_sink_metadata.rir_support.reset();
  frame_sink_metadata.rir_support =
      MakeRenderInputRouterSupport(rir, child_frame_sink_id);
  frame_sink_metadata.rir_support->NotifySiteIsMobileOptimized(
      frame_sink_metadata.is_mobile_optimized);
}

std::unique_ptr<RenderInputRouterSupportBase>
InputManager::MakeRenderInputRouterSupport(input::RenderInputRouter* rir,
                                           const FrameSinkId& frame_sink_id) {
  TRACE_EVENT_INSTANT("input", "InputManager::MakeRenderInputRouterSupport");
  auto parent_id =
      frame_sink_manager_->GetOldestParentByChildFrameId(frame_sink_id);
  if (frame_sink_manager_->IsFrameSinkIdInRootSinkMap(parent_id)) {
#if BUILDFLAG(IS_ANDROID)
    return std::make_unique<RenderInputRouterSupportAndroid>(
        rir, this, frame_sink_id, GetGpuService());
#else
    // InputVizard only supports Android currently.
    NOTREACHED();
#endif
  }
  return std::make_unique<RenderInputRouterSupportChildFrame>(rir, this,
                                                              frame_sink_id);
}

void InputManager::OnRIRDelegateClientDisconnected(
    const FrameSinkId& frame_sink_id) {
  rir_delegate_remote_map_.erase(frame_sink_id);
}

#if BUILDFLAG(IS_ANDROID)
void InputManager::CreateOrReuseAndroidInputReceiver(
    const FrameSinkId& frame_sink_id,
    const gpu::SurfaceHandle& surface_handle) {
  CHECK(base::AndroidInputReceiverCompat::IsSupportAvailable());

  if (receiver_data_ && receiver_data_->root_frame_sink_id().is_valid()) {
    // Only allow input receiver "creation" for single root compositor frame
    // sink.
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kNotCreatingMoreThanOneReceiver);
    return;
  }

  if (!frame_sink_manager_->IsFrameSinkIdInRootSinkMap(frame_sink_id)) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kRootCompositorFrameSinkDestroyed);
    return;
  }

  // This results in a sync binder to Browser, the same call is made on
  // CompositorGpu thread as well but to keep the code simple and not having to
  // plumb through Android SurfaceControl and InputTransferToken, this duplicate
  // call is made from here.
  auto surface_record =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireJavaSurface(surface_handle);

  CHECK(std::holds_alternative<gl::ScopedJavaSurface>(
      surface_record.surface_variant));
  gl::ScopedJavaSurface& scoped_java_surface =
      std::get<gl::ScopedJavaSurface>(surface_record.surface_variant);

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

  if (!parent_input_surface->surface()) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullParentSurfaceControl);
    return;
  }

  scoped_refptr<gfx::SurfaceControl::Surface> input_surface =
      base::MakeRefCounted<gfx::SurfaceControl::Surface>(*parent_input_surface,
                                                         kInputSCName);
  if (!input_surface->surface()) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullChildSurfaceControl);
    return;
  }

  ALooper* looper = ALooper_prepare(0);
  if (!looper) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullLooper);
    return;
  }

  // TODO(crbug.com/409003682): Investigate in what scenarios Browser can send a
  // null token.
  if (!surface_record.host_input_token) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kNullBrowserInputToken);
    return;
  }

  input::ScopedInputTransferToken browser_input_token(
      surface_record.host_input_token.obj());
  if (!browser_input_token) {
    UMA_HISTOGRAM_ENUMERATION(
        kInputReceiverCreationResultHistogram,
        CreateAndroidInputReceiverResult::kFailedNullInputTransferToken);
    return;
  }

  std::unique_ptr<input::AndroidInputCallback> android_input_callback =
      std::make_unique<input::AndroidInputCallback>(
          frame_sink_id, &android_state_transfer_handler_);
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

  AInputReceiver* a_input_receiver;
  bool batched = base::FeatureList::IsEnabled(
      input::features::kUseAndroidBufferedInputDispatch);
  if (batched) {
    const gfx::AChoreographerCompat& a_choreographer_compat =
        gfx::AChoreographerCompat::Get();
    if (!a_choreographer_compat.supported) {
      UMA_HISTOGRAM_ENUMERATION(
          kInputReceiverCreationResultHistogram,
          CreateAndroidInputReceiverResult::kFailedChoreographerNotSupported);
      return;
    }

    // Note: This call relies on calling |ALooper_prepare| above because
    // |AChoreographer_getInstance| "must be called on an ALooper thread". See
    // https://developer.android.com/ndk/reference/group/choreographer#achoreographer_getinstance.
    AChoreographer* a_choreographer =
        a_choreographer_compat.AChoreographer_getInstanceFn();
    if (!a_choreographer) {
      UMA_HISTOGRAM_ENUMERATION(
          kInputReceiverCreationResultHistogram,
          CreateAndroidInputReceiverResult::kFailedNullChoreographer);
      return;
    }

    a_input_receiver =
        base::AndroidInputReceiverCompat::GetInstance()
            .AInputReceiver_createBatchedInputReceiverFn(
                a_choreographer, browser_input_token.a_input_transfer_token(),
                input_surface->surface(),
                callbacks.a_input_receiver_callbacks());
  } else {
    a_input_receiver =
        base::AndroidInputReceiverCompat::GetInstance()
            .AInputReceiver_createUnbatchedInputReceiverFn(
                looper, browser_input_token.a_input_transfer_token(),
                input_surface->surface(),
                callbacks.a_input_receiver_callbacks());
  }

  input::ScopedInputReceiver receiver(a_input_receiver);
  if (!receiver) {
    UMA_HISTOGRAM_ENUMERATION(kInputReceiverCreationResultHistogram,
                              CreateAndroidInputReceiverResult::kFailedUnknown);
    return;
  }

  input::ScopedInputTransferToken viz_input_token(a_input_receiver);
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

bool InputManager::TransferInputBackToBrowser() {
  return ReturnInputBackToBrowser();
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace viz
