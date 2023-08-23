// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface_tree_host.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/presentation_feedback.h"

namespace exo {

namespace {

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(SurfaceTreeHost* surface_tree_host)
      : surface_tree_host_(surface_tree_host) {}

  CustomWindowTargeter(const CustomWindowTargeter&) = delete;
  CustomWindowTargeter& operator=(const CustomWindowTargeter&) = delete;

  ~CustomWindowTargeter() override = default;

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    if (window != surface_tree_host_->host_window())
      return aura::WindowTargeter::EventLocationInsideBounds(window, event);

    Surface* surface = surface_tree_host_->root_surface();
    if (!surface)
      return false;

    gfx::Point local_point =
        ConvertEventLocationToWindowCoordinates(window, event);

    aura::Window::ConvertPointToTarget(window, surface->window(), &local_point);
    return surface->HitTest(local_point);
  }

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    aura::Window* window = static_cast<aura::Window*>(root);
    if (window != surface_tree_host_->host_window())
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    ui::EventTarget* target =
        aura::WindowTargeter::FindTargetForEvent(root, event);
    // Do not accept events in SurfaceTreeHost window.
    return target != root ? target : nullptr;
  }

 private:
  const raw_ptr<SurfaceTreeHost, ExperimentalAsh> surface_tree_host_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, public:

SurfaceTreeHost::SurfaceTreeHost(const std::string& window_name)
    : SurfaceTreeHost(window_name, nullptr) {}

SurfaceTreeHost::SurfaceTreeHost(const std::string& window_name,
                                 std::unique_ptr<aura::Window> host_window)
    : host_window_(host_window ? std::move(host_window)
                               : std::make_unique<aura::Window>(
                                     nullptr,
                                     aura::client::WINDOW_TYPE_CONTROL)),
      frame_sink_holder_factory_(
          base::BindRepeating(&SurfaceTreeHost::CreateLayerTreeFrameSinkHolder,
                              base::Unretained(this))) {
  InitHostWindow(window_name);
  context_provider_ = aura::Env::GetInstance()
                          ->context_factory()
                          ->SharedMainThreadContextProvider();
  DCHECK(context_provider_);
  context_provider_->AddObserver(this);
}

SurfaceTreeHost::~SurfaceTreeHost() {
  context_provider_->RemoveObserver(this);

  SetRootSurface(nullptr);
  LayerTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(layer_tree_frame_sink_holder_));
  CleanUpCallbacks();

  if (frame_sink_id_.is_valid()) {
    auto* context_factory = aura::Env::GetInstance()->context_factory();
    auto* host_frame_sink_manager = context_factory->GetHostFrameSinkManager();
    host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
  }
}

void SurfaceTreeHost::SetRootSurface(Surface* root_surface) {
  if (root_surface == root_surface_)
    return;

  // This method applies multiple changes to the window tree. Use ScopedPause to
  // ensure that occlusion isn't recomputed before all changes have been
  // applied.
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;

  if (root_surface_) {
    root_surface_->window()->Hide();
    host_window_->RemoveChild(root_surface_->window());
    host_window_->SetBounds(
        gfx::Rect(host_window_->bounds().origin(), gfx::Size()));
    AllocateLocalSurfaceId();
    if (!layer_tree_frame_sink_holder_->is_lost()) {
      layer_tree_frame_sink_holder_->SetLocalSurfaceId(
          GetCurrentLocalSurfaceId());
    }
    MaybeActivateSurface();
    root_surface_->SetSurfaceDelegate(nullptr);
    // Force recreating resources when the surface is added to a tree again.
    root_surface_->SurfaceHierarchyResourcesLost();
    root_surface_ = nullptr;
  }

  if (root_surface) {
    root_surface_ = root_surface;
    root_surface_->SetSurfaceDelegate(this);

    if (client_submits_surfaces_in_pixel_coordinates_) {
      SetScaleFactorTransform(GetScaleFactor());
    }
    host_window_->AddChild(root_surface_->window());
    UpdateHostWindowSizeAndRootSurfaceOrigin();
  }
  set_bounds_is_dirty(true);
}

