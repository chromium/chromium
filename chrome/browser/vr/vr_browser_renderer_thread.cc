// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_browser_renderer_thread.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/graphics_delegate_win.h"
#include "ui/gfx/geometry/quaternion.h"

// To avoid conflicts with the macro from the Windows SDK...
#undef DrawState

namespace {
constexpr base::TimeDelta kWebVrInitialFrameTimeout = base::Seconds(5);
constexpr base::TimeDelta kWebVrSpinnerTimeout = base::Seconds(2);

constexpr float kEpsilon = 0.1f;
constexpr float kMaxPosition = 1000000;
constexpr float kMinPosition = -kMaxPosition;
bool g_overlay_ui_disabled_for_testing_ = false;

bool InRange(float val, float min = kMinPosition, float max = kMaxPosition) {
  return val > min && val < max;
}

}  // namespace

namespace vr {

VRBrowserRendererThread* VRBrowserRendererThread::instance_for_testing_ =
    nullptr;

VRBrowserRendererThread::VRBrowserRendererThread(
    mojo::PendingRemote<device::mojom::ImmersiveOverlay> overlay,
    const std::vector<device::mojom::XRViewPtr>& views)
    : overlay_(std::move(overlay)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(instance_for_testing_ == nullptr);
  instance_for_testing_ = this;

  for (auto& view : views) {
    if (view->eye == device::mojom::XREye::kLeft ||
        view->eye == device::mojom::XREye::kRight) {
      default_views_.push_back(view.Clone());
    }
  }

  StartWebXrTimeout();
}

VRBrowserRendererThread::~VRBrowserRendererThread() {
  StopWebXrTimeout();

  // Call Cleanup to ensure correct destruction order of VR-UI classes.
  StopOverlay();
  instance_for_testing_ = nullptr;
}

void VRBrowserRendererThread::StopOverlay() {
  browser_renderer_ = nullptr;
  started_ = false;
  graphics_ = nullptr;
  ui_ = nullptr;
  scheduler_ui_ = nullptr;
}

void VRBrowserRendererThread::StartWebXrTimeout() {
  if (g_overlay_ui_disabled_for_testing_) {
    return;
  }

  frame_timeout_running_ = true;
  overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                         draw_state_.ShouldDrawWebXR());

  if (!waiting_for_webxr_frame_) {
    waiting_for_webxr_frame_ = true;
    overlay_->RequestNotificationOnWebXrSubmitted(base::BindOnce(
        &VRBrowserRendererThread::OnWebXRSubmitted, base::Unretained(this)));
  }

  webxr_spinner_timeout_closure_.Reset(base::BindOnce(
      &VRBrowserRendererThread::OnWebXrTimeoutImminent,
      base::Unretained(
          this)));  // Unretained safe because we explicitly cancel.
  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_spinner_timeout_closure_.callback(),
                                kWebVrSpinnerTimeout);
  webxr_frame_timeout_closure_.Reset(base::BindOnce(
      &VRBrowserRendererThread::OnWebXrTimedOut,
      base::Unretained(
          this)));  // Unretained safe because we explicitly cancel.
  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_frame_timeout_closure_.callback(),
                                kWebVrInitialFrameTimeout);
}

void VRBrowserRendererThread::StopWebXrTimeout() {
  if (g_overlay_ui_disabled_for_testing_) {
    return;
  }

  if (!webxr_spinner_timeout_closure_.IsCancelled())
    webxr_spinner_timeout_closure_.Cancel();
  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();
  OnSpinnerVisibilityChanged(false);
  frame_timeout_running_ = false;
}

int VRBrowserRendererThread::GetNextRequestId() {
  current_request_id_++;
  if (current_request_id_ >= 0x10000)
    current_request_id_ = 0;
  return current_request_id_;
}

void VRBrowserRendererThread::OnWebXrTimeoutImminent() {
  OnSpinnerVisibilityChanged(true);
  scheduler_ui_->OnWebXrTimeoutImminent();
}

void VRBrowserRendererThread::OnWebXrTimedOut() {
  OnSpinnerVisibilityChanged(true);
  scheduler_ui_->OnWebXrTimedOut();
}

