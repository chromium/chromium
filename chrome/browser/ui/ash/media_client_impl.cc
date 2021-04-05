// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_client_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/media_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using ash::MediaCaptureState;

namespace {

MediaClientImpl* g_media_client = nullptr;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CameraPrivacySwitchEvent {
  kSwitchOn = 0,
  kSwitchOff = 1,
  kSwitchOnNotificationShown = 2,
  kMaxValue = kSwitchOnNotificationShown
};

// The name for the histogram used to record camera privacy switch related
// events.
constexpr char kCameraPrivacySwitchEventsHistogramName[] =
    "Ash.Media.CameraPrivacySwitch.Event";

// The name for the histogram used to record delay (in seconds) after a
// notification about camera privacy switch being on before the user turns
// the camera privacy switch off.
constexpr char kCameraPrivacySwitchTimeToTurnOffHistogramName[] =
    "Ash.Media.CameraPrivacySwitch.TimeFromNotificationToOff";

// The max recorded value for `kCameraPrivacySwitchTimeToTurnOffHistogramName`.
constexpr int kMaxRecordedTimeInSeconds = 60;

// The granularity used for
// reporting`kCameraPrivacySwitchToTurnOffHistogramName`.
constexpr int kRecordedTimeGranularityInSeconds = 5;

// The ID for a notification shown when the user tries to use a camera while the
// camera privacy switch is on.
constexpr char kCameraPrivacySwitchOnNotificationId[] =
    "ash.media.camera.activity_with_privacy_switch_on";

// The notifier ID for a notification shown when the user tries to use a camera
// while the camera privacy switch is on.
constexpr char kCameraPrivacySwitchNotifierId[] = "ash.media.camera";

// The ID for the toast shown when the camera privacy switch is turned on.
constexpr char kCameraPrivacySwitchOnToastId[] =
    "ash.media.camera.privacy_switch_on";

// The ID for the toast shown when the camera privacy switch is turned off.
constexpr char kCameraPrivacySwitchOffToastId[] =
    "ash.media.camera.privacy_switch_off";

// The amount of time for which the camera privacy switch toasts will remain
// displayed.
constexpr int kCameraPrivacySwitchToastDurationMs = 6 * 1000;

MediaCaptureState& operator|=(MediaCaptureState& lhs, MediaCaptureState rhs) {
  lhs = static_cast<MediaCaptureState>(static_cast<int>(lhs) |
                                       static_cast<int>(rhs));
  return lhs;
}

void GetMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                          content::WebContents* web_contents,
                          MediaCaptureState* media_state_out) {
  bool video = indicator->IsCapturingVideo(web_contents);
  bool audio = indicator->IsCapturingAudio(web_contents);

  if (video)
    *media_state_out |= MediaCaptureState::kVideo;
  if (audio)
    *media_state_out |= MediaCaptureState::kAudio;
}

void GetBrowserMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                                 const content::BrowserContext* context,
                                 MediaCaptureState* media_state_out) {
  const BrowserList* desktop_list = BrowserList::GetInstance();

  for (BrowserList::BrowserVector::const_iterator iter = desktop_list->begin();
       iter != desktop_list->end(); ++iter) {
    TabStripModel* tab_strip_model = (*iter)->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents->GetBrowserContext() != context)
        continue;
      GetMediaCaptureState(indicator, web_contents, media_state_out);
      if (*media_state_out == MediaCaptureState::kAudioVideo)
        return;
    }
  }
}

void GetAppMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                             content::BrowserContext* context,
                             MediaCaptureState* media_state_out) {
  const extensions::AppWindowRegistry::AppWindowList& apps =
      extensions::AppWindowRegistry::Get(context)->app_windows();
  for (extensions::AppWindowRegistry::AppWindowList::const_iterator iter =
           apps.begin();
       iter != apps.end(); ++iter) {
    GetMediaCaptureState(indicator, (*iter)->web_contents(), media_state_out);
    if (*media_state_out == MediaCaptureState::kAudioVideo)
      return;
  }
}

void GetExtensionMediaCaptureState(const MediaStreamCaptureIndicator* indicator,
                                   content::BrowserContext* context,
                                   MediaCaptureState* media_state_out) {
  for (content::RenderFrameHost* host :
       extensions::ProcessManager::Get(context)->GetAllFrames()) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(host);
    // RFH may not have web contents.
    if (!web_contents)
      continue;
    GetMediaCaptureState(indicator, web_contents, media_state_out);
    if (*media_state_out == MediaCaptureState::kAudioVideo)
      return;
  }
}

