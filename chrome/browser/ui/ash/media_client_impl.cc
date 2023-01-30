// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_client_impl.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/media_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/ash/extensions/media_player_api.h"
#include "chrome/browser/ash/extensions/media_player_event_router.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "media/capture/video/chromeos/public/cros_features.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

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

// The prefix of ID of the notification shown when the user tries to use a
// camera while the camera privacy switch is on.
constexpr char kCameraPrivacySwitchOnNotificationIdPrefix[] =
    "ash.media.camera.activity_with_privacy_switch_on.";

// The notifier ID for a notification shown when the user tries to use a camera
// while the camera privacy switch is on.
constexpr char kCameraPrivacySwitchNotifierId[] = "ash.media.camera";

// The ID for the toast shown when the camera privacy switch is turned on.
constexpr char kCameraPrivacySwitchOnToastId[] =
    "ash.media.camera.privacy_switch_on";

// The ID for the toast shown when the camera privacy switch is turned off.
constexpr char kCameraPrivacySwitchOffToastId[] =
    "ash.media.camera.privacy_switch_off";

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

const user_manager::User* GetActiveUser() {
  if (user_manager::UserManager::IsInitialized()) {
    return user_manager::UserManager::Get()->GetActiveUser();
  }

  return nullptr;
}

apps::AppRegistryCache* GetAppRegistryCache(
    const user_manager::User* active_user) {
  if (active_user) {
    return apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
        active_user->GetAccountId());
  }

  return nullptr;
}

apps::AppCapabilityAccessCache* GetAppCapabilityAccessCache(
    const user_manager::User* active_user) {
  if (active_user) {
    return apps::AppCapabilityAccessCacheWrapper::Get()
        .GetAppCapabilityAccessCache(active_user->GetAccountId());
  }

  return nullptr;
}

// Relieves GetNameOfAppAccessingCamera() of the responsibility for gathering up
// the AppRegistryCache and AppCapabilityAccessCache objects, which drastically
// simplifies the unit tests of that function.
std::string GetNameOfAppAccessingCameraInternal() {
  const user_manager::User* active_user = GetActiveUser();
  if (!active_user)
    return std::string();

  apps::AppRegistryCache* reg_cache = GetAppRegistryCache(active_user);
  DCHECK(reg_cache);
  apps::AppCapabilityAccessCache* cap_cache =
      GetAppCapabilityAccessCache(active_user);
  DCHECK(cap_cache);
  return MediaClientImpl::GetNameOfAppAccessingCamera(cap_cache, reg_cache);
}

std::string GetDeviceName(
    const std::string& device_id,
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  for (const auto& device : devices) {
    if (device.descriptor.device_id == device_id) {
      return device.descriptor.display_name();
    }
  }
  return std::string();
}

// Small helper to make sure that `kCameraPrivacySwitchOnNotificationIdPrefix`
// combined with `device_id` always produce the same identifier.
std::string PrivacySwitchOnNotificationIdForDevice(
    const std::string& device_id) {
  return base::StrCat({kCameraPrivacySwitchOnNotificationIdPrefix, device_id});
}

}  // namespace

MediaClientImpl::MediaClientImpl()
    : notification_(
          kCameraPrivacySwitchNotifierId,
          IDS_CAMERA_PRIVACY_SWITCH_ON_NOTIFICATION_TITLE,
          ash::PrivacyHubNotification::MessageIds{
              IDS_CAMERA_PRIVACY_SWITCH_ON_NOTIFICATION_MESSAGE},
          ash::PrivacyHubNotification::SensorSet{},
          base::MakeRefCounted<ash::PrivacyHubNotificationClickDelegate>(
              base::BindRepeating(
                  ash::PrivacyHubNotificationController::OpenSupportUrl,
                  ash::PrivacyHubNotificationController::Sensor::kCamera)),
          ash::NotificationCatalogName::kCameraPrivacySwitch,
          IDS_ASH_LEARN_MORE) {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  BrowserList::AddObserver(this);

  ash::VmCameraMicManager::Get()->AddObserver(this);

  // These checks are needed for testing, where Shell and/or the
  // SessionController may not exist.
  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->AddObserver(this);
  }

  // Camera service does not behave in non ChromeOS environment (e.g. testing,
  // linux chromeos).
  if (base::SysInfo::IsRunningOnChromeOS() &&
      base::FeatureList::IsEnabled(
          ash::features::kCameraPrivacySwitchNotifications) &&
      media::ShouldUseCrosCameraService()) {
    device_id_to_camera_privacy_switch_state_ =
        media::CameraHalDispatcherImpl::GetInstance()
            ->AddCameraPrivacySwitchObserver(this);
    media::CameraHalDispatcherImpl::GetInstance()->AddActiveClientObserver(
        this);
    media::CameraHalDispatcherImpl::GetInstance()
        ->GetCameraSWPrivacySwitchState(
            base::BindOnce(&MediaClientImpl::OnGetCameraSWPrivacySwitchState,
                           weak_ptr_factory_.GetWeakPtr()));
    content::GetVideoCaptureService().ConnectToVideoSourceProvider(
        video_source_provider_remote_.BindNewPipeAndPassReceiver());
  }

  notification_.builder().SetNotifierId(
      notification_.builder().GetNotifierId());

  DCHECK(!g_media_client);
  g_media_client = this;
}

