// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_host/vr_ui_host_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "device/base/features.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace vr {

namespace {
static constexpr base::TimeDelta kPermissionPromptTimeout = base::Seconds(5);

#if BUILDFLAG(IS_WIN)
// Some runtimes on Windows have quite lengthy lengthy startup animations that
// may cause indicators/permissions to not be visible during the normal timeout.
static constexpr base::TimeDelta kFirstWindowsPermissionPromptTimeout =
    base::Seconds(10);
#endif

base::TimeDelta GetPermissionPromptTimeout(bool first_time) {
#if BUILDFLAG(IS_WIN)
  if (first_time)
    return kFirstWindowsPermissionPromptTimeout;
#endif
  return kPermissionPromptTimeout;
}

static constexpr base::TimeDelta kPollCapturingStateInterval =
    base::Seconds(0.2);

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
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
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
  if (have_webxr_web_contents_) {
    WebXRWebContentsChanged(nullptr);
  }
}

void VRUiHostImpl::WebXRWebContentsChanged(content::WebContents* contents) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__ << ": web_contents_.get()=" << web_contents_.get()
           << " contents=" << contents
           << " have_webxr_web_contents_=" << have_webxr_web_contents_;

  // Note that we can't just check for equality here, because in the destruction
  // case we may be *supposed* to have a WebContents that needs to be cleaned
  // up, but it's already been nulled out. So we need to use the
  // `have_webxr_web_contents_` as a proxy for "should web_contents_ actually be
  // null".
  const bool has_new_webxr_web_contents = !!contents;
  if (have_webxr_web_contents_ == has_new_webxr_web_contents &&
      web_contents_.get() == contents) {
    // Nothing to do. This includes the case where both the old and new contents
    // are null.
    return;
  }

  // Eventually the contents will be used to poll for permissions, or determine
  // what overlays should show.

  if (have_webxr_web_contents_) {
    // If the WebContents change, make sure we unregister pre-existing
    // observers, if any. It's safe to try to remove a nonexistent observer.

    DesktopMediaPickerManager::Get()->RemoveObserver(this);
    if (!contents) {
      poll_capturing_state_task_.Cancel();

      if (ui_rendering_thread_) {
        ui_rendering_thread_->SetWebXrPresenting(false);
      }
      StopUiRendering();
    }

    // Even though we think we should have a WebContents, we only hold onto it
    // as a WeakPtr, so it may have been destroyed. So check here before we do
    // any cleanup.
    if (web_contents_) {
      VrTabHelper::SetIsContentDisplayedInHeadset(web_contents_.get(), false);

      // Don't save the permission request manager for future use to avoid a
      // race condition when destroying the WebContents, see
      // https://crbug.com/1203146
      raw_ptr<permissions::PermissionRequestManager> old_manager =
          permissions::PermissionRequestManager::FromWebContents(
              web_contents_.get());
      if (old_manager) {
        old_manager->RemoveObserver(this);
      }
    }
  }

  have_webxr_web_contents_ = has_new_webxr_web_contents;
  web_contents_ = contents ? contents->GetWeakPtr() : nullptr;

  if (contents) {
    DesktopMediaPickerManager::Get()->AddObserver(this);

    VrTabHelper::SetIsContentDisplayedInHeadset(contents, true);

    StartUiRendering();
    InitCapturingStates();
    ui_rendering_thread_->SetWebXrPresenting(true);
    ui_rendering_thread_->SetFramesThrottled(frames_throttled_);

    PollCapturingState();

    permissions::PermissionRequestManager::CreateForWebContents(contents);
    raw_ptr<permissions::PermissionRequestManager> permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(contents);
    // Attaching a permission request manager to WebContents can fail, so a
    // DCHECK would be inappropriate here. If it fails, the user won't get
    // notified about permission prompts, but other than that the session would
    // work normally.
    if (permission_request_manager) {
      permission_request_manager->AddObserver(this);

      // There might already be a visible permission bubble from before
      // we registered the observer, show the HMD message now in that case.
      if (permission_request_manager->IsRequestInProgress()) {
        OnPromptAdded();
      }
    }
  }
}

void VRUiHostImpl::SetDefaultXrViews(
    const std::vector<device::mojom::XRViewPtr>& views) {
  if (!base::Contains(views, device::mojom::XREye::kLeft,
                      &device::mojom::XRView::eye) ||
      !base::Contains(views, device::mojom::XREye::kRight,
                      &device::mojom::XRView::eye)) {
    // The graphics delegate requires the left and right views to render.
    content::XRRuntimeManager::ExitImmersivePresentation();
    return;
  }

  if (ui_rendering_thread_) {
    ui_rendering_thread_->SetDefaultXrViews(views);
  }

  for (auto& view : views) {
    if (view->eye == device::mojom::XREye::kLeft ||
        view->eye == device::mojom::XREye::kRight) {
      default_views_.push_back(view.Clone());
    }
  }
}

