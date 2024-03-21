// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/browser_renderer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

BrowserRenderer::BrowserRenderer(
    std::unique_ptr<UiInterface> ui,
    std::unique_ptr<GraphicsDelegate> graphics_delegate,
    size_t sliding_time_size)
    : graphics_delegate_(std::move(graphics_delegate)),
      ui_processing_time_(sliding_time_size),
      ui_(std::move(ui)) {}

BrowserRenderer::~BrowserRenderer() = default;

void BrowserRenderer::DrawBrowserFrame(base::TimeTicks current_time,
                                       const gfx::Transform& head_pose) {
  Draw(kUiFrame, current_time, head_pose);
}

void BrowserRenderer::DrawWebXrFrame(base::TimeTicks current_time,
                                     const gfx::Transform& head_pose) {
  Draw(kWebXrFrame, current_time, head_pose);
}

void BrowserRenderer::Draw(FrameType frame_type,
                           base::TimeTicks current_time,
                           const gfx::Transform& head_pose) {
  TRACE_EVENT1("gpu", __func__, "frame_type", frame_type);
  const auto& render_info =
      graphics_delegate_->GetRenderInfo(frame_type, head_pose);
  UpdateUi(render_info, current_time, frame_type);

  if (frame_type == kWebXrFrame) {
    if (ui_->HasWebXrOverlayElementsToDraw()) {
      DrawWebXrOverlay(render_info);
    }
  } else {
    DrawBrowserUi(render_info);
  }

  TRACE_COUNTER1("gpu", "VR UI timing (us)",
                 ui_processing_time_.GetAverage().InMicroseconds());
}

void BrowserRenderer::DrawWebXrOverlay(const RenderInfo& render_info) {
  TRACE_EVENT0("gpu", __func__);
  // Calculate optimized viewport and corresponding render info.
  const auto& recommended_fovs = graphics_delegate_->GetRecommendedFovs();
  const auto& fovs = ui_->GetMinimalFovForWebXrOverlayElements(
      render_info.left_eye_model.view_matrix, recommended_fovs.first,
      render_info.right_eye_model.view_matrix, recommended_fovs.second,
      graphics_delegate_->GetZNear());
  const auto& webxr_overlay_render_info =
      graphics_delegate_->GetOptimizedRenderInfoForFovs(fovs);

  ui_->DrawWebVrOverlayForeground(webxr_overlay_render_info);
}

void BrowserRenderer::DrawBrowserUi(const RenderInfo& render_info) {
  TRACE_EVENT0("gpu", __func__);
  ui_->Draw(render_info);
}

void BrowserRenderer::WatchElementForVisibilityStatusForTesting(
    std::optional<UiVisibilityState> visibility_expectation) {
  DCHECK(!ui_visibility_state_.has_value() ||
         !visibility_expectation.has_value())
      << "Attempted to watch a UI element "
         "for visibility changes with one "
         "in progress";
  ui_visibility_state_ = std::move(visibility_expectation);
}

void BrowserRenderer::UpdateUi(const RenderInfo& render_info,
                               base::TimeTicks current_time,
                               FrameType frame_type) {
  TRACE_EVENT0("gpu", __func__);

  // Update the render position of all UI elements.
  base::TimeTicks timing_start = base::TimeTicks::Now();
  ui_->OnBeginFrame(current_time, render_info.head_pose);

  if (ui_->SceneHasDirtyTextures()) {
    ui_->UpdateSceneTextures();
  }
  ReportElementVisibilityStatus(timing_start);

  base::TimeDelta scene_time = base::TimeTicks::Now() - timing_start;
  // Don't double-count the controller time that was part of the scene time.
  ui_processing_time_.AddSample(scene_time);
}

void BrowserRenderer::ReportElementVisibilityStatus(
    const base::TimeTicks& current_time) {
  if (!ui_visibility_state_.has_value()) {
    return;
  }
  base::TimeDelta time_since_start =
      current_time - ui_visibility_state_->start_time;
  if (ui_->GetElementVisibility(ui_visibility_state_->element_to_watch) ==
      ui_visibility_state_->expected_visibile) {
    ReportElementVisibilityResult(true);  // IN-TEST
  } else if (time_since_start > ui_visibility_state_->timeout_ms) {
    ReportElementVisibilityResult(false);  // IN-TEST
  }
}

void BrowserRenderer::ReportElementVisibilityResult(bool result) {
  // Grab the callback and then destroy 'ui_visibility_state_' to prevent
  // re-entrant behavior being blocked by having the 'ui_visibility_state_' or
  // overwriting it and then dropping our callback.
  auto callback = std::move(ui_visibility_state_->on_visibility_change_result);
  ui_visibility_state_ = std::nullopt;
  std::move(callback).Run(result);
}

}  // namespace vr
