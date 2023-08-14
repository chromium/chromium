// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_initial_state.h"
#include "chrome/browser/vr/win/graphics_delegate_win.h"
#include "chrome/browser/vr/win/input_delegate_win.h"
#include "chrome/browser/vr/win/scheduler_delegate_win.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "ui/gfx/geometry/quaternion.h"

// To avoid conflicts with the macro from the Windows SDK...
#undef DrawState

namespace {
constexpr base::TimeDelta kWebVrInitialFrameTimeout = base::Seconds(5);
constexpr base::TimeDelta kWebVrSpinnerTimeout = base::Seconds(2);

constexpr float kEpsilon = 0.1f;
constexpr float kMaxPosition = 1000000;
constexpr float kMinPosition = -kMaxPosition;
bool g_frame_timeout_ui_disabled_for_testing_ = false;

bool InRange(float val, float min = kMinPosition, float max = kMaxPosition) {
  return val > min && val < max;
}

}  // namespace

namespace vr {

VRBrowserRendererThreadWin* VRBrowserRendererThreadWin::instance_for_testing_ =
    nullptr;

VRBrowserRendererThreadWin::VRBrowserRendererThreadWin(
    device::mojom::XRCompositorHost* compositor)
    : compositor_(compositor),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(instance_for_testing_ == nullptr);
  instance_for_testing_ = this;
}

VRBrowserRendererThreadWin::~VRBrowserRendererThreadWin() {
  // Call Cleanup to ensure correct destruction order of VR-UI classes.
  StopOverlay();
  instance_for_testing_ = nullptr;
}

void VRBrowserRendererThreadWin::StopOverlay() {
  browser_renderer_ = nullptr;
  initializing_graphics_ = nullptr;
  started_ = false;
  graphics_ = nullptr;
  scheduler_ = nullptr;
  ui_ = nullptr;
  scheduler_ui_ = nullptr;
}

void VRBrowserRendererThreadWin::SetDefaultXrViews(
    const std::vector<device::mojom::XRViewPtr>& views) {
  if (graphics_) {
    graphics_->SetXrViews(views);
  }

  for (auto& view : views) {
    if (view->eye == device::mojom::XREye::kLeft ||
        view->eye == device::mojom::XREye::kRight) {
      default_views_.push_back(view.Clone());
    }
  }
}

void VRBrowserRendererThreadWin::SetWebXrPresenting(bool presenting) {
  webxr_presenting_ = presenting;

  if (g_frame_timeout_ui_disabled_for_testing_)
    return;

  if (presenting) {
    compositor_->CreateImmersiveOverlay(overlay_.BindNewPipeAndPassReceiver());
    StartWebXrTimeout();
  } else {
    StopWebXrTimeout();
  }
}

void VRBrowserRendererThreadWin::StartWebXrTimeout() {
  frame_timeout_running_ = true;
  overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                         draw_state_.ShouldDrawWebXR());

  if (!waiting_for_webxr_frame_) {
    waiting_for_webxr_frame_ = true;
    overlay_->RequestNotificationOnWebXrSubmitted(base::BindOnce(
        &VRBrowserRendererThreadWin::OnWebXRSubmitted, base::Unretained(this)));
  }

  webxr_spinner_timeout_closure_.Reset(base::BindOnce(
      &VRBrowserRendererThreadWin::OnWebXrTimeoutImminent,
      base::Unretained(
          this)));  // Unretained safe because we explicitly cancel.
  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_spinner_timeout_closure_.callback(),
                                kWebVrSpinnerTimeout);
  webxr_frame_timeout_closure_.Reset(base::BindOnce(
      &VRBrowserRendererThreadWin::OnWebXrTimedOut,
      base::Unretained(
          this)));  // Unretained safe because we explicitly cancel.
  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_frame_timeout_closure_.callback(),
                                kWebVrInitialFrameTimeout);
}

void VRBrowserRendererThreadWin::StopWebXrTimeout() {
  if (!webxr_spinner_timeout_closure_.IsCancelled())
    webxr_spinner_timeout_closure_.Cancel();
  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();
  OnSpinnerVisibilityChanged(false);
  frame_timeout_running_ = false;
}