bool SurfaceTreeHost::HasHitTestRegion() const {
  return root_surface_ && root_surface_->HasHitTestRegion();
}

void SurfaceTreeHost::GetHitTestMask(SkPath* mask) const {
  if (root_surface_)
    root_surface_->GetHitTestMask(mask);
}

void SurfaceTreeHost::DidReceiveCompositorFrameAck() {
  const base::TimeTicks now = base::TimeTicks::Now();
  for (auto& callback : frame_callbacks_.front()) {
    callback.Run(now);
  }
  frame_callbacks_.pop();
}

void SurfaceTreeHost::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  auto it = active_presentation_callbacks_.find(presentation_token);
  if (it == active_presentation_callbacks_.end())
    return;
  for (auto callback : it->second)
    callback.Run(feedback);
  active_presentation_callbacks_.erase(it);
}

void SurfaceTreeHost::SetScaleFactor(float scale_factor) {
  pending_scale_factor_ = scale_factor;
}

void SurfaceTreeHost::SetSecurityDelegate(SecurityDelegate* security_delegate) {
  DCHECK(security_delegate_ == nullptr);
  security_delegate_ = security_delegate;
}

void SurfaceTreeHost::SubmitCompositorFrameForTesting(
    viz::CompositorFrame frame) {
  // Make sure that every submission has an entry pushed into
  // `frame_callbacks_`, which will be pop when ack is received.
  frame_callbacks_.emplace();
  active_presentation_callbacks_[frame.metadata.frame_token] =
      PresentationCallbacks();
  layer_tree_frame_sink_holder_->SubmitCompositorFrame(std::move(frame));
}

void SurfaceTreeHost::SetLayerTreeFrameSinkHolderFactoryForTesting(
    LayerTreeFrameSinkHolderFactory frame_sink_holder_factory) {
  DCHECK(frame_callbacks_.empty() && active_presentation_callbacks_.empty());

  frame_sink_holder_factory_ = std::move(frame_sink_holder_factory);
  layer_tree_frame_sink_holder_ = frame_sink_holder_factory_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void SurfaceTreeHost::OnSurfaceCommit() {
  root_surface_->CommitSurfaceHierarchy(false);
  UpdateHostWindowSizeAndRootSurfaceOrigin();
}

bool SurfaceTreeHost::IsSurfaceSynchronized() const {
  // To host a surface tree, the root surface has to be desynchronized.
  DCHECK(root_surface_);
  return false;
}

bool SurfaceTreeHost::IsInputEnabled(Surface*) const {
  return true;
}

void SurfaceTreeHost::OnNewOutputAdded() {
  UpdateDisplayOnTree();
}

SecurityDelegate* SurfaceTreeHost::GetSecurityDelegate() {
  DCHECK(security_delegate_);
  return security_delegate_;
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver:
void SurfaceTreeHost::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t changed_metrics) {
  // The output of the surface may change when the primary display changes.
  if (changed_metrics & DisplayObserver::DISPLAY_METRIC_PRIMARY)
    UpdateDisplayOnTree();
}

////////////////////////////////////////////////////////////////////////////////
// viz::ContextLostObserver overrides:

void SurfaceTreeHost::OnContextLost() {
  // Handle context loss in a new stack frame to avoid bugs from re-entrant
  // code.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SurfaceTreeHost::HandleContextLost,
                                weak_ptr_factory_.GetWeakPtr()));
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, protected:

void SurfaceTreeHost::UpdateDisplayOnTree() {
  auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(host_window());
  if (display_id_ != display.id()) {
    if (root_surface_) {
      if (root_surface_->UpdateDisplay(display_id_, display.id())) {
        display_id_ = display.id();
      } else {
        // The surface failed to update to the new display.
        // Invalidate cached display id, so the surface always gets updated
        // next time, even when it gets updated back to the previous display.
        display_id_ = display::kInvalidDisplayId;
      }
    }
  }
}

void SurfaceTreeHost::WillCommit() {
  scale_factor_ = pending_scale_factor_;
}