MediaCaptureState GetMediaCaptureStateOfAllWebContents(
    content::BrowserContext* context) {
  if (!context)
    return MediaCaptureState::kNone;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  MediaCaptureState media_state = MediaCaptureState::kNone;
  // Browser windows
  GetBrowserMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == MediaCaptureState::kAudioVideo)
    return MediaCaptureState::kAudioVideo;

  // App windows
  GetAppMediaCaptureState(indicator.get(), context, &media_state);
  if (media_state == MediaCaptureState::kAudioVideo)
    return MediaCaptureState::kAudioVideo;

  // Extensions
  GetExtensionMediaCaptureState(indicator.get(), context, &media_state);

  return media_state;
}

}  // namespace

MediaClientImpl::MediaClientImpl() {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  BrowserList::AddObserver(this);

  ash::VmCameraMicManager::Get()->AddObserver(this);

  // Camera service does not behave in non ChromeOS environment (e.g. testing,
  // linux chromeos).
  if (base::SysInfo::IsRunningOnChromeOS() &&
      base::FeatureList::IsEnabled(
          chromeos::features::kCameraPrivacySwitchNotifications) &&
      media::ShouldUseCrosCameraService()) {
    camera_privacy_switch_state_ = media::CameraHalDispatcherImpl::GetInstance()
                                       ->AddCameraPrivacySwitchObserver(this);
    media::CameraHalDispatcherImpl::GetInstance()->AddActiveClientObserver(
        this);
  }

  DCHECK(!g_media_client);
  g_media_client = this;
}

MediaClientImpl::~MediaClientImpl() {
  g_media_client = nullptr;

  if (media_controller_ && ash::MediaController::Get() == media_controller_)
    media_controller_->SetClient(nullptr);

  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
  BrowserList::RemoveObserver(this);

  ash::VmCameraMicManager::Get()->RemoveObserver(this);
  if (base::SysInfo::IsRunningOnChromeOS() &&
      base::FeatureList::IsEnabled(
          chromeos::features::kCameraPrivacySwitchNotifications) &&
      media::ShouldUseCrosCameraService()) {
    media::CameraHalDispatcherImpl::GetInstance()
        ->RemoveCameraPrivacySwitchObserver(this);
    media::CameraHalDispatcherImpl::GetInstance()->RemoveActiveClientObserver(
        this);
  }
}

// static
MediaClientImpl* MediaClientImpl::Get() {
  return g_media_client;
}

void MediaClientImpl::Init() {
  DCHECK(!media_controller_);

  media_controller_ = ash::MediaController::Get();
  media_controller_->SetClient(this);
}

void MediaClientImpl::InitForTesting(ash::MediaController* controller) {
  DCHECK(!media_controller_);

  media_controller_ = controller;
  media_controller_->SetClient(this);
}

void MediaClientImpl::HandleMediaNextTrack() {
  HandleMediaAction(ui::VKEY_MEDIA_NEXT_TRACK);
}

void MediaClientImpl::HandleMediaPlayPause() {
  HandleMediaAction(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaClientImpl::HandleMediaPlay() {
  HandleMediaAction(ui::VKEY_MEDIA_PLAY);
}

void MediaClientImpl::HandleMediaPause() {
  HandleMediaAction(ui::VKEY_MEDIA_PAUSE);
}

void MediaClientImpl::HandleMediaStop() {
  HandleMediaAction(ui::VKEY_MEDIA_STOP);
}

void MediaClientImpl::HandleMediaPrevTrack() {
  HandleMediaAction(ui::VKEY_MEDIA_PREV_TRACK);
}

void MediaClientImpl::HandleMediaSeekBackward() {
  HandleMediaAction(ui::VKEY_OEM_103);
}

void MediaClientImpl::HandleMediaSeekForward() {
  HandleMediaAction(ui::VKEY_OEM_104);
}

void MediaClientImpl::RequestCaptureState() {
  base::flat_map<AccountId, MediaCaptureState> capture_states;
  auto* manager = user_manager::UserManager::Get();
  for (user_manager::User* user : manager->GetLRULoggedInUsers()) {
    capture_states[user->GetAccountId()] = GetMediaCaptureStateOfAllWebContents(
        chromeos::ProfileHelper::Get()->GetProfileByUser(user));
  }

  const user_manager::User* primary_user = manager->GetPrimaryUser();
  if (primary_user)
    capture_states[primary_user->GetAccountId()] |= vm_media_capture_state_;

  media_controller_->NotifyCaptureState(std::move(capture_states));
}

void MediaClientImpl::SuspendMediaSessions() {
  for (auto* web_contents : AllTabContentses()) {
    content::MediaSession::Get(web_contents)
        ->Suspend(content::MediaSession::SuspendType::kSystem);
  }
}

void MediaClientImpl::OnRequestUpdate(int render_process_id,
                                      int render_frame_id,
                                      blink::mojom::MediaStreamType stream_type,
                                      const content::MediaRequestState state) {
  DCHECK(base::CurrentUIThread::IsSet());
  // The PostTask is necessary because the state of MediaStreamCaptureIndicator
  // gets updated after this.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MediaClientImpl::RequestCaptureState,
                                weak_ptr_factory_.GetWeakPtr()));
}

