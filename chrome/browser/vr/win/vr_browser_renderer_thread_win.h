// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
#define CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/model/web_vr_model.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace vr {

class InputDelegateWin;
class GraphicsDelegateWin;
class SchedulerDelegateWin;
class VRUiBrowserInterface;
class SchedulerUiInterface;

class VR_EXPORT VRBrowserRendererThreadWin {
 public:
  explicit VRBrowserRendererThreadWin(
      device::mojom::XRCompositorHost* compositor);
  ~VRBrowserRendererThreadWin();

  void SetDefaultXrViews(const std::vector<device::mojom::XRViewPtr>& views);
  void SetWebXrPresenting(bool presenting);
  void SetFramesThrottled(bool throttled);

  // The below function(s) affect(s) whether UI is drawn or not.
  void SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType prompt);
  void SetIndicatorsVisible(bool visible);
  void SetCapturingState(const CapturingStateModel& active_capturing,
                         const CapturingStateModel& background_capturing,
                         const CapturingStateModel& potential_capturing);

  static VRBrowserRendererThreadWin* GetInstanceForTesting();
  BrowserRenderer* GetBrowserRendererForTesting();
  static void DisableFrameTimeoutForTesting();

 private:
  class DrawState {
   public:
    // State changing methods.
    bool SetPrompt(ExternalPromptNotificationType prompt);
    bool SetSpinnerVisible(bool visible);
    bool SetIndicatorsVisible(bool visible);

    // State querying methods.
    bool ShouldDrawUI();
    bool ShouldDrawWebXR();

   private:
    ExternalPromptNotificationType prompt_ =
        ExternalPromptNotificationType::kPromptNone;

    bool spinner_visible_ = false;
    bool indicators_visible_ = false;
  };

  void OnPose(int request_id, device::mojom::XRRenderInfoPtr data);
  bool PreRender();
  void SubmitResult(bool success);
  void SubmitFrame(int16_t frame_id);
  void StartOverlay();
  void StopOverlay();
  void OnWebXRSubmitted();
  void OnSpinnerVisibilityChanged(bool visible);
  void OnWebXrTimeoutImminent();
  void OnWebXrTimedOut();
  void StartWebXrTimeout();
  void StopWebXrTimeout();
  int GetNextRequestId();

  void UpdateOverlayState();

  std::unique_ptr<VRUiBrowserInterface> ui_browser_interface_;
  std::unique_ptr<BrowserRenderer> browser_renderer_;
  std::unique_ptr<SchedulerDelegateWin> scheduler_delegate_win_;

  // Raw pointers to objects owned by browser_renderer_:
  raw_ptr<InputDelegateWin, DanglingUntriaged> input_ = nullptr;
  raw_ptr<GraphicsDelegateWin, DanglingUntriaged> graphics_ = nullptr;
  raw_ptr<SchedulerDelegateWin, DanglingUntriaged> scheduler_ = nullptr;
  raw_ptr<BrowserUiInterface, DanglingUntriaged> ui_ = nullptr;
  raw_ptr<SchedulerUiInterface, DanglingUntriaged> scheduler_ui_ = nullptr;

  // Owned by vr_ui_host:
  raw_ptr<device::mojom::XRCompositorHost> compositor_;

  DrawState draw_state_;
  bool started_ = false;
  bool webxr_presenting_ = false;
  bool frame_timeout_running_ = true;
  bool waiting_for_webxr_frame_ = false;
  bool frames_throttled_ = false;
  int current_request_id_ = 0;
  std::vector<device::mojom::XRViewPtr> default_views_;

  mojo::Remote<device::mojom::ImmersiveOverlay> overlay_;

  base::CancelableOnceClosure webxr_frame_timeout_closure_;
  base::CancelableOnceClosure webxr_spinner_timeout_closure_;

  // This class is effectively a singleton, although it's not actually
  // implemented as one. Since tests need to access the thread to post tasks,
  // just keep a static reference to the existing instance.
  static VRBrowserRendererThreadWin* instance_for_testing_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