void SurfaceTreeHost::SubmitCompositorFrame() {
  viz::CompositorFrame frame = PrepareToSubmitCompositorFrame();

  // TODO(1041932,1034876): Remove or early return once these issues
  // are fixed or identified.
  if (frame.size_in_pixels().IsEmpty()) {
    aura::Window* toplevel = root_surface_->window()->GetToplevelWindow();
    auto app_type = toplevel->GetProperty(aura::client::kAppType);
    const std::string* app_id = GetShellApplicationId(toplevel);
    const std::string* startup_id = GetShellStartupId(toplevel);
    auto* shell_surface = GetShellSurfaceBaseForWindow(toplevel);
    CHECK(!frame.size_in_pixels().IsEmpty())
        << " Title=" << shell_surface->GetWindowTitle()
        << ", AppType=" << static_cast<int>(app_type)
        << ", AppId=" << (app_id ? *app_id : "''")
        << ", StartupId=" << (startup_id ? *startup_id : "''");
  }

  std::list<Surface::FrameCallback> current_frame_callbacks;
  PresentationCallbacks presentation_callbacks;
  root_surface_->AppendSurfaceHierarchyCallbacks(&current_frame_callbacks,
                                                 &presentation_callbacks);

  frame_callbacks_.push(std::move(current_frame_callbacks));

  const uint32_t frame_token = frame.metadata.frame_token;

  DCHECK_EQ(active_presentation_callbacks_.count(frame_token), 0u);
  active_presentation_callbacks_[frame_token] =
      std::move(presentation_callbacks);

  root_surface_->AppendSurfaceHierarchyContentsToFrame(
      gfx::PointF(root_surface_origin_),
      layer_tree_frame_sink_holder_->NeedsFullDamageForNextFrame(),
      layer_tree_frame_sink_holder_->resource_manager(),
      client_submits_surfaces_in_pixel_coordinates()
          ? absl::nullopt
          : absl::make_optional(GetScaleFactor()),
      &frame);

  std::vector<GLbyte*> sync_tokens;
  // We track previously verified tokens and set them to be verified to avoid
  // the considerable overhead of flush verification in
  // 'VerifySyncTokensCHROMIUM'.
  for (auto& resource : frame.resource_list) {
    if (prev_frame_verified_tokens_.find(resource.mailbox_holder.sync_token) !=
        prev_frame_verified_tokens_.end()) {
      resource.mailbox_holder.sync_token.SetVerifyFlush();
    }
    sync_tokens.push_back(resource.mailbox_holder.sync_token.GetData());
  }
  gpu::gles2::GLES2Interface* gles2 = context_provider_->ContextGL();
  gles2->VerifySyncTokensCHROMIUM(sync_tokens.data(), sync_tokens.size());

  prev_frame_verified_tokens_.clear();
  for (auto& resource : frame.resource_list) {
    if (resource.mailbox_holder.sync_token.verified_flush()) {
      prev_frame_verified_tokens_.insert(resource.mailbox_holder.sync_token);
    }
  }

  frame.metadata.content_color_usage = gfx::ContentColorUsage::kSRGB;
  for (auto& resource : frame.resource_list) {
    frame.metadata.content_color_usage =
        std::max(frame.metadata.content_color_usage,
                 resource.color_space.GetContentColorUsage());
  }

  frame.metadata.may_contain_video = root_surface_->ContainsVideo();

  layer_tree_frame_sink_holder_->SubmitCompositorFrame(std::move(frame));
}