void VRBrowserRendererThread::UpdateOverlayState() {
  if (draw_state_.ShouldDrawUI()) {
    StartOverlay();
  }

  if (!g_overlay_ui_disabled_for_testing_) {
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
  }

  if (draw_state_.ShouldDrawUI()) {
    // Note that this is intentionally checked separately from if we should draw
    // the UI to prevent just auto-stopping the Overlay for tests, so that the
    // other logic can potentially run.
    if (!g_overlay_ui_disabled_for_testing_) {
      // If we don't have a graphics yet (because StartOverlay hasn't finished),
      // then postpone running the overlay update until it is.
      if (!graphics_) {
        // Unretained is safe since we maintain ownership of this callback.
        pending_overlay_update_ =
            base::BindOnce(&VRBrowserRendererThread::UpdateOverlayState,
                           base::Unretained(this));
        return;
      }

      overlay_->RequestNextOverlayPose(
          base::BindOnce(&VRBrowserRendererThread::OnPose,
                         base::Unretained(this), GetNextRequestId()));
    }
  } else {
    StopOverlay();
  }
}

void VRBrowserRendererThread::SetFramesThrottled(bool throttled) {
  if (frames_throttled_ == throttled)
    return;

  frames_throttled_ = throttled;

  if (g_overlay_ui_disabled_for_testing_) {
    return;
  }

  // TODO(crbug.com/40653353): If we try to re-start the timeouts after UI has
  // already been shown (e.g. a user takes their headset off for a permissions
  // prompt). Then the prompt UI doesn't seem to be dismissed immediately.
  if (!waiting_for_webxr_frame_)
    return;

  if (frames_throttled_) {
    StopWebXrTimeout();

    // TODO(alcooper): This is not necessarily the best thing to show, but it's
    // the best that we have right now.  It ensures that we submit *something*
    // rather than letting the default system "Stalled" UI take over, without
    // showing a message that the page is behaving badly.
    OnWebXrTimeoutImminent();
  } else {
    StartWebXrTimeout();
  }
}

void VRBrowserRendererThread::SetVisibleExternalPromptNotification(
    ExternalPromptNotificationType prompt) {
  if (!draw_state_.SetPrompt(prompt))
    return;

  UpdateOverlayState();

  if (!ui_) {
    // If the ui is dismissed, make sure that we don't *actually* have a prompt
    // state that we needed to set.
    DCHECK(prompt == ExternalPromptNotificationType::kPromptNone);
    return;
  }

  ui_->SetVisibleExternalPromptNotification(prompt);
}

void VRBrowserRendererThread::SetIndicatorsVisible(bool visible) {
  if (draw_state_.SetIndicatorsVisible(visible))
    UpdateOverlayState();
}

void VRBrowserRendererThread::OnSpinnerVisibilityChanged(bool visible) {
  if (draw_state_.SetSpinnerVisible(visible))
    UpdateOverlayState();
}

void VRBrowserRendererThread::SetCapturingState(
    const CapturingStateModel& active_capturing,
    const CapturingStateModel& background_capturing,
    const CapturingStateModel& potential_capturing) {
  if (ui_)
    ui_->SetCapturingState(active_capturing, background_capturing,
                           potential_capturing);
}

VRBrowserRendererThread*
VRBrowserRendererThread::GetInstanceForTesting() {
  return instance_for_testing_;
}

BrowserRenderer* VRBrowserRendererThread::GetBrowserRendererForTesting() {
  return browser_renderer_.get();
}

namespace {
// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kSlidingAverageSize = 5;
}  // namespace

void VRBrowserRendererThread::DisableOverlayForTesting() {
  g_overlay_ui_disabled_for_testing_ = true;
}

void VRBrowserRendererThread::StartOverlay() {
  if (started_)
    return;

  started_ = true;
  std::unique_ptr<GraphicsDelegate> graphics = GraphicsDelegate::Create();

  // We're going to pass the unique_ptr into the callback so grab a temporary
  // reference to it here to prevent a use-after-move. This keeps the member
  // null until we've been fully initialized.
  auto* initializing_graphics = graphics.get();
  initializing_graphics->Initialize(
      base::BindOnce(&VRBrowserRendererThread::OnGraphicsReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(graphics)));
}

