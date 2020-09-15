// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"

#include <memory>

#include "base/task/post_task.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "device/base/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

namespace vr {

namespace {
static constexpr base::TimeDelta kPermissionPromptTimeout =
    base::TimeDelta::FromSeconds(5);

base::TimeDelta GetPermissionPromptTimeout(bool first_time) {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (base::FeatureList::IsEnabled(device::features::kWindowsMixedReality) &&
      first_time)
    return base::TimeDelta::FromSeconds(10);
#endif
  return kPermissionPromptTimeout;
}

static constexpr base::TimeDelta kPollCapturingStateInterval =
    base::TimeDelta::FromSecondsD(0.2);

const CapturingStateModel g_default_capturing_state;
}  // namespace

VRUiHostImpl::CapturingStateModelTransience::CapturingStateModelTransience(
    CapturingStateModel* capturing_model)
    : active_capture_state_model_(capturing_model) {}

void VRUiHostImpl::CapturingStateModelTransience::ResetStartTimes() {
  auto now = base::Time::Now();
  midi_indicator_start_ = now;
  usb_indicator_start_ = now;
  bluetooth_indicator_start_ = now;
  location_indicator_start_ = now;
  screen_capture_indicator_start_ = now;
  video_indicator_start_ = now;
  audio_indicator_start_ = now;
}

void VRUiHostImpl::CapturingStateModelTransience::
    TurnFlagsOnBasedOnTriggeredState(
        const CapturingStateModel& model_with_triggered_states) {
  auto now = base::Time::Now();
  if (model_with_triggered_states.audio_capture_enabled) {
    audio_indicator_start_ = now;
    active_capture_state_model_->audio_capture_enabled = true;
  }
  if (model_with_triggered_states.video_capture_enabled) {
    video_indicator_start_ = now;
    active_capture_state_model_->video_capture_enabled = true;
  }
  if (model_with_triggered_states.screen_capture_enabled) {
    screen_capture_indicator_start_ = now;
    active_capture_state_model_->screen_capture_enabled = true;
  }
  if (model_with_triggered_states.location_access_enabled) {
    location_indicator_start_ = now;
    active_capture_state_model_->location_access_enabled = true;
  }
  if (model_with_triggered_states.bluetooth_connected) {
    bluetooth_indicator_start_ = now;
    active_capture_state_model_->bluetooth_connected = true;
  }
  if (model_with_triggered_states.usb_connected) {
    usb_indicator_start_ = now;
    active_capture_state_model_->usb_connected = true;
  }
  if (model_with_triggered_states.midi_connected) {
    midi_indicator_start_ = now;
    active_capture_state_model_->midi_connected = true;
  }
}

void VRUiHostImpl::CapturingStateModelTransience::
    TurnOffAllFlagsTogetherWhenAllTransiencesExpire(
        const base::TimeDelta& transience_period) {
  if (!active_capture_state_model_->IsAtleastOnePermissionGrantedOrInUse())
    return;
  auto now = base::Time::Now();
  if ((!active_capture_state_model_->audio_capture_enabled ||
       now > audio_indicator_start_ + transience_period) &&
      (!active_capture_state_model_->video_capture_enabled ||
       now > video_indicator_start_ + transience_period) &&
      (!active_capture_state_model_->screen_capture_enabled ||
       now > screen_capture_indicator_start_ + transience_period) &&
      (!active_capture_state_model_->location_access_enabled ||
       now > location_indicator_start_ + transience_period) &&
      (!active_capture_state_model_->bluetooth_connected ||
       now > bluetooth_indicator_start_ + transience_period) &&
      (!active_capture_state_model_->usb_connected ||
       now > usb_indicator_start_ + transience_period) &&
      (!active_capture_state_model_->midi_connected ||
       now > midi_indicator_start_ + transience_period))
    *active_capture_state_model_ = CapturingStateModel();
}

VRUiHostImpl::VRUiHostImpl(
    device::mojom::XRDeviceId device_id,
    mojo::PendingRemote<device::mojom::XRCompositorHost> compositor)
    : compositor_(std::move(compositor)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      triggered_capturing_transience_(&triggered_capturing_state_model_) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  auto* runtime_manager = content::XRRuntimeManager::GetInstanceIfCreated();
  DCHECK(runtime_manager != nullptr);
  content::BrowserXRRuntime* runtime = runtime_manager->GetRuntime(device_id);
  if (runtime) {
    runtime->AddObserver(this);
  }

  content::GetDeviceService().BindGeolocationConfig(
      geolocation_config_.BindNewPipeAndPassReceiver());
}

VRUiHostImpl::~VRUiHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  // We don't call BrowserXRRuntime::RemoveObserver, because if we are being
  // destroyed, it means the corresponding device has been removed from
  // XRRuntimeManager, and the BrowserXRRuntime has been destroyed.
  if (web_contents_)
    SetWebXRWebContents(nullptr);
}