void MediaClientImpl::OnBrowserSetLastActive(Browser* browser) {
  active_context_ = browser ? browser->profile() : nullptr;

  UpdateForceMediaClientKeyHandling();
}

void MediaClientImpl::OnVmCameraMicActiveChanged(
    ash::VmCameraMicManager* manager) {
  using DeviceType = ash::VmCameraMicManager::DeviceType;
  vm_media_capture_state_ = MediaCaptureState::kNone;
  if (manager->IsDeviceActive(DeviceType::kCamera))
    vm_media_capture_state_ |= MediaCaptureState::kVideo;
  if (manager->IsDeviceActive(DeviceType::kMic))
    vm_media_capture_state_ |= MediaCaptureState::kAudio;

  media_controller_->NotifyVmMediaNotificationState(
      manager->IsNotificationActive(
          ash::VmCameraMicManager::kCameraNotification),
      manager->IsNotificationActive(ash::VmCameraMicManager::kMicNotification),
      manager->IsNotificationActive(
          ash::VmCameraMicManager::kCameraAndMicNotification));
}

void MediaClientImpl::OnCameraPrivacySwitchStatusChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  // Show camera privacy switch toast.
  switch (state) {
    case cros::mojom::CameraPrivacySwitchState::UNKNOWN:
      break;
    case cros::mojom::CameraPrivacySwitchState::ON: {
      if (camera_privacy_switch_state_ !=
          cros::mojom::CameraPrivacySwitchState::UNKNOWN) {
        base::UmaHistogramEnumeration(kCameraPrivacySwitchEventsHistogramName,
                                      CameraPrivacySwitchEvent::kSwitchOn);
      }
      // On some devices, the camera privacy switch state can only be detected
      // while the camera is active. In that case the privacy switch state will
      // become known as the camera becomes active, in which case showing a
      // notification is preferred to showing a toast.
      if (is_camera_active_ &&
          camera_privacy_switch_state_ ==
              cros::mojom::CameraPrivacySwitchState::UNKNOWN) {
        ShowCameraOffNotification();
        break;
      }

      ash::ToastManager::Get()->Cancel(kCameraPrivacySwitchOffToastId);
      ash::ToastData toast(
          kCameraPrivacySwitchOnToastId,
          l10n_util::GetStringUTF16(IDS_CAMERA_PRIVACY_SWITCH_ON_TOAST),
          kCameraPrivacySwitchToastDurationMs,
          /*dismiss_text=*/base::nullopt,
          /*visible_on_lock_screen=*/true);
      ash::ToastManager::Get()->Show(toast);
      break;
    }
    case cros::mojom::CameraPrivacySwitchState::OFF: {
      if (camera_privacy_switch_state_ !=
          cros::mojom::CameraPrivacySwitchState::UNKNOWN) {
        base::UmaHistogramEnumeration(kCameraPrivacySwitchEventsHistogramName,
                                      CameraPrivacySwitchEvent::kSwitchOff);
      }

      // Record the time since the time notification that the privacy switch was
      // on was shown.
      if (!camera_switch_notification_shown_timestamp_.is_null()) {
        base::TimeDelta time_from_notification =
            base::TimeTicks::Now() -
            camera_switch_notification_shown_timestamp_;
        int64_t seconds_from_notification = time_from_notification.InSeconds();
        base::UmaHistogramExactLinear(
            kCameraPrivacySwitchTimeToTurnOffHistogramName,
            seconds_from_notification / kRecordedTimeGranularityInSeconds,
            kMaxRecordedTimeInSeconds / kRecordedTimeGranularityInSeconds);
        camera_switch_notification_shown_timestamp_ = base::TimeTicks();
      }
      // Only show the "Camera is on" toast if the privacy switch state changed
      // from ON (avoiding the toast when the state changes from UNKNOWN).
      if (camera_privacy_switch_state_ !=
          cros::mojom::CameraPrivacySwitchState::ON) {
        break;
      }
      ash::ToastManager::Get()->Cancel(kCameraPrivacySwitchOnToastId);
      ash::ToastData toast(
          kCameraPrivacySwitchOffToastId,
          l10n_util::GetStringUTF16(IDS_CAMERA_PRIVACY_SWITCH_OFF_TOAST),
          kCameraPrivacySwitchToastDurationMs,
          /*dismiss_text=*/base::nullopt,
          /*visible_on_lock_screen=*/true);
      ash::ToastManager::Get()->Show(toast);
      break;
    }
  }

  if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
    SystemNotificationHelper::GetInstance()->Close(
        kCameraPrivacySwitchOnNotificationId);
  }

  camera_privacy_switch_state_ = state;
}

