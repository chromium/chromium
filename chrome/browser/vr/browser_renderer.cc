// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/browser_renderer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/browser_renderer_browser_interface.h"
#include "chrome/browser/vr/input_delegate_for_testing.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/scheduler_delegate.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_test_input.h"

namespace vr {

BrowserRenderer::BrowserRenderer(
    std::unique_ptr<UiInterface> ui,
    std::unique_ptr<SchedulerDelegate> scheduler_delegate,
    std::unique_ptr<GraphicsDelegate> graphics_delegate,
    std::unique_ptr<InputDelegate> input_delegate,
    BrowserRendererBrowserInterface* browser,
    size_t sliding_time_size)
    : scheduler_delegate_(std::move(scheduler_delegate)),
      graphics_delegate_(std::move(graphics_delegate)),
      input_delegate_(std::move(input_delegate)),
      browser_(browser),
      ui_processing_time_(sliding_time_size),
      ui_controller_update_time_(sliding_time_size),
      ui_(std::move(ui)) {
  scheduler_delegate_->SetBrowserRenderer(this);
}

BrowserRenderer::~BrowserRenderer() = default;

void BrowserRenderer::DrawBrowserFrame(base::TimeTicks current_time) {
  Draw(kUiFrame, current_time, input_delegate_->GetHeadPose());
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
  ui_->OnProjMatrixChanged(render_info.left_eye_model.proj_matrix);

  graphics_delegate_->InitializeBuffers();
  if (frame_type == kWebXrFrame) {
    DrawWebXr();
    if (ui_->HasWebXrOverlayElementsToDraw()) {
      DrawWebXrOverlay(render_info);
    }
  } else {
    DrawBrowserUi(render_info);
  }

  TRACE_COUNTER2("gpu", "VR UI timing (us)", "scene update",
                 ui_processing_time_.GetAverage().InMicroseconds(),
                 "controller",
                 ui_controller_update_time_.GetAverage().InMicroseconds());

  scheduler_delegate_->SubmitDrawnFrame(frame_type, head_pose);
}

void BrowserRenderer::DrawWebXr() {
  TRACE_EVENT0("gpu", __func__);
  graphics_delegate_->PrepareBufferForWebXr();

  int texture_id;
  GraphicsDelegate::Transform uv_transform;
  graphics_delegate_->GetWebXrDrawParams(&texture_id, &uv_transform);
  ui_->DrawWebXr(texture_id, uv_transform);
  graphics_delegate_->OnFinishedDrawingBuffer();
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

  graphics_delegate_->PrepareBufferForWebXrOverlayElements();
  ui_->DrawWebVrOverlayForeground(webxr_overlay_render_info);
  graphics_delegate_->OnFinishedDrawingBuffer();
}

void BrowserRenderer::DrawBrowserUi(const RenderInfo& render_info) {
  TRACE_EVENT0("gpu", __func__);
  graphics_delegate_->PrepareBufferForBrowserUi();
  ui_->Draw(render_info);
  graphics_delegate_->OnFinishedDrawingBuffer();
}

void BrowserRenderer::OnPause() {
  DCHECK(input_delegate_);
  input_delegate_->OnPause();
  scheduler_delegate_->OnPause();
  ui_->OnPause();
}

void BrowserRenderer::OnResume() {
  DCHECK(input_delegate_);
  scheduler_delegate_->OnResume();
  input_delegate_->OnResume();
}

void BrowserRenderer::OnExitPresent() {
  scheduler_delegate_->OnExitPresent();
}

void BrowserRenderer::OnTriggerEvent(bool pressed) {
  input_delegate_->OnTriggerEvent(pressed);
}

base::WeakPtr<BrowserUiInterface> BrowserRenderer::GetBrowserUiWeakPtr() {
  return ui_->GetBrowserUiWeakPtr();
}

void BrowserRenderer::SetUiExpectingActivityForTesting(
    UiTestActivityExpectation ui_expectation) {
  DCHECK(ui_test_state_ == nullptr)
      << "Attempted to set a UI activity expectation with one in progress";
  ui_test_state_ = std::make_unique<UiTestState>();
  ui_test_state_->quiescence_timeout_ms =
      base::Milliseconds(ui_expectation.quiescence_timeout_ms);
}

void BrowserRenderer::WatchElementForVisibilityStatusForTesting(
    VisibilityChangeExpectation visibility_expectation) {
  DCHECK(ui_visibility_state_ == nullptr) << "Attempted to watch a UI element "
                                             "for visibility changes with one "
                                             "in progress";
  ui_visibility_state_ = std::make_unique<UiVisibilityState>();
  ui_visibility_state_->timeout_ms =
      base::Milliseconds(visibility_expectation.timeout_ms);
  ui_visibility_state_->element_to_watch = visibility_expectation.element_name;
  ui_visibility_state_->expected_visibile = visibility_expectation.visibility;
}

void BrowserRenderer::SetBrowserRendererBrowserInterfaceForTesting(
    BrowserRendererBrowserInterface* interface_ptr) {
  browser_ = interface_ptr;
}

void BrowserRenderer::UpdateUi(const RenderInfo& render_info,
                               base::TimeTicks current_time,
                               FrameType frame_type) {
  TRACE_EVENT0("gpu", __func__);

  // Update the render position of all UI elements.
  base::TimeTicks timing_start = base::TimeTicks::Now();
  ui_->OnBeginFrame(current_time, render_info.head_pose);

  if (ui_->SceneHasDirtyTextures()) {
    if (!graphics_delegate_->RunInSkiaContext(base::BindOnce(
            &UiInterface::UpdateSceneTextures, base::Unretained(ui_.get())))) {
      browser_->ForceExitVr();
      return;
    }
  }
  ReportElementVisibilityStatusForTesting(timing_start);

  base::TimeDelta scene_time = base::TimeTicks::Now() - timing_start;
  // Don't double-count the controller time that was part of the scene time.
  ui_processing_time_.AddSample(scene_time);
}

void BrowserRenderer::ProcessControllerInputForWebXr(
    const gfx::Transform& head_pose,
    base::TimeTicks current_time) {
  TRACE_EVENT0("gpu", "Vr.ProcessControllerInputForWebXr");
  DCHECK(input_delegate_);
  DCHECK(ui_);
  base::TimeTicks timing_start = base::TimeTicks::Now();

  input_delegate_->UpdateController(head_pose, current_time, true);
  auto input_event_list = input_delegate_->GetGestures(current_time);
  ui_->HandleMenuButtonEvents(&input_event_list);

  ui_controller_update_time_.AddSample(base::TimeTicks::Now() - timing_start);

  scheduler_delegate_->AddInputSourceState(
      input_delegate_->GetInputSourceState());
}

void BrowserRenderer::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  scheduler_delegate_->ConnectPresentingService(std::move(options));
}

base::WeakPtr<BrowserRenderer> BrowserRenderer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserRenderer::ReportElementVisibilityStatusForTesting(
    const base::TimeTicks& current_time) {
  if (ui_visibility_state_ == nullptr)
    return;
  base::TimeDelta time_since_start =
      current_time - ui_visibility_state_->start_time;
  if (ui_->GetElementVisibilityForTesting(
          ui_visibility_state_->element_to_watch) ==
      ui_visibility_state_->expected_visibile) {
    ReportElementVisibilityResultForTesting(
        UiTestOperationResult::kVisibilityMatch);
  } else if (time_since_start > ui_visibility_state_->timeout_ms) {
    ReportElementVisibilityResultForTesting(
        UiTestOperationResult::kTimeoutNoVisibilityMatch);
  }
}

void BrowserRenderer::ReportElementVisibilityResultForTesting(
    UiTestOperationResult result) {
  ui_visibility_state_ = nullptr;
  browser_->ReportUiOperationResultForTesting(
      UiTestOperationType::kElementVisibilityStatus, result);
}

}  // namespace vr