void SurfaceTreeHost::SubmitEmptyCompositorFrame() {
  viz::CompositorFrame frame = PrepareToSubmitCompositorFrame();

  const std::unique_ptr<viz::CompositorRenderPass>& render_pass =
      frame.render_pass_list.back();
  const gfx::Rect quad_rect = gfx::Rect(0, 0, 1, 1);
  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(gfx::Transform(), /*layer_rect=*/quad_rect,
                     /*visible_layer_rect=*/quad_rect,
                     /*filter_info=*/gfx::MaskFilterInfo(),
                     /*clip=*/absl::nullopt,
                     /*contents_opaque=*/true, /*opacity_f=*/1.f,
                     /*blend=*/SkBlendMode::kSrcOver, /*sorting_context=*/0);

  viz::SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, quad_rect, quad_rect, SkColors::kBlack,
                     /*anti_aliasing_off=*/false);

  // Make sure that every submission has an entry pushed into
  // `frame_callbacks_`, which will be pop when ack is received.
  frame_callbacks_.emplace();
  active_presentation_callbacks_[frame.metadata.frame_token] =
      PresentationCallbacks();
  layer_tree_frame_sink_holder_->SubmitCompositorFrame(std::move(frame),
                                                       /*submit_now=*/true);
}

void SurfaceTreeHost::UpdateHostWindowSizeAndRootSurfaceOrigin() {
  // This method applies multiple changes to the window tree. Use ScopedPause
  // to ensure that occlusion isn't recomputed before all changes have been
  // applied.
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;

  const gfx::Rect& bounds = root_surface_->surface_hierarchy_content_bounds();
  gfx::Size size = bounds.size();
  if (client_submits_surfaces_in_pixel_coordinates_) {
    size = gfx::ScaleToCeiledSize(size, 1.0f / GetScaleFactor());
  }
  gfx::Rect scaled_bounds(bounds.origin(), size);
  if (scaled_bounds != host_window_->bounds()) {
    // DP size has changed, set new bounds.
    host_window_->SetBounds({host_window_->bounds().origin(), size});
  }

  // TODO(yjliu): a) consolidate with ClientControlledShellSurface. b) use the
  // scale factor the buffer is created for to set the transform for
  // synchronization.
  if (client_submits_surfaces_in_pixel_coordinates_) {
    SetScaleFactorTransform(GetScaleFactor());
  }
  const bool fills_bounds_opaquely =
      gfx::SizeF(bounds.size()) == root_surface_->content_size() &&
      root_surface_->FillsBoundsOpaquely();
  host_window_->SetTransparent(!fills_bounds_opaquely);

  root_surface_origin_ = gfx::Point() - bounds.OffsetFromOrigin();
  const gfx::Rect& window_bounds = root_surface_->window()->bounds();
  if (root_surface_origin_ != window_bounds.origin()) {
    gfx::Rect updated_bounds(root_surface_origin_, window_bounds.size());
    root_surface_->window()->SetBounds(updated_bounds);
  }
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, private:

void SurfaceTreeHost::InitHostWindow(const std::string& window_name) {
  host_window_->SetName(window_name);
  host_window_->Init(ui::LAYER_SOLID_COLOR);
  host_window_->set_owned_by_parent(false);
  // The host window is a container of surface tree. It doesn't handle pointer
  // events.
  host_window_->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kDescendantsOnly);
  host_window_->SetEventTargeter(std::make_unique<CustomWindowTargeter>(this));
  host_window_.get()->ui::LayerOwner::AddObserver(this);
  layer_tree_frame_sink_holder_ = frame_sink_holder_factory_.Run();
}

std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink>
SurfaceTreeHost::CreateLayerTreeFrameSink() {
  auto* context_factory = aura::Env::GetInstance()->context_factory();
  auto* host_frame_sink_manager = context_factory->GetHostFrameSinkManager();

  if (!frame_sink_id_.is_valid()) {
    frame_sink_id_ = context_factory->AllocateFrameSinkId();
    host_frame_sink_manager->RegisterFrameSinkId(
        frame_sink_id_, host_window(), viz::ReportFirstSurfaceActivation::kNo);
    host_window_->SetEmbedFrameSinkId(frame_sink_id_);
  }

  // For creating an async frame sink which connects to the viz display
  // compositor.
  mojo::PendingRemote<viz::mojom::CompositorFrameSink> sink_remote;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink_receiver =
      sink_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client_remote;
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      client_remote.InitWithNewPipeAndPassReceiver();
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(sink_receiver), std::move(client_remote));

  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.gpu_memory_buffer_manager =
      aura::Env::GetInstance()->context_factory()->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_remote = std::move(sink_remote);
  params.pipes.client_receiver = std::move(client_receiver);
  auto frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          nullptr /* context_provider */, nullptr /* worker_context_provider */,
          /*shared_image_interface=*/nullptr, &params);
  AllocateLocalSurfaceId();
  CHECK(GetCurrentLocalSurfaceId().is_valid());
  return frame_sink;
}