void MediaClientImpl::OnActiveClientChange(cros::mojom::CameraClientType type,
                                           bool is_active) {
  is_camera_active_ = is_active;

  if (is_active && camera_privacy_switch_state_ ==
                       cros::mojom::CameraPrivacySwitchState::ON) {
    ShowCameraOffNotification();
  }
}

void MediaClientImpl::EnableCustomMediaKeyHandler(
    content::BrowserContext* context,
    ui::MediaKeysListener::Delegate* delegate) {
  auto it = media_key_delegates_.find(context);

  DCHECK(!base::Contains(media_key_delegates_, context) ||
         it->second == delegate);

  media_key_delegates_.emplace(context, delegate);

  UpdateForceMediaClientKeyHandling();
}

void MediaClientImpl::DisableCustomMediaKeyHandler(
    content::BrowserContext* context,
    ui::MediaKeysListener::Delegate* delegate) {
  if (!base::Contains(media_key_delegates_, context))
    return;

  auto it = media_key_delegates_.find(context);
  DCHECK_EQ(it->second, delegate);

  media_key_delegates_.erase(it);

  UpdateForceMediaClientKeyHandling();
}

void MediaClientImpl::UpdateForceMediaClientKeyHandling() {
  bool enabled = GetCurrentMediaKeyDelegate() != nullptr;

  if (enabled == is_forcing_media_client_key_handling_)
    return;

  is_forcing_media_client_key_handling_ = enabled;

  media_controller_->SetForceMediaClientKeyHandling(enabled);
}

ui::MediaKeysListener::Delegate* MediaClientImpl::GetCurrentMediaKeyDelegate()
    const {
  auto it = media_key_delegates_.find(active_context_);
  if (it != media_key_delegates_.end())
    return it->second;

  return nullptr;
}

void MediaClientImpl::HandleMediaAction(ui::KeyboardCode keycode) {
  if (ui::MediaKeysListener::Delegate* custom = GetCurrentMediaKeyDelegate()) {
    custom->OnMediaKeysAccelerator(ui::Accelerator(keycode, ui::EF_NONE));
    return;
  }

  // This API is used by Chrome apps so we should use the logged in user here.
  extensions::MediaPlayerAPI* player_api =
      extensions::MediaPlayerAPI::Get(ProfileManager::GetActiveUserProfile());
  if (!player_api)
    return;

  extensions::MediaPlayerEventRouter* router =
      player_api->media_player_event_router();
  if (!router)
    return;

  switch (keycode) {
    case ui::VKEY_MEDIA_NEXT_TRACK:
      router->NotifyNextTrack();
      break;
    case ui::VKEY_MEDIA_PREV_TRACK:
      router->NotifyPrevTrack();
      break;
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      router->NotifyTogglePlayState();
      break;
    // TODO(https://crbug.com/1053777): Handle media action for VKEY_MEDIA_PLAY,
    // VKEY_MEDIA_PAUSE, VKEY_MEDIA_STOP, VKEY_OEM_103, and VKEY_OEM_104.
    case ui::VKEY_MEDIA_PLAY:
    case ui::VKEY_MEDIA_PAUSE:
    case ui::VKEY_MEDIA_STOP:
    case ui::VKEY_OEM_103:  // KEYCODE_MEDIA_REWIND
    case ui::VKEY_OEM_104:  // KEYCODE_MEDIA_FAST_FORWARD
      break;
    default:
      break;
  }
}

void MediaClientImpl::ShowCameraOffNotification() {
  base::UmaHistogramEnumeration(
      kCameraPrivacySwitchEventsHistogramName,
      CameraPrivacySwitchEvent::kSwitchOnNotificationShown);

  camera_switch_notification_shown_timestamp_ = base::TimeTicks::Now();

  SystemNotificationHelper::GetInstance()->Close(
      kCameraPrivacySwitchOnNotificationId);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kCameraPrivacySwitchOnNotificationId,
          l10n_util::GetStringUTF16(
              IDS_CAMERA_PRIVACY_SWITCH_ON_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_CAMERA_PRIVACY_SWITCH_ON_NOTIFICATION_MESSAGE),
          std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kCameraPrivacySwitchNotifierId),
          message_center::RichNotificationData(),
          new message_center::HandleNotificationClickDelegate(
              base::DoNothing::Repeatedly()),
          vector_icons::kVideocamOffIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(*notification);
}