void VRBrowserRendererThread::OnGraphicsReady(
    std::unique_ptr<GraphicsDelegate> initializing_graphics) {
  DVLOG(2) << __func__;
  // The graphics delegate will eventually be owned by the browser_renderer_,
  // but we need to keep a raw pointer to it.
  graphics_ = initializing_graphics.get();

  // We should have received valid views from the ui host before rendering.
  DCHECK(!default_views_.empty());
  graphics_->SetXrViews(default_views_);

  graphics_->BindContext();

  // Create a vr::Ui
  std::unique_ptr<Ui> ui = std::make_unique<Ui>();
  static_cast<UiInterface*>(ui.get())->OnGlInitialized();
  ui_ = static_cast<BrowserUiInterface*>(ui.get());
  scheduler_ui_ = static_cast<UiInterface*>(ui.get())->GetSchedulerUiPtr();

  // Create the BrowserRenderer to drive UI rendering based on the delegates.
  browser_renderer_ = std::make_unique<BrowserRenderer>(
      std::move(ui), std::move(initializing_graphics), kSlidingAverageSize);

  graphics_->ClearContext();

  if (pending_overlay_update_) {
    std::move(pending_overlay_update_).Run();
  }
}

void VRBrowserRendererThread::OnWebXRSubmitted() {
  waiting_for_webxr_frame_ = false;
  if (scheduler_ui_)
    scheduler_ui_->OnWebXrFrameAvailable();

  StopWebXrTimeout();
}

// Ensures that relevant XRRendererInfo entries are valid and returns patched up
// XRRendererInfo to ensure that we always use normalized orientation
// quaternion, and that we do not use position with out-of-range values.
// In case the received data does not contain position and/or orientation, they
// will be set to default values.
device::mojom::XRRenderInfoPtr ValidateFrameData(
    device::mojom::XRRenderInfoPtr data) {
  device::mojom::XRRenderInfoPtr ret = device::mojom::XRRenderInfo::New();
  ret->mojo_from_viewer = device::mojom::VRPose::New();

  if (data->mojo_from_viewer) {
    if (data->mojo_from_viewer->orientation) {
      if (abs(data->mojo_from_viewer->orientation->Length() - 1) < kEpsilon) {
        ret->mojo_from_viewer->orientation =
            data->mojo_from_viewer->orientation->Normalized();
      }
    }

    if (data->mojo_from_viewer->position) {
      ret->mojo_from_viewer->position = data->mojo_from_viewer->position;

      bool any_out_of_range = !(InRange(ret->mojo_from_viewer->position->x()) &&
                                InRange(ret->mojo_from_viewer->position->y()) &&
                                InRange(ret->mojo_from_viewer->position->z()));
      if (any_out_of_range) {
        ret->mojo_from_viewer->position = std::nullopt;
        // If testing with unexpectedly high values, catch on debug builds
        // rather than silently change data.  On release builds its better to
        // be safe and validate.
        DCHECK(false);
      }
    }
  }  // if (data->mojo_from_viewer)

  if (!ret->mojo_from_viewer->orientation) {
    ret->mojo_from_viewer->orientation = gfx::Quaternion();
  }

  if (!ret->mojo_from_viewer->position) {
    ret->mojo_from_viewer->position = gfx::Point3F();
  }

  ret->views.resize(data->views.size());
  for (size_t i = 0; i < data->views.size(); i++) {
    ret->views[i] = std::move(data->views[i]);
  }

  ret->frame_id = data->frame_id;

  return ret;
}