void VRUiHostImpl::WebXRFramesThrottledChanged(bool throttled) {
  frames_throttled_ = throttled;

  if (!ui_rendering_thread_) {
    DVLOG(1) << __func__ << ": no ui_rendering_thread_";
    return;
  }

  ui_rendering_thread_->SetFramesThrottled(frames_throttled_);
}

void VRUiHostImpl::StartUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  ui_rendering_thread_ =
      std::make_unique<VRBrowserRendererThreadWin>(compositor_.get());

  // We should have received default views from the browser before rendering
  DCHECK(!default_views_.empty());
  ui_rendering_thread_->SetDefaultXrViews(default_views_);
}

void VRUiHostImpl::StopUiRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;

  ui_rendering_thread_ = nullptr;
}

void VRUiHostImpl::OnPromptAdded() {
  ShowExternalNotificationPrompt();
}

void VRUiHostImpl::OnPromptRemoved() {
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

  if (indicators_visible_) {
    indicators_visible_ = false;
    ui_rendering_thread_->SetIndicatorsVisible(false);
  }

  ui_rendering_thread_->SetVisibleExternalPromptNotification(
      ExternalPromptNotificationType::kPromptGenericPermission);

  is_external_prompt_showing_in_headset_ = true;
  external_prompt_timeout_task_.Reset(
      base::BindOnce(&VRUiHostImpl::RemoveHeadsetNotificationPrompt,
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

  CHECK(web_contents_);
  content::PermissionController* permission_controller =
      web_contents_->GetBrowserContext()->GetPermissionController();
  potential_capturing_.audio_capture_enabled =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::AUDIO_CAPTURE,
          web_contents_->GetPrimaryMainFrame()) ==
      blink::mojom::PermissionStatus::GRANTED;
  potential_capturing_.video_capture_enabled =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::VIDEO_CAPTURE,
          web_contents_->GetPrimaryMainFrame()) ==
      blink::mojom::PermissionStatus::GRANTED;
  potential_capturing_.location_access_enabled =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::GEOLOCATION,
          web_contents_->GetPrimaryMainFrame()) ==
      blink::mojom::PermissionStatus::GRANTED;
  potential_capturing_.midi_connected =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::MIDI_SYSEX,
          web_contents_->GetPrimaryMainFrame()) ==
      blink::mojom::PermissionStatus::GRANTED;

  indicators_shown_start_time_ = base::Time::Now();
  indicators_visible_ = false;
  indicators_showing_first_time_ = true;
  triggered_capturing_transience_.ResetStartTimes();
}

void VRUiHostImpl::PollCapturingState() {
  poll_capturing_state_task_.Reset(base::BindOnce(
      &VRUiHostImpl::PollCapturingState, base::Unretained(this)));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE, poll_capturing_state_task_.callback(),
      kPollCapturingStateInterval);

  // location, microphone, camera, midi.
  CapturingStateModel active_capturing = active_capturing_;
  // TODO(https://crbug.com/1103176): Plumb the actual frame reference here (we
  // should get a RFH from VRServiceImpl instead of WebContents)
  if (web_contents_) {
    content_settings::PageSpecificContentSettings* settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents_->GetPrimaryMainFrame());

    if (settings) {
      active_capturing.location_access_enabled =
          settings->IsContentAllowed(ContentSettingsType::GEOLOCATION);

      active_capturing.audio_capture_enabled =
          settings->GetMicrophoneCameraState().Has(
              content_settings::PageSpecificContentSettings::
                  kMicrophoneAccessed) &&
          !settings->GetMicrophoneCameraState().Has(
              content_settings::PageSpecificContentSettings::
                  kMicrophoneBlocked);

      active_capturing.video_capture_enabled =
          settings->GetMicrophoneCameraState().Has(
              content_settings::PageSpecificContentSettings::kCameraAccessed) &&
          !settings->GetMicrophoneCameraState().Has(
              content_settings::PageSpecificContentSettings::kCameraBlocked);

      active_capturing.midi_connected =
          settings->IsContentAllowed(ContentSettingsType::MIDI_SYSEX);
    }

    // Screen capture.
    scoped_refptr<MediaStreamCaptureIndicator> indicator =
        MediaCaptureDevicesDispatcher::GetInstance()
            ->GetMediaStreamCaptureIndicator();
    active_capturing.screen_capture_enabled =
        indicator->IsBeingMirrored(web_contents_.get()) ||
        indicator->IsCapturingWindow(web_contents_.get()) ||
        indicator->IsCapturingDisplay(web_contents_.get());

    // Bluetooth.
    active_capturing.bluetooth_connected =
        web_contents_->IsConnectedToBluetoothDevice();

    // USB.
    active_capturing.usb_connected = web_contents_->IsConnectedToUsbDevice();
  }

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
