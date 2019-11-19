// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_HOST_VR_UI_HOST_IMPL_H_
#define CHROME_BROWSER_VR_UI_HOST_VR_UI_HOST_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/vr_ui_host.h"
#include "components/bubble/bubble_manager.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"

namespace vr {

class VRBrowserRendererThreadWin;

// Concrete implementation of VRBrowserRendererHost, part of the "browser"
// component. Used on the browser's main thread.
class VRUiHostImpl : public VRUiHost,
                     public PermissionRequestManager::Observer,
                     public BrowserXRRuntimeObserver,
                     public BubbleManager::BubbleManagerObserver,
                     public DesktopMediaPickerManager::DialogObserver {
 public:
  VRUiHostImpl(device::mojom::XRDeviceId device_id,
               mojo::PendingRemote<device::mojom::XRCompositorHost> compositor);
  ~VRUiHostImpl() override;

  // Factory for use with VRUiHost::{Set,Get}Factory
  static std::unique_ptr<VRUiHost> Create(
      device::mojom::XRDeviceId device_id,
      mojo::PendingRemote<device::mojom::XRCompositorHost> compositor);

 private:
  // This class manages the transience of each of a CapturingStateModel's flags.
  class CapturingStateModelTransience {
   public:
    explicit CapturingStateModelTransience(CapturingStateModel* model);

    void ResetStartTimes();

    // Turns the flags in |model| on immediately, based on the given
    // triggered_state.
    void TurnFlagsOnBasedOnTriggeredState(
        const CapturingStateModel& triggered_state);

    // Any on flags stay on until every one of those flags has been on for
    // longer than |period|.
    void TurnOffAllFlagsTogetherWhenAllTransiencesExpire(
        const base::TimeDelta& period);

   private:
    base::Time audio_indicator_start_;
    base::Time video_indicator_start_;
    base::Time screen_capture_indicator_start_;
    base::Time location_indicator_start_;
    base::Time bluetooth_indicator_start_;
    base::Time usb_indicator_start_;
    base::Time midi_indicator_start_;
    CapturingStateModel* active_capture_state_model_;  // Not owned.
  };

  // BrowserXRRuntimeObserver implementation.
  void SetWebXRWebContents(content::WebContents* contents) override;
  void SetVRDisplayInfo(device::mojom::VRDisplayInfoPtr display_info) override;
  void SetFramesThrottled(bool throttled) override;

  // Internal methods used to start/stop the UI rendering thread that is used
  // for drawing browser UI (such as permission prompts) for display in VR.
  void StartUiRendering();
  void StopUiRendering();

  // PermissionRequestManager::Observer
  void OnBubbleAdded() override;
  void OnBubbleRemoved() override;

  // content::BubbleManager::BubbleManagerObserver
  void OnBubbleNeverShown(BubbleReference bubble) override;
  void OnBubbleClosed(BubbleReference bubble,
                      BubbleCloseReason reason) override;
  void OnBubbleShown(BubbleReference bubble) override;

  // DesktopMediaPickerManager::DialogObserver
  // These are dialogs displayed in response to getDisplayMedia()
  void OnDialogOpened() override;
  void OnDialogClosed() override;

  void ShowExternalNotificationPrompt();
  void RemoveHeadsetNotificationPrompt();
  void SetLocationInfoOnUi();

  void InitCapturingStates();
  void PollCapturingState();

  mojo::Remote<device::mojom::XRCompositorHost> compositor_;
  std::unique_ptr<VRBrowserRendererThreadWin> ui_rendering_thread_;
  device::mojom::VRDisplayInfoPtr info_;
  content::WebContents* web_contents_ = nullptr;
  PermissionRequestManager* permission_request_manager_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::CancelableClosure external_prompt_timeout_task_;
  bool is_external_prompt_showing_in_headset_ = false;

  CapturingStateModel active_capturing_;
  CapturingStateModel potential_capturing_;
  // Keeps track of the state flags that were set to true between
  // consecutive polls of active_capturing_ above.
  CapturingStateModel triggered_capturing_state_model_;
  CapturingStateModelTransience triggered_capturing_transience_;
  base::Time indicators_shown_start_time_;
  bool indicators_visible_ = false;
  bool indicators_showing_first_time_ = true;
  bool frames_throttled_ = false;

  mojo::Remote<device::mojom::GeolocationConfig> geolocation_config_;
  base::CancelableClosure poll_capturing_state_task_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VRUiHostImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VRUiHostImpl);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_HOST_VR_UI_HOST_IMPL_H_