MediaClientImpl::~MediaClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  g_media_client = nullptr;

  if (media_controller_ && ash::MediaController::Get() == media_controller_)
    media_controller_->SetClient(nullptr);

  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
  BrowserList::RemoveObserver(this);

  ash::VmCameraMicManager::Get()->RemoveObserver(this);

  if (ash::SessionController::Get()) {
    ash::SessionController::Get()->RemoveObserver(this);
  }

  if (base::SysInfo::IsRunningOnChromeOS() &&
      base::FeatureList::IsEnabled(
          ash::features::kCameraPrivacySwitchNotifications) &&
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
        ash::ProfileHelper::Get()->GetProfileByUser(user));
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

void MediaClientImpl::OnCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_source_provider_remote_->GetSourceInfos(base::BindOnce(
      &MediaClientImpl::OnGetSourceInfosByCameraHWPrivacySwitchStateChanged,
      weak_ptr_factory_.GetWeakPtr(), device_id, state));
}

void MediaClientImpl::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  camera_sw_privacy_switch_state_ = state;
  if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
    // The software switch is OFF. Display hardware switch notifications if any.
    video_source_provider_remote_->GetSourceInfos(base::BindOnce(
        &MediaClientImpl::OnGetSourceInfosByCameraSWPrivacySwitchStateChanged,
        weak_ptr_factory_.GetWeakPtr()));
  } else if (state == cros::mojom::CameraPrivacySwitchState::ON) {
    // The software switch is ON. Clear all hardware switch notifications.
    for (auto it = devices_having_visible_notification_.begin();
         it != devices_having_visible_notification_.end();) {
      it = RemoveCameraOffNotificationForDevice(*it);
    }
  }
}

void MediaClientImpl::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_new_active_client,
    const base::flat_set<std::string>& active_device_ids) {
  if (is_new_active_client) {
    active_camera_client_count_++;
  } else if (active_device_ids.empty()) {
    DCHECK(active_camera_client_count_ > 0);
    active_camera_client_count_--;
  }

  devices_used_by_client_.insert_or_assign(type, active_device_ids);

  video_source_provider_remote_->GetSourceInfos(
      base::BindOnce(&MediaClientImpl::OnGetSourceInfosByActiveClientChanged,
                     weak_ptr_factory_.GetWeakPtr(), active_device_ids));
}

void MediaClientImpl::OnCapabilityAccessUpdate(
    const apps::CapabilityAccessUpdate& capability_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Updating the map eagerly is not necessary. We just want to make sure that
  // the app name in the notification is one of the apps being used, not
  // necessarily the most recent one.
  if (capability_update.Camera().value_or(true) || last_device_for_app_.empty())
    return;

  apps::AppRegistryCache* registry_cache = GetAppRegistryCache(GetActiveUser());

  // This can happen during testing.
  if (!registry_cache)
    return;

  std::string app_name;
  registry_cache->ForOneApp(capability_update.AppId(),
                            [&app_name](const apps::AppUpdate& app_update) {
                              app_name = app_update.ShortName();
                            });

  if (app_name.empty())
    return;

  if (!last_device_for_app_.contains(app_name))
    return;

  const auto [device_id, device_name] = last_device_for_app_.at(app_name);

  // Remove the current app_name from the map. It will be repopulated in
  // `ShowCameraOffNotification`.
  last_device_for_app_.erase(app_name);

  // Make sure we don't show the notification depending on the order of the
  // observers being called and introduce a blinking effect when the last
  // application stops using the camera.
  if (active_camera_client_count_ > 0) {
    ShowCameraOffNotification(device_id, device_name, /*resurface=*/false);
  } else {
    RemoveCameraOffNotificationForDevice(device_id);
  }
}