int VRBrowserRendererThreadWin::GetNextRequestId() {
  current_request_id_++;
  if (current_request_id_ >= 0x10000)
    current_request_id_ = 0;
  return current_request_id_;
}

void VRBrowserRendererThreadWin::OnWebXrTimeoutImminent() {
  OnSpinnerVisibilityChanged(true);
  scheduler_ui_->OnWebXrTimeoutImminent();
}

void VRBrowserRendererThreadWin::OnWebXrTimedOut() {
  OnSpinnerVisibilityChanged(true);
  scheduler_ui_->OnWebXrTimedOut();
}

void VRBrowserRendererThreadWin::UpdateOverlayState() {
  if (draw_state_.ShouldDrawUI())
    StartOverlay();

  if (overlay_)
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
  if (draw_state_.ShouldDrawUI()) {
    if (overlay_)  // False only while testing
      overlay_->RequestNextOverlayPose(
          base::BindOnce(&VRBrowserRendererThreadWin::OnPose,
                         base::Unretained(this), GetNextRequestId()));
  } else {
    StopOverlay();
  }
}

void VRBrowserRendererThreadWin::SetFramesThrottled(bool throttled) {
  if (frames_throttled_ == throttled)
    return;

  frames_throttled_ = throttled;

  if (g_frame_timeout_ui_disabled_for_testing_)
    return;

  // TODO(crbug.com/1014764): If we try to re-start the timeouts after UI has
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

void VRBrowserRendererThreadWin::SetVisibleExternalPromptNotification(
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

void VRBrowserRendererThreadWin::SetIndicatorsVisible(bool visible) {
  if (draw_state_.SetIndicatorsVisible(visible))
    UpdateOverlayState();
}

void VRBrowserRendererThreadWin::OnSpinnerVisibilityChanged(bool visible) {
  if (draw_state_.SetSpinnerVisible(visible))
    UpdateOverlayState();
}

void VRBrowserRendererThreadWin::SetCapturingState(
    const CapturingStateModel& active_capturing,
    const CapturingStateModel& background_capturing,
    const CapturingStateModel& potential_capturing) {
  if (ui_)
    ui_->SetCapturingState(active_capturing, background_capturing,
                           potential_capturing);
}

VRBrowserRendererThreadWin*
VRBrowserRendererThreadWin::GetInstanceForTesting() {
  return instance_for_testing_;
}

BrowserRenderer* VRBrowserRendererThreadWin::GetBrowserRendererForTesting() {
  return browser_renderer_.get();
}

namespace {
// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kSlidingAverageSize = 5;
}  // namespace

void VRBrowserRendererThreadWin::DisableFrameTimeoutForTesting() {
  g_frame_timeout_ui_disabled_for_testing_ = true;
}

class VRUiBrowserInterface : public UiBrowserInterface {
 public:
  ~VRUiBrowserInterface() override = default;

  void ExitPresent() override {}
};

void VRBrowserRendererThreadWin::StartOverlay() {
  if (started_)
    return;

  initializing_graphics_ = std::make_unique<GraphicsDelegateWin>();
  if (!initializing_graphics_->InitializeOnMainThread()) {
    return;
  }

  // We should have received valid views from the ui host before rendering.
  DCHECK(!default_views_.empty());
  initializing_graphics_->SetXrViews(default_views_);

  initializing_graphics_->InitializeOnGLThread();
  initializing_graphics_->BindContext();

  // Create a vr::Ui
  ui_browser_interface_ = std::make_unique<VRUiBrowserInterface>();
  UiInitialState ui_initial_state = {};
  std::unique_ptr<Ui> ui =
      std::make_unique<Ui>(ui_browser_interface_.get(), ui_initial_state);
  static_cast<UiInterface*>(ui.get())->OnGlInitialized();
  ui_ = static_cast<BrowserUiInterface*>(ui.get());
  scheduler_ui_ = static_cast<UiInterface*>(ui.get())->GetSchedulerUiPtr();

  // Create the delegates, and keep raw pointers to them.  They are owned by
  // browser_renderer_.
  std::unique_ptr<SchedulerDelegateWin> scheduler_delegate =
      std::make_unique<SchedulerDelegateWin>();
  scheduler_ = scheduler_delegate.get();
  graphics_ = initializing_graphics_.get();
  std::unique_ptr<InputDelegateWin> input_delegate =
      std::make_unique<InputDelegateWin>();
  input_ = input_delegate.get();

  // Create the BrowserRenderer to drive UI rendering based on the delegates.
  browser_renderer_ = std::make_unique<BrowserRenderer>(
      std::move(ui), std::move(scheduler_delegate),
      std::move(initializing_graphics_), std::move(input_delegate),
      nullptr /*browser_renderer_interface*/, kSlidingAverageSize);

  graphics_->ClearContext();

  started_ = true;
}

void VRBrowserRendererThreadWin::OnWebXRSubmitted() {
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
        ret->mojo_from_viewer->position = absl::nullopt;
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

void VRBrowserRendererThreadWin::OnPose(int request_id,
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

  input_->OnPose(head_from_world);

  // base::Unretained is safe because scheduler_ will be destroyed without
  // calling the callback if we are destroyed.
  scheduler_->OnPose(base::BindOnce(&VRBrowserRendererThreadWin::SubmitFrame,
                                    base::Unretained(this), data->frame_id),
                     head_from_world, draw_state_.ShouldDrawWebXR(),
                     draw_state_.ShouldDrawUI());
}

bool VRBrowserRendererThreadWin::PreRender() {
  // GraphicsDelegateWin::PreRender can fail if the context has become lost
  // due to hybrid adapter switching. Giving up on life means no overlays are
  // submitted to the XR process, causing it hang, waiting forever. Instead,
  // we shutdown and restart the overlay system, re-establishing the GPU process
  // connection and all of the graphics related state in vr::Ui.
  if (!graphics_->PreRender()) {
    StopOverlay();
    StartOverlay();
    return graphics_->PreRender();
  }
  return true;
}

void VRBrowserRendererThreadWin::SubmitFrame(int16_t frame_id) {
  graphics_->PostRender();

  overlay_->SubmitOverlayTexture(
      frame_id, graphics_->GetTexture(), graphics_->GetSyncToken(),
      graphics_->GetLeft(), graphics_->GetRight(),
      base::BindOnce(&VRBrowserRendererThreadWin::SubmitResult,
                     base::Unretained(this)));
}

void VRBrowserRendererThreadWin::SubmitResult(bool success) {
  if (!success && graphics_) {
    graphics_->ResetMemoryBuffer();
  }

  // Make sure that we only notify that a WebXr frame is now
  if (scheduler_ui_ && success && !frame_timeout_running_) {
    scheduler_ui_->OnWebXrFrameAvailable();
  }

  if (draw_state_.ShouldDrawUI() && started_) {
    overlay_->RequestNextOverlayPose(
        base::BindOnce(&VRBrowserRendererThreadWin::OnPose,
                       base::Unretained(this), GetNextRequestId()));
  }
}

// VRBrowserRendererThreadWin::DrawContentType functions.
bool VRBrowserRendererThreadWin::DrawState::ShouldDrawUI() {
  return prompt_ != ExternalPromptNotificationType::kPromptNone ||
         spinner_visible_ || indicators_visible_;
}

bool VRBrowserRendererThreadWin::DrawState::ShouldDrawWebXR() {
  return ((prompt_ == ExternalPromptNotificationType::kPromptNone ||
           indicators_visible_) &&
          !spinner_visible_);
}

bool VRBrowserRendererThreadWin::DrawState::SetPrompt(
    ExternalPromptNotificationType prompt) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  prompt_ = prompt;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

bool VRBrowserRendererThreadWin::DrawState::SetSpinnerVisible(bool visible) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  spinner_visible_ = visible;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

bool VRBrowserRendererThreadWin::DrawState::SetIndicatorsVisible(bool visible) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  indicators_visible_ = visible;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

}  // namespace vr