bool IsValidInfo(device::mojom::VRDisplayInfoPtr& info) {
  // Numeric properties are validated elsewhere, but we expect a stereo headset.
  if (!info)
    return false;
  if (!info->left_eye)
    return false;
  if (!info->right_eye)
    return false;
  return true;
}

void VRUiHostImpl::SetWebXRWebContents(content::WebContents* contents) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsValidInfo(info_)) {
    content::XRRuntimeManager::ExitImmersivePresentation();
    return;
  }

  // Eventually the contents will be used to poll for permissions, or determine
  // what overlays should show.

  // permission_request_manager_ is an unowned pointer; it's owned by
  // WebContents. If the WebContents change, make sure we unregister any
  // pre-existing observers. We only have a non-null permission_request_manager_
  // if we successfully added an observer.
  if (permission_request_manager_) {
    permission_request_manager_->RemoveObserver(this);
    permission_request_manager_ = nullptr;
  }

  if (web_contents_ != contents) {
    if (web_contents_) {
      DesktopMediaPickerManager::Get()->RemoveObserver(this);
    }
    if (contents) {
      DesktopMediaPickerManager::Get()->AddObserver(this);
    }
  }

  if (web_contents_)
    VrTabHelper::SetIsContentDisplayedInHeadset(web_contents_, false);
  if (contents)
    VrTabHelper::SetIsContentDisplayedInHeadset(contents, true);

  web_contents_ = contents;
  if (contents) {
    StartUiRendering();
    InitCapturingStates();
    ui_rendering_thread_->SetWebXrPresenting(true);
    ui_rendering_thread_->SetFramesThrottled(frames_throttled_);

    PollCapturingState();

    permissions::PermissionRequestManager::CreateForWebContents(contents);
    permission_request_manager_ =
        permissions::PermissionRequestManager::FromWebContents(contents);
    // Attaching a permission request manager to WebContents can fail, so a
    // DCHECK would be inappropriate here. If it fails, the user won't get
    // notified about permission prompts, but other than that the session would
    // work normally.
    if (permission_request_manager_) {
      permission_request_manager_->AddObserver(this);

      // There might already be a visible permission bubble from before
      // we registered the observer, show the HMD message now in that case.
      if (permission_request_manager_->IsRequestInProgress())
        OnBubbleAdded();
    } else {
      DVLOG(1) << __func__ << ": No PermissionRequestManager";
    }
  } else {
    poll_capturing_state_task_.Cancel();

    if (ui_rendering_thread_)
      ui_rendering_thread_->SetWebXrPresenting(false);
    StopUiRendering();
  }
}

void VRUiHostImpl::SetFramesThrottled(bool throttled) {
  frames_throttled_ = throttled;

  if (!ui_rendering_thread_) {
    DVLOG(1) << __func__ << ": no ui_rendering_thread_";
    return;
  }

  ui_rendering_thread_->SetFramesThrottled(frames_throttled_);
}