void MediaClientImpl::OnAppCapabilityAccessCacheWillBeDestroyed(
    apps::AppCapabilityAccessCache* cache) {
  Observe(nullptr);
}

void MediaClientImpl::OnActiveUserSessionChanged(const AccountId& account_id) {
  apps::AppCapabilityAccessCache* capability_cache =
      apps::AppCapabilityAccessCacheWrapper::Get().GetAppCapabilityAccessCache(
          account_id);

  // This can happen when testing.
  if (!capability_cache)
    return;

  Observe(capability_cache);
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

std::string MediaClientImpl::GetNameOfAppAccessingCamera(
    apps::AppCapabilityAccessCache* capability_cache,
    apps::AppRegistryCache* registry_cache) {
  DCHECK(capability_cache);
  DCHECK(registry_cache);
  for (const std::string& app : capability_cache->GetAppsAccessingCamera()) {
    std::string name;
    registry_cache->ForOneApp(app, [&name](const apps::AppUpdate& update) {
      name = update.ShortName();
    });
    if (!name.empty())
      return name;
  }

  return std::string();
}

void MediaClientImpl::ShowCameraOffNotification(const std::string& device_id,
                                                const std::string& device_name,
                                                const bool resurface) {
  auto it = device_id_to_camera_privacy_switch_state_.find(device_id);
  if (it == device_id_to_camera_privacy_switch_state_.end() ||
      it->second != cros::mojom::CameraPrivacySwitchState::ON ||
      !IsDeviceActive(device_id)) {
    return;
  }

  if (ash::features::IsCrosPrivacyHubEnabled() &&
      camera_sw_privacy_switch_state_ ==
          cros::mojom::CameraPrivacySwitchState::ON) {
    return;
  }

  base::UmaHistogramEnumeration(
      kCameraPrivacySwitchEventsHistogramName,
      CameraPrivacySwitchEvent::kSwitchOnNotificationShown);

  camera_switch_notification_shown_timestamp_ = base::TimeTicks::Now();

  const std::u16string device_name_u16 = base::UTF8ToUTF16(device_name);
  // TODO(b/262380194) Show a message which includes the app name once we can
  // reliably determine which app is being used by what camera that has an
  // active privacy switch.
  if (const std::string app_name = GetNameOfAppAccessingCameraInternal();
      !app_name.empty()) {
    last_device_for_app_.insert_or_assign(
        app_name, std::make_pair(device_id, device_name));
  }

  if (resurface) {
    RemoveCameraOffNotificationForDevice(device_id);
  }

  SystemNotificationHelper::GetInstance()->Display(
      notification_.builder()
          .SetId(PrivacySwitchOnNotificationIdForDevice(device_id))
          .SetTitleWithArgs(IDS_CAMERA_PRIVACY_SWITCH_ON_NOTIFICATION_TITLE,
                            {device_name_u16})
          .SetMessageWithArgs(IDS_CAMERA_PRIVACY_SWITCH_ON_NOTIFICATION_MESSAGE,
                              {device_name_u16})
          .Build());
  devices_having_visible_notification_.insert(device_id);
}

base::flat_set<std::string>::iterator
MediaClientImpl::RemoveCameraOffNotificationForDevice(
    const std::string& device_id) {
  auto it = devices_having_visible_notification_.find(device_id);
  if (it != devices_having_visible_notification_.end()) {
    SystemNotificationHelper::GetInstance()->Close(
        PrivacySwitchOnNotificationIdForDevice(device_id));
    return devices_having_visible_notification_.erase(it);
  }
  return it;
}

void MediaClientImpl::OnGetSourceInfosByCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
    cros::mojom::CameraPrivacySwitchState state,
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string device_name = GetDeviceName(device_id, devices);
  std::u16string device_name_u16 = base::UTF8ToUTF16(device_name);
  if (device_name.empty()) {
    LOG(ERROR) << "Could not find VideoCaptureDeviceDescriptor with device_id: "
               << device_id;
    return;
  }

  cros::mojom::CameraPrivacySwitchState old_state =
      device_id_to_camera_privacy_switch_state_.contains(device_id)
          ? device_id_to_camera_privacy_switch_state_[device_id]
          : cros::mojom::CameraPrivacySwitchState::UNKNOWN;
  device_id_to_camera_privacy_switch_state_[device_id] = state;

  // Show camera privacy switch toast.
  switch (state) {
    case cros::mojom::CameraPrivacySwitchState::UNKNOWN:
      break;
    case cros::mojom::CameraPrivacySwitchState::ON: {
      if (old_state != cros::mojom::CameraPrivacySwitchState::UNKNOWN) {
        base::UmaHistogramEnumeration(kCameraPrivacySwitchEventsHistogramName,
                                      CameraPrivacySwitchEvent::kSwitchOn);
      }
      // On some devices, the camera privacy switch state can only be detected
      // while the camera is active. In that case the privacy switch state will
      // become known as the camera becomes active, in which case showing a
      // notification is preferred to showing a toast.
      if (active_camera_client_count_ > 0 &&
          old_state == cros::mojom::CameraPrivacySwitchState::UNKNOWN) {
        ShowCameraOffNotification(device_id, device_name);
        break;
      }

      ash::ToastManager::Get()->Cancel(kCameraPrivacySwitchOffToastId);
      ash::ToastData toast(
          kCameraPrivacySwitchOnToastId,
          ash::ToastCatalogName::kCameraPrivacySwitchOn,
          l10n_util::GetStringFUTF16(IDS_CAMERA_PRIVACY_SWITCH_ON_TOAST,
                                     device_name_u16),
          ash::ToastData::kDefaultToastDuration,
          /*visible_on_lock_screen=*/true);
      ash::ToastManager::Get()->Show(std::move(toast));
      break;
    }
    case cros::mojom::CameraPrivacySwitchState::OFF: {
      if (old_state != cros::mojom::CameraPrivacySwitchState::UNKNOWN) {
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
      if (old_state != cros::mojom::CameraPrivacySwitchState::ON) {
        break;
      }
      ash::ToastManager::Get()->Cancel(kCameraPrivacySwitchOnToastId);
      ash::ToastData toast(
          kCameraPrivacySwitchOffToastId,
          ash::ToastCatalogName::kCameraPrivacySwitchOff,
          l10n_util::GetStringFUTF16(IDS_CAMERA_PRIVACY_SWITCH_OFF_TOAST,
                                     device_name_u16),
          ash::ToastData::kDefaultToastDuration,
          /*visible_on_lock_screen=*/true);
      ash::ToastManager::Get()->Show(std::move(toast));
      break;
    }
  }

  if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
    RemoveCameraOffNotificationForDevice(device_id);
  }
}

