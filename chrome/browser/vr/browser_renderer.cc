// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/browser_renderer.h"

#include <utility>

#include "base/bind.h"
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
  bool use_quad_layer = ui_->IsContentVisibleAndOpaque() &&
                        graphics_delegate_->IsContentQuadReady();
  ui_->SetContentUsesQuadLayer(use_quad_layer);

  graphics_delegate_->InitializeBuffers();
  graphics_delegate_->SetFrameDumpFilepathBase(
      frame_buffer_dump_filepath_base_);
  if (frame_type == kWebXrFrame) {
    DCHECK(!use_quad_layer);
    DrawWebXr();
    if (ui_->HasWebXrOverlayElementsToDraw())
      DrawWebXrOverlay(render_info);
  } else {
    if (use_quad_layer)
      DrawContentQuad();
    DrawBrowserUi(render_info);
  }

  TRACE_COUNTER2("gpu", "VR UI timing (us)", "scene update",
                 ui_processing_time_.GetAverage().InMicroseconds(),
                 "controller",
                 ui_controller_update_time_.GetAverage().InMicroseconds());

  ReportFrameBufferDumpForTesting();
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

void BrowserRenderer::DrawContentQuad() {
  TRACE_EVENT0("gpu", __func__);
  graphics_delegate_->PrepareBufferForContentQuadLayer(
      ui_->GetContentWorldSpaceTransform());

  GraphicsDelegate::Transform uv_transform;
  float border_x;
  float border_y;
  graphics_delegate_->GetContentQuadDrawParams(&uv_transform, &border_x,
                                               &border_y);
  ui_->DrawContent(uv_transform, border_x, border_y);
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

void BrowserRenderer::SetWebXrMode(bool enabled) {
  scheduler_delegate_->SetWebXrMode(enabled);
}

void BrowserRenderer::EnableAlertDialog(PlatformInputHandler* input_handler,
                                        float width,
                                        float height) {
  scheduler_delegate_->SetShowingVrDialog(true);
  vr_dialog_input_delegate_ =
      std::make_unique<PlatformUiInputDelegate>(input_handler);
  vr_dialog_input_delegate_->SetSize(width, height);
  if (ui_->IsContentVisibleAndOpaque()) {
    auto content_width = graphics_delegate_->GetContentBufferWidth();
    DCHECK(content_width);
    ui_->SetContentOverlayAlertDialogEnabled(
        true, vr_dialog_input_delegate_.get(), width / content_width,
        height / content_width);
  } else {
    ui_->SetAlertDialogEnabled(true, vr_dialog_input_delegate_.get(), width,
                               height);
  }
}

void BrowserRenderer::DisableAlertDialog() {
  ui_->SetAlertDialogEnabled(false, nullptr, 0, 0);
  vr_dialog_input_delegate_ = nullptr;
  scheduler_delegate_->SetShowingVrDialog(false);
}

void BrowserRenderer::SetAlertDialogSize(float width, float height) {
  if (vr_dialog_input_delegate_)
    vr_dialog_input_delegate_->SetSize(width, height);
  // If not floating, dialogs are rendered with a fixed width, so that only the
  // ratio matters. But, if they are floating, its size should be relative to
  // the contents. During a WebXR presentation, the contents are not present
  // but, in this case, the dialogs are never floating.
  if (ui_->IsContentVisibleAndOpaque()) {
    auto content_width = graphics_delegate_->GetContentBufferWidth();
    DCHECK(content_width);
    ui_->SetContentOverlayAlertDialogEnabled(
        true, vr_dialog_input_delegate_.get(), width / content_width,
        height / content_width);
  } else {
    ui_->SetAlertDialogEnabled(true, vr_dialog_input_delegate_.get(), width,
                               height);
  }
}

void BrowserRenderer::ResumeContentRendering() {
  graphics_delegate_->ResumeContentRendering();
}

void BrowserRenderer::BufferBoundsChanged(
    const gfx::Size& content_buffer_size,
    const gfx::Size& overlay_buffer_size) {
  graphics_delegate_->BufferBoundsChanged(content_buffer_size,
                                          overlay_buffer_size);
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
      base::TimeDelta::FromMilliseconds(ui_expectation.quiescence_timeout_ms);
}

void BrowserRenderer::SaveNextFrameBufferToDiskForTesting(
    std::string filepath_base) {
  frame_buffer_dump_filepath_base_ = filepath_base;
}

void BrowserRenderer::WatchElementForVisibilityStatusForTesting(
    VisibilityChangeExpectation visibility_expectation) {
  DCHECK(ui_visibility_state_ == nullptr) << "Attempted to watch a UI element "
                                             "for visibility changes with one "
                                             "in progress";
  ui_visibility_state_ = std::make_unique<UiVisibilityState>();
  ui_visibility_state_->timeout_ms =
      base::TimeDelta::FromMilliseconds(visibility_expectation.timeout_ms);
  ui_visibility_state_->element_to_watch = visibility_expectation.element_name;
  ui_visibility_state_->expected_visibile = visibility_expectation.visibility;
}

void BrowserRenderer::AcceptDoffPromptForTesting() {
  ui_->AcceptDoffPromptForTesting();
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
  bool ui_updated = ui_->OnBeginFrame(current_time, render_info.head_pose);

  // WebXR handles controller input in OnVsync.
  base::TimeDelta controller_time;
  if (frame_type == kUiFrame)
    controller_time = ProcessControllerInput(render_info, current_time);

  if (ui_->SceneHasDirtyTextures()) {
    if (!graphics_delegate_->RunInSkiaContext(base::BindOnce(
            &UiInterface::UpdateSceneTextures, base::Unretained(ui_.get())))) {
      browser_->ForceExitVr();
      return;
    }
    ui_updated = true;
  }
  ReportUiStatusForTesting(timing_start, ui_updated);
  ReportElementVisibilityStatusForTesting(timing_start);

  base::TimeDelta scene_time = base::TimeTicks::Now() - timing_start;
  // Don't double-count the controller time that was part of the scene time.
  ui_processing_time_.AddSample(scene_time - controller_time);
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
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  scheduler_delegate_->ConnectPresentingService(std::move(display_info),
                                                std::move(options));
}

base::TimeDelta BrowserRenderer::ProcessControllerInput(
    const RenderInfo& render_info,
    base::TimeTicks current_time) {
  TRACE_EVENT0("gpu", "Vr.ProcessControllerInput");
  DCHECK(input_delegate_);
  DCHECK(ui_);
  base::TimeTicks timing_start = base::TimeTicks::Now();

  input_delegate_->UpdateController(render_info.head_pose, current_time, false);
  auto input_event_list = input_delegate_->GetGestures(current_time);
  ReticleModel reticle_model;
  ControllerModel controller_model =
      input_delegate_->GetControllerModel(render_info.head_pose);
  ui_->HandleInput(current_time, render_info, controller_model, &reticle_model,
                   &input_event_list);
  std::vector<ControllerModel> controller_models;
  controller_models.push_back(controller_model);
  ui_->OnControllersUpdated(controller_models, reticle_model);

  auto controller_time = base::TimeTicks::Now() - timing_start;
  ui_controller_update_time_.AddSample(controller_time);
  return controller_time;
}

void BrowserRenderer::PerformControllerActionForTesting(
    ControllerTestInput controller_input) {
  DCHECK(input_delegate_);
  if (controller_input.action == VrControllerTestAction::kRevertToRealInput) {
    if (using_input_delegate_for_testing_) {
      DCHECK(static_cast<InputDelegateForTesting*>(input_delegate_.get())
                 ->IsQueueEmpty())
          << "Attempted to revert to using real controller with actions still "
             "queued";
      using_input_delegate_for_testing_ = false;
      input_delegate_for_testing_.swap(input_delegate_);
      ui_->SetUiInputManagerForTesting(false);
    }
    return;
  }
  if (!using_input_delegate_for_testing_) {
    using_input_delegate_for_testing_ = true;
    if (!input_delegate_for_testing_)
      input_delegate_for_testing_ =
          std::make_unique<InputDelegateForTesting>(ui_.get());
    input_delegate_for_testing_.swap(input_delegate_);
    ui_->SetUiInputManagerForTesting(true);
  }
  if (controller_input.action != VrControllerTestAction::kEnableMockedInput) {
    static_cast<InputDelegateForTesting*>(input_delegate_.get())
        ->QueueControllerActionForTesting(controller_input);
  }
}

void BrowserRenderer::ReportUiStatusForTesting(
    const base::TimeTicks& current_time,
    bool ui_updated) {
  if (ui_test_state_ == nullptr)
    return;
  base::TimeDelta time_since_start = current_time - ui_test_state_->start_time;
  if (ui_updated) {
    ui_test_state_->activity_started = true;
    if (time_since_start > ui_test_state_->quiescence_timeout_ms) {
      // The UI is being updated, but hasn't reached a stable state in the
      // given time -> report timeout.
      ReportUiActivityResultForTesting(UiTestOperationResult::kTimeoutNoEnd);
    }
  } else {
    if (ui_test_state_->activity_started) {
      // The UI has been updated since the test requested notification of
      // quiescence, but wasn't this frame -> report that the UI is quiescent.
      ReportUiActivityResultForTesting(UiTestOperationResult::kQuiescent);
    } else if (time_since_start > ui_test_state_->quiescence_timeout_ms) {
      // The UI has never been updated and we've reached the timeout.
      ReportUiActivityResultForTesting(UiTestOperationResult::kTimeoutNoStart);
    }
  }
}

base::WeakPtr<BrowserRenderer> BrowserRenderer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BrowserRenderer::ReportUiActivityResultForTesting(
    UiTestOperationResult result) {
  ui_test_state_ = nullptr;
  browser_->ReportUiOperationResultForTesting(
      UiTestOperationType::kUiActivityResult, result);
}

void BrowserRenderer::ReportFrameBufferDumpForTesting() {
  if (frame_buffer_dump_filepath_base_.empty())
    return;

  frame_buffer_dump_filepath_base_.clear();
  browser_->ReportUiOperationResultForTesting(
      UiTestOperationType::kFrameBufferDumped,
      UiTestOperationResult::kQuiescent /* unused */);
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
