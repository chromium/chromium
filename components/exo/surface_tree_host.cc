// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface_tree_host.h"

#include <algorithm>

#include "base/macros.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/presentation_feedback.h"

namespace exo {

namespace {

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(SurfaceTreeHost* surface_tree_host)
      : surface_tree_host_(surface_tree_host) {}
  ~CustomWindowTargeter() override = default;

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    if (window != surface_tree_host_->host_window())
      return aura::WindowTargeter::EventLocationInsideBounds(window, event);

    Surface* surface = surface_tree_host_->root_surface();
    if (!surface)
      return false;

    gfx::Point local_point = event.location();

    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);
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
  SurfaceTreeHost* const surface_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, public:

SurfaceTreeHost::SurfaceTreeHost(const std::string& window_name)
    : host_window_(
          std::make_unique<aura::Window>(nullptr,
                                         aura::client::WINDOW_TYPE_CONTROL)) {
  host_window_->SetName(window_name);
  host_window_->Init(ui::LAYER_SOLID_COLOR);
  host_window_->set_owned_by_parent(false);
  // The host window is a container of surface tree. It doesn't handle pointer
  // events.
  host_window_->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kDescendantsOnly);
  host_window_->SetEventTargeter(std::make_unique<CustomWindowTargeter>(this));
  layer_tree_frame_sink_holder_ = std::make_unique<LayerTreeFrameSinkHolder>(
      this, host_window_->CreateLayerTreeFrameSink());
  aura::Env::GetInstance()->context_factory()->AddObserver(this);
}

SurfaceTreeHost::~SurfaceTreeHost() {
  aura::Env::GetInstance()->context_factory()->RemoveObserver(this);
  SetRootSurface(nullptr);
  LayerTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(layer_tree_frame_sink_holder_));
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
    root_surface_->SetSurfaceDelegate(nullptr);
    // Force recreating resources when the surface is added to a tree again.
    root_surface_->SurfaceHierarchyResourcesLost();
    root_surface_ = nullptr;

    // Call all frame callbacks with a null frame time to indicate that they
    // have been cancelled.
    while (!frame_callbacks_.empty()) {
      frame_callbacks_.front().Run(base::TimeTicks());
      frame_callbacks_.pop_front();
    }

    DCHECK(presentation_callbacks_.empty());
    for (auto entry : active_presentation_callbacks_) {
      while (!entry.second.empty()) {
        entry.second.front().Run(gfx::PresentationFeedback());
        entry.second.pop_front();
      }
    }
    active_presentation_callbacks_.clear();
  }

  if (root_surface) {
    root_surface_ = root_surface;
    root_surface_->SetSurfaceDelegate(this);
    host_window_->AddChild(root_surface_->window());
    UpdateHostWindowBounds();
    root_surface_->window()->Show();
  }
}

bool SurfaceTreeHost::HasHitTestRegion() const {
  return root_surface_ && root_surface_->HasHitTestRegion();
}

void SurfaceTreeHost::GetHitTestMask(SkPath* mask) const {
  if (root_surface_)
    root_surface_->GetHitTestMask(mask);
}