void SurfaceTreeHost::AllocateLocalSurfaceId() {
  if (!child_local_surface_id_allocator_) {
    child_local_surface_id_allocator_ =
        std::make_unique<viz::ChildLocalSurfaceIdAllocator>();
    child_local_surface_id_allocator_->UpdateFromParent(
        host_window_->GetLocalSurfaceId());
  }
  child_local_surface_id_allocator_->GenerateId();
}

void SurfaceTreeHost::UpdateLocalSurfaceIdFromParent(
    const viz::LocalSurfaceId& parent_local_surface_id) {
  child_local_surface_id_allocator_->UpdateFromParent(parent_local_surface_id);
}

void SurfaceTreeHost::MaybeActivateSurface() {
  DCHECK(!host_window_->layer()->GetSurfaceId() ||
         GetCurrentLocalSurfaceId().IsSameOrNewerThan(
             host_window_->layer()->GetSurfaceId()->local_surface_id()));

  if (host_window_->layer()->GetSurfaceId() &&
      !GetCurrentLocalSurfaceId().IsNewerThan(
          host_window_->layer()->GetSurfaceId()->local_surface_id())) {
    return;
  }

  host_window_->UpdateLocalSurfaceIdFromEmbeddedClient(
      GetCurrentLocalSurfaceId());
  host_window_->layer()->SetShowSurface(
      GetSurfaceId(), host_window_->bounds().size(), SK_ColorWHITE,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      false /* stretch_content_to_fill_bounds */);
}

viz::SurfaceId SurfaceTreeHost::GetSurfaceId() const {
  return viz::SurfaceId(frame_sink_id_, GetCurrentLocalSurfaceId());
}

const viz::LocalSurfaceId& SurfaceTreeHost::GetCurrentLocalSurfaceId() const {
  return child_local_surface_id_allocator_->GetCurrentLocalSurfaceId();
}

void SurfaceTreeHost::OnLayerRecreated(ui::Layer* old_layer) {
  // TODO(crbug/1251778): Track the old layer copies that are kept and advance
  // them to new local_surface_id in case the surface property changes, until
  // the copies' corresponding parent local_surface_id is synchronized to the
  // surface_tree_host.
}