void VRUiHostImpl::SetVRDisplayInfo(
    device::mojom::VRDisplayInfoPtr display_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // On Windows this is getting logged every frame, so set to 3.
  DVLOG(3) << __func__;

  if (!IsValidInfo(display_info)) {
    content::XRRuntimeManager::ExitImmersivePresentation();
    return;
  }

  info_ = std::move(display_info);
  if (ui_rendering_thread_) {
    ui_rendering_thread_->SetVRDisplayInfo(info_.Clone());
  }
}

void VRUiHostImpl::StartUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  DCHECK(info_);
  ui_rendering_thread_ =
      std::make_unique<VRBrowserRendererThreadWin>(compositor_.get());
  ui_rendering_thread_->SetVRDisplayInfo(info_.Clone());
}

void VRUiHostImpl::StopUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  ui_rendering_thread_ = nullptr;
}

void VRUiHostImpl::SetLocationInfoOnUi() {
  GURL gurl;
  if (web_contents_) {
    content::NavigationEntry* entry =
        web_contents_->GetController().GetVisibleEntry();
    if (entry) {
      gurl = entry->GetVirtualURL();
    }
  }
  // TODO(https://crbug.com/905375): The below call should eventually be
  // rewritten to take a LocationBarState and not just GURL. See
  // VRBrowserRendererThreadWin::StartOverlay() also.
  ui_rendering_thread_->SetLocationInfo(gurl);
}

void VRUiHostImpl::OnBubbleAdded() {
  ShowExternalNotificationPrompt();
}

void VRUiHostImpl::OnBubbleRemoved() {
  RemoveHeadsetNotificationPrompt();
}

void VRUiHostImpl::OnDialogOpened() {
  ShowExternalNotificationPrompt();
}

void VRUiHostImpl::OnDialogClosed() {
  RemoveHeadsetNotificationPrompt();
}

void VRUiHostImpl::ShowExternalNotificationPrompt() {
  if (!ui_rendering_thread_) {
    DVLOG(1) << __func__ << ": no ui_rendering_thread_";
    return;
  }

  SetLocationInfoOnUi();

  if (indicators_visible_) {
    indicators_visible_ = false;
    ui_rendering_thread_->SetIndicatorsVisible(false);
  }

  ui_rendering_thread_->SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType::kPromptGenericPermission);

  is_external_prompt_showing_in_headset_ = true;
  external_prompt_timeout_task_.Reset(
      base::BindRepeating(&VRUiHostImpl::RemoveHeadsetNotificationPrompt,
                          weak_ptr_factory_.GetWeakPtr()));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE, external_prompt_timeout_task_.callback(),
      kPermissionPromptTimeout);
}

void VRUiHostImpl::RemoveHeadsetNotificationPrompt() {
  if (!external_prompt_timeout_task_.IsCancelled())
    external_prompt_timeout_task_.Cancel();

  if (!is_external_prompt_showing_in_headset_)
    return;

  is_external_prompt_showing_in_headset_ = false;
  ui_rendering_thread_->SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType::kPromptNone);
  indicators_shown_start_time_ = base::Time::Now();
}

void VRUiHostImpl::InitCapturingStates() {
  active_capturing_ = g_default_capturing_state;
  potential_capturing_ = g_default_capturing_state;

  DCHECK(web_contents_);
  permissions::PermissionManager* permission_manager =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  const GURL& origin = web_contents_->GetLastCommittedURL();
  content::RenderFrameHost* rfh = web_contents_->GetMainFrame();
  potential_capturing_.audio_capture_enabled =
      permission_manager
          ->GetPermissionStatusForFrame(ContentSettingsType::MEDIASTREAM_MIC,
                                        rfh, origin)
          .content_setting == CONTENT_SETTING_ALLOW;
  potential_capturing_.video_capture_enabled =
      permission_manager
          ->GetPermissionStatusForFrame(ContentSettingsType::MEDIASTREAM_CAMERA,
                                        rfh, origin)
          .content_setting == CONTENT_SETTING_ALLOW;
  potential_capturing_.location_access_enabled =
      permission_manager
          ->GetPermissionStatusForFrame(ContentSettingsType::GEOLOCATION, rfh,
                                        origin)
          .content_setting == CONTENT_SETTING_ALLOW;
  potential_capturing_.midi_connected =
      permission_manager
          ->GetPermissionStatusForFrame(ContentSettingsType::MIDI_SYSEX, rfh,
                                        origin)
          .content_setting == CONTENT_SETTING_ALLOW;

  indicators_shown_start_time_ = base::Time::Now();
  indicators_visible_ = false;
  indicators_showing_first_time_ = true;
  triggered_capturing_transience_.ResetStartTimes();
}