void MediaClientImpl::OnGetSourceInfosByActiveClientChanged(
    const base::flat_set<std::string>& active_device_ids,
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& device : devices) {
    const std::string& device_id = device.descriptor.device_id;
    const std::string& device_name = device.descriptor.display_name();

    if (active_device_ids.find(device_id) != active_device_ids.end()) {
      // As the device is being actively used by the client, display a
      // notification.
      ShowCameraOffNotification(device_id, device_name);
    } else if (!IsDeviceActive(device_id)) {
      // No application is actively using this camera. Remove the notification
      // for the device if exists.
      RemoveCameraOffNotificationForDevice(device_id);
    }
  }

  // Remove notifications for detached devices if any.
  for (auto it = devices_having_visible_notification_.begin();
       it != devices_having_visible_notification_.end();) {
    if (IsDeviceActive(*it)) {
      ++it;
    } else {
      it = RemoveCameraOffNotificationForDevice(*it);
    }
  }
}

bool MediaClientImpl::IsDeviceActive(const std::string& device_id) {
  for (const auto& [client_type, devices_used] : devices_used_by_client_) {
    if (devices_used.find(device_id) != devices_used.end()) {
      return true;
    }
  }
  return false;
}

void MediaClientImpl::OnGetSourceInfosByCameraSWPrivacySwitchStateChanged(
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& device : devices) {
    ShowCameraOffNotification(device.descriptor.device_id,
                              device.descriptor.display_name());
  }
}

void MediaClientImpl::OnGetCameraSWPrivacySwitchState(
    cros::mojom::CameraPrivacySwitchState state) {
  camera_sw_privacy_switch_state_ = state;
}