viz::CompositorFrame SurfaceTreeHost::PrepareToSubmitCompositorFrame() {
  DCHECK(root_surface_);

  if (layer_tree_frame_sink_holder_->is_lost()) {
    // We can immediately delete the old LayerTreeFrameSinkHolder because all of
    // it's resources are lost anyways.
    layer_tree_frame_sink_holder_ = frame_sink_holder_factory_.Run();
    CleanUpCallbacks();
  }

  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.frame_token = GenerateNextFrameToken();
  frame.render_pass_list.push_back(viz::CompositorRenderPass::Create());
  const std::unique_ptr<viz::CompositorRenderPass>& render_pass =
      frame.render_pass_list.back();

  const viz::CompositorRenderPassId kRenderPassId{1};
  // Compute a temporally stable (across frames) size for the render pass output
  // rectangle that is consistent with the window size. It is used to set the
  // size of the output surface. Note that computing the actual coverage while
  // building up the render pass can lead to the size being one pixel too large,
  // especially if the device scale factor has a floating point representation
  // that requires many bits of precision in the mantissa, due to the coverage
  // computing an "enclosing" pixel rectangle. This isn't a problem for the
  // dirty rectangle, so it is updated as part of filling in the render pass.
  // Additionally, we must use this size even if we are submitting an empty
  // compositor frame, otherwise we may set the Surface created by Viz to be the
  // wrong size. Then, trying to submit a regular compositor frame will fail
  // because the size is different.
  const float device_scale_factor = GetScaleFactor();

  gfx::Size output_surface_size_in_pixels =
      root_surface_->surface_hierarchy_content_bounds().size();
  if (!client_submits_surfaces_in_pixel_coordinates_) {
    // TODO(crbug.com/1131628): Should this be ceil? Why do we choose floor?
    output_surface_size_in_pixels = gfx::ScaleToFlooredSize(
        output_surface_size_in_pixels, device_scale_factor);
  }

  // Viz will crash if the frame size is empty. Ensure it's not empty.
  // crbug.com/1041932.
  if (output_surface_size_in_pixels.IsEmpty())
    output_surface_size_in_pixels.SetSize(1, 1);

  render_pass->SetNew(kRenderPassId, gfx::Rect(output_surface_size_in_pixels),
                      gfx::Rect(), gfx::Transform());
  frame.metadata.device_scale_factor = device_scale_factor;

  if (output_surface_size_in_pixels !=
          layer_tree_frame_sink_holder_->LastSizeInPixels() ||
      device_scale_factor !=
          layer_tree_frame_sink_holder_->LastDeviceScaleFactor()) {
    AllocateLocalSurfaceId();
  }
  layer_tree_frame_sink_holder_->SetLocalSurfaceId(GetCurrentLocalSurfaceId());
  MaybeActivateSurface();

  return frame;
}

void SurfaceTreeHost::HandleContextLost() {
  // Stop observering the lost context.
  context_provider_->RemoveObserver(this);

  // Get new context and start observing it.
  context_provider_ = aura::Env::GetInstance()
                          ->context_factory()
                          ->SharedMainThreadContextProvider();
  DCHECK(context_provider_);
  context_provider_->AddObserver(this);

  if (!GetSurfaceId().is_valid() || !root_surface_) {
    return;
  }

  root_surface_->SurfaceHierarchyResourcesLost();
  SubmitCompositorFrame();
}

float SurfaceTreeHost::GetScaleFactor() const {
  return CalculateScaleFactor(scale_factor_);
}

float SurfaceTreeHost::GetPendingScaleFactor() const {
  return CalculateScaleFactor(pending_scale_factor_);
}

void SurfaceTreeHost::CleanUpCallbacks() {
  const base::TimeTicks now = base::TimeTicks::Now();
  while (!frame_callbacks_.empty()) {
    for (auto& callback : frame_callbacks_.front()) {
      callback.Run(now);
    }
    frame_callbacks_.pop();
  }

  for (auto entry : active_presentation_callbacks_) {
    while (!entry.second.empty()) {
      entry.second.front().Run(gfx::PresentationFeedback());
      entry.second.pop_front();
    }
  }
  active_presentation_callbacks_.clear();
}

std::unique_ptr<LayerTreeFrameSinkHolder>
SurfaceTreeHost::CreateLayerTreeFrameSinkHolder() {
  return std::make_unique<LayerTreeFrameSinkHolder>(this,
                                                    CreateLayerTreeFrameSink());
}

float SurfaceTreeHost::CalculateScaleFactor(
    const absl::optional<float>& scale_factor) const {
  if (scale_factor) {
    // TODO(crbug.com/1412420): Remove this once the scale factor precision
    // issue is fixed for ARC.
    if (std::abs(scale_factor.value() -
                 host_window_->layer()->device_scale_factor()) <
        display::kDeviceScaleFactorErrorTolerance) {
      return host_window_->layer()->device_scale_factor();
    }
    return scale_factor.value();
  }
  return host_window_->layer()->device_scale_factor();
}

void SurfaceTreeHost::SetScaleFactorTransform(float scale_factor) {
  DCHECK(client_submits_surfaces_in_pixel_coordinates_);

  gfx::Transform tr;
  tr.Scale(1.0f / scale_factor, 1.0f / scale_factor);
  if (root_surface()->window()->transform() != tr) {
    root_surface()->window()->SetTransform(tr);
  }
}

}  // namespace exo