void VRUiHostImpl::PollCapturingState() {
  poll_capturing_state_task_.Reset(base::BindRepeating(
      &VRUiHostImpl::PollCapturingState, base::Unretained(this)));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE, poll_capturing_state_task_.callback(),
      kPollCapturingStateInterval);

  // location, microphone, camera, midi.
  CapturingStateModel active_capturing = active_capturing_;
  // TODO(https://crbug.com/1103176): Plumb the actual frame reference here (we
  // should get a RFH from VRServiceImpl instead of WebContents)
  content_settings::PageSpecificContentSettings* settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents_->GetMainFrame());

  if (settings) {
    active_capturing.location_access_enabled =
        settings->IsContentAllowed(ContentSettingsType::GEOLOCATION);

    active_capturing.audio_capture_enabled =
        (settings->GetMicrophoneCameraState() &
         content_settings::PageSpecificContentSettings::MICROPHONE_ACCESSED) &&
        !(settings->GetMicrophoneCameraState() &
          content_settings::PageSpecificContentSettings::MICROPHONE_BLOCKED);

    active_capturing.video_capture_enabled =
        (settings->GetMicrophoneCameraState() &
         content_settings::PageSpecificContentSettings::CAMERA_ACCESSED) &
        !(settings->GetMicrophoneCameraState() &
          content_settings::PageSpecificContentSettings::CAMERA_BLOCKED);

    active_capturing.midi_connected =
        settings->IsContentAllowed(ContentSettingsType::MIDI_SYSEX);
  }

  // Screen capture.
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();
  active_capturing.screen_capture_enabled =
      indicator->IsBeingMirrored(web_contents_) ||
      indicator->IsCapturingWindow(web_contents_) ||
      indicator->IsCapturingDisplay(web_contents_);

  // Bluetooth.
  active_capturing.bluetooth_connected =
      web_contents_->IsConnectedToBluetoothDevice();

  // USB.
  UsbTabHelper* usb_tab_helper =
      UsbTabHelper::GetOrCreateForWebContents(web_contents_);
  DCHECK(usb_tab_helper != nullptr);
  active_capturing.usb_connected = usb_tab_helper->IsDeviceConnected();

  auto capturing_switched_on =
      active_capturing.NewlyUpdatedPermissions(active_capturing_);
  if (capturing_switched_on.IsAtleastOnePermissionGrantedOrInUse()) {
    indicators_shown_start_time_ = base::Time::Now();
    triggered_capturing_transience_.TurnFlagsOnBasedOnTriggeredState(
        capturing_switched_on);
    active_capturing_ = active_capturing;
  }
  triggered_capturing_transience_
      .TurnOffAllFlagsTogetherWhenAllTransiencesExpire(
          GetPermissionPromptTimeout(indicators_showing_first_time_));

  ui_rendering_thread_->SetCapturingState(triggered_capturing_state_model_,
                                          g_default_capturing_state,
                                          potential_capturing_);

  if (indicators_shown_start_time_ +
          GetPermissionPromptTimeout(indicators_showing_first_time_) >
      base::Time::Now()) {
    if (!indicators_visible_ && !is_external_prompt_showing_in_headset_) {
      indicators_visible_ = true;
      ui_rendering_thread_->SetIndicatorsVisible(true);
    }
  } else {
    indicators_showing_first_time_ = false;
    potential_capturing_ = CapturingStateModel();
    if (indicators_visible_) {
      indicators_visible_ = false;
      ui_rendering_thread_->SetIndicatorsVisible(false);
    }
  }
}

}  // namespace vr