void VRBrowserRendererThread::OnPose(int request_id,
                                        device::mojom::XRRenderInfoPtr data) {
  if (request_id != current_request_id_) {
    // Old request. Do nothing.
    return;
  }

  if (!draw_state_.ShouldDrawUI()) {
    // We shouldn't be showing UI.
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
    if (graphics_)
      graphics_->ResetMemoryBuffer();
    return;
  }

  data = ValidateFrameData(std::move(data));

  // If we're getting poses and should be drawing, StartOverlay() should have
  // initialized graphics_.
  DCHECK(graphics_);
  graphics_->SetXrViews(std::move(data->views));

  if (!PreRender())
    return;

  // Deliver pose to input and scheduler.
  DCHECK(data);
  DCHECK(data->mojo_from_viewer);
  DCHECK(data->mojo_from_viewer->orientation);
  DCHECK(data->mojo_from_viewer->position);
  const gfx::Point3F& pos = *data->mojo_from_viewer->position;

  // The incoming pose represents where the headset is in "world space".  So
  // we'll need to invert to get the view transform.
  gfx::Transform head_from_unoriented_head(
      data->mojo_from_viewer->orientation->inverse());

  // Negating all components will invert the translation.
  gfx::Transform unoriented_head_from_world;
  unoriented_head_from_world.Translate3d(-pos.x(), -pos.y(), -pos.z());

  // Compose these to get the base "view" matrix (before accounting for per-eye
  // transforms).
  gfx::Transform head_from_world =
      head_from_unoriented_head * unoriented_head_from_world;

  base::TimeTicks now = base::TimeTicks::Now();
  bool need_submit = false;
  if (draw_state_.ShouldDrawWebXR()) {
    browser_renderer_->DrawWebXrFrame(now, head_from_world);
    need_submit = true;
  } else if (draw_state_.ShouldDrawUI()) {
    browser_renderer_->DrawBrowserFrame(now, head_from_world);
    need_submit = true;
  }

  if (need_submit) {
    SubmitFrame(data->frame_id);
  }
}

bool VRBrowserRendererThread::PreRender() {
  // GraphicsDelegate::PreRender can fail if the context has become lost
  // due to hybrid adapter switching. Giving up on life means no overlays are
  // submitted to the XR process, causing it hang, waiting forever. Instead,
  // we shutdown and restart the overlay system, re-establishing the GPU process
  // connection and all of the graphics related state in vr::Ui.
  if (!graphics_->PreRender()) {
    StopOverlay();
    StartOverlay();
    // StartOverlay is asynchronous, so we may not have a graphics_ again
    // immediately. We'll essentially bail on this pose and ask for a new one
    // once the connection has been re-established.
    if (!graphics_) {
      // Unretained is safe since we maintain ownership of this callback.
      pending_overlay_update_ =
          base::BindOnce(&VRBrowserRendererThread::UpdateOverlayState,
                         base::Unretained(this));
      return false;
    }
    return graphics_->PreRender();
  }
  return true;
}

void VRBrowserRendererThread::SubmitFrame(int16_t frame_id) {
  DVLOG(3) << __func__ << " frame_id=" << frame_id;
  graphics_->PostRender();

  overlay_->SubmitOverlayTexture(
      frame_id, graphics_->GetTexture(), graphics_->GetSyncToken(),
      graphics_->GetLeft(), graphics_->GetRight(),
      base::BindOnce(&VRBrowserRendererThread::SubmitResult,
                     base::Unretained(this)));
}

void VRBrowserRendererThread::SubmitResult(bool success) {
  DVLOG(3) << __func__ << " success=" << success;
  if (!success && graphics_) {
    graphics_->ResetMemoryBuffer();
  }

  // Make sure that we only notify that a WebXr frame is now
  if (scheduler_ui_ && success && !frame_timeout_running_) {
    scheduler_ui_->OnWebXrFrameAvailable();
  }

  if (draw_state_.ShouldDrawUI() && started_) {
    DVLOG(3) << __func__ << " Requesting Overlay Pose";
    overlay_->RequestNextOverlayPose(
        base::BindOnce(&VRBrowserRendererThread::OnPose,
                       base::Unretained(this), GetNextRequestId()));
  }
}

// VRBrowserRendererThread::DrawContentType functions.
bool VRBrowserRendererThread::DrawState::ShouldDrawUI() {
  return prompt_ != ExternalPromptNotificationType::kPromptNone ||
         spinner_visible_ || indicators_visible_;
}

bool VRBrowserRendererThread::DrawState::ShouldDrawWebXR() {
  return ((prompt_ == ExternalPromptNotificationType::kPromptNone ||
           indicators_visible_) &&
          !spinner_visible_);
}

bool VRBrowserRendererThread::DrawState::SetPrompt(
    ExternalPromptNotificationType prompt) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  prompt_ = prompt;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

bool VRBrowserRendererThread::DrawState::SetSpinnerVisible(bool visible) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  spinner_visible_ = visible;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

bool VRBrowserRendererThread::DrawState::SetIndicatorsVisible(bool visible) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  indicators_visible_ = visible;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

}  // namespace vr