void SurfaceTreeHost::DidReceiveCompositorFrameAck() {
  while (!frame_callbacks_.empty()) {
    frame_callbacks_.front().Run(base::TimeTicks::Now());
    frame_callbacks_.pop_front();
  }
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

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void SurfaceTreeHost::OnSurfaceCommit() {
  DCHECK(presentation_callbacks_.empty());
  root_surface_->CommitSurfaceHierarchy(false);
  UpdateHostWindowBounds();
}

bool SurfaceTreeHost::IsSurfaceSynchronized() const {
  // To host a surface tree, the root surface has to be desynchronized.
  DCHECK(root_surface_);
  return false;
}

bool SurfaceTreeHost::IsInputEnabled(Surface*) const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ui::ContextFactoryObserver overrides:

void SurfaceTreeHost::OnLostSharedContext() {
  if (!host_window_->GetSurfaceId().is_valid() || !root_surface_)
    return;
  root_surface_->SurfaceHierarchyResourcesLost();
  SubmitCompositorFrame();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, protected:

void SurfaceTreeHost::SubmitCompositorFrame() {
  viz::CompositorFrame frame = PrepareToSubmitCompositorFrame();

  root_surface_->AppendSurfaceHierarchyCallbacks(&frame_callbacks_,
                                                 &presentation_callbacks_);
  if (!presentation_callbacks_.empty()) {
    DCHECK_EQ(active_presentation_callbacks_.count(*next_token_), 0u);
    active_presentation_callbacks_[*next_token_] =
        std::move(presentation_callbacks_);
  } else {
    active_presentation_callbacks_[*next_token_] = PresentationCallbacks();
  }

  root_surface_->AppendSurfaceHierarchyContentsToFrame(
      root_surface_origin_, host_window()->layer()->device_scale_factor(),
      layer_tree_frame_sink_holder_->resource_manager(), &frame);

  std::vector<GLbyte*> sync_tokens;
  for (auto& resource : frame.resource_list)
    sync_tokens.push_back(resource.mailbox_holder.sync_token.GetData());
  ui::ContextFactory* context_factory =
      aura::Env::GetInstance()->context_factory();
  gpu::gles2::GLES2Interface* gles2 =
      context_factory->SharedMainThreadContextProvider()->ContextGL();
  gles2->VerifySyncTokensCHROMIUM(sync_tokens.data(), sync_tokens.size());

  layer_tree_frame_sink_holder_->SubmitCompositorFrame(std::move(frame));
}

void SurfaceTreeHost::SubmitEmptyCompositorFrame() {
  viz::CompositorFrame frame = PrepareToSubmitCompositorFrame();

  const std::unique_ptr<viz::RenderPass>& render_pass =
      frame.render_pass_list.back();
  const gfx::Rect quad_rect = gfx::Rect(0, 0, 1, 1);
  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      gfx::Transform(), /*quad_layer_rect=*/quad_rect,
      /*visible_quad_layer_rect=*/quad_rect,
      /*rounded_corner_bounds=*/gfx::RRectF(), /*clip_rect=*/gfx::Rect(),
      /*is_clipped=*/false, /*are_contents_opaque=*/true, /*opacity=*/1.f,
      /*blend_mode=*/SkBlendMode::kSrcOver, /*sorting_context_id=*/0);

  viz::SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, quad_rect, quad_rect, SK_ColorBLACK,
                     /*force_anti_aliasing_off=*/false);
  layer_tree_frame_sink_holder_->SubmitCompositorFrame(std::move(frame));
}

void SurfaceTreeHost::UpdateHostWindowBounds() {
  // This method applies multiple changes to the window tree. Use ScopedPause
  // to ensure that occlusion isn't recomputed before all changes have been
  // applied.
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;

  gfx::Rect bounds = root_surface_->surface_hierarchy_content_bounds();
  host_window_->SetBounds(
      gfx::Rect(host_window_->bounds().origin(), bounds.size()));
  const bool fills_bounds_opaquely =
      bounds.size() == root_surface_->content_size() &&
      root_surface_->FillsBoundsOpaquely();
  host_window_->SetTransparent(!fills_bounds_opaquely);

  root_surface_origin_ = gfx::Point() - bounds.OffsetFromOrigin();
  root_surface_->window()->SetBounds(gfx::Rect(
      root_surface_origin_, root_surface_->window()->bounds().size()));
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, private:

viz::CompositorFrame SurfaceTreeHost::PrepareToSubmitCompositorFrame() {
  DCHECK(root_surface_);

  if (layer_tree_frame_sink_holder_->is_lost()) {
    // We can immediately delete the old LayerTreeFrameSinkHolder because all of
    // it's resources are lost anyways.
    layer_tree_frame_sink_holder_ = std::make_unique<LayerTreeFrameSinkHolder>(
        this, host_window_->CreateLayerTreeFrameSink());
  }

  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.frame_token = ++next_token_;
  frame.render_pass_list.push_back(viz::RenderPass::Create());
  const std::unique_ptr<viz::RenderPass>& render_pass =
      frame.render_pass_list.back();

  const int kRenderPassId = 1;
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
  // because  the size is different.
  const float device_scale_factor =
      host_window()->layer()->device_scale_factor();
  const gfx::Size output_surface_size_in_pixels = gfx::ConvertSizeToPixel(
      device_scale_factor, host_window_->bounds().size());
  render_pass->SetNew(kRenderPassId, gfx::Rect(output_surface_size_in_pixels),
                      gfx::Rect(), gfx::Transform());
  frame.metadata.device_scale_factor = device_scale_factor;
  frame.metadata.local_surface_id_allocation_time =
      host_window()->GetLocalSurfaceIdAllocation().allocation_time();

  return frame;
}

}  // namespace exo
