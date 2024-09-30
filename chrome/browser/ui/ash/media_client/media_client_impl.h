// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_CLIENT_MEDIA_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_CLIENT_MEDIA_CLIENT_IMPL_H_

#include "ash/public/cpp/media_client.h"
#include "ash/public/cpp/media_controller.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "ui/base/accelerators/media_keys_listener.h"

using GetSourceInfosResult =
    video_capture::mojom::VideoSourceProvider::GetSourceInfosResult;

class MediaClientImpl : public ash::MediaClient,
                        public ash::VmCameraMicManager::Observer,
                        public BrowserListObserver,
                        public MediaCaptureDevicesDispatcher::Observer,
                        public media::CameraPrivacySwitchObserver,
                        public media::CameraActiveClientObserver {
 public:
  MediaClientImpl();

  MediaClientImpl(const MediaClientImpl&) = delete;
  MediaClientImpl& operator=(const MediaClientImpl&) = delete;

  ~MediaClientImpl() override;

  // Initializes and set as client for ash.
  void Init();

  // Tests can provide a mock mojo interface for the ash controller.
  void InitForTesting(ash::MediaController* controller);

  // Returns a pointer to the singleton MediaClient, or nullptr if none exists.
  static MediaClientImpl* Get();

  // ash::MediaClient:
  void HandleMediaNextTrack() override;
  void HandleMediaPlayPause() override;
  void HandleMediaPlay() override;
  void HandleMediaPause() override;
  void HandleMediaStop() override;
  void HandleMediaPrevTrack() override;
  void HandleMediaSeekBackward() override;
  void HandleMediaSeekForward() override;
  void RequestCaptureState() override;
  void SuspendMediaSessions() override;

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // ash::VmCameraMicManager::Observer
  void OnVmCameraMicActiveChanged(ash::VmCameraMicManager* manager) override;

  // media::CameraPrivacySwitchObserver:
  void OnCameraHWPrivacySwitchStateChanged(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state) override;
  void OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  // media::CameraActiveClientObserver:
  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_active,
      const base::flat_set<std::string>& device_ids) override;

  // Enables/disables custom media key handling when |context| is the active
  // browser. Media keys will be forwarded to |delegate|.
  void EnableCustomMediaKeyHandler(content::BrowserContext* context,
                                   ui::MediaKeysListener::Delegate* delegate);
  void DisableCustomMediaKeyHandler(content::BrowserContext* context,
                                    ui::MediaKeysListener::Delegate* delegate);

 private:
  friend class MediaClientAppUsingCameraTest;

  using GetSourceCallback = base::OnceCallback<void(
      GetSourceInfosResult,
      const std::vector<media::VideoCaptureDeviceInfo>&)>;

  // Passes a given callback to the GetSourcesInfos() method of the video source
  // provider
  void ProcessSourceInfos(GetSourceCallback callback);

  // Sets |is_forcing_media_client_key_handling_| to true if
  // |GetCurrentMediaKeyDelegate| returns a delegate. This will also mirror the
  // value of |is_forcing_media_client_key_handling_| to Ash.
  void UpdateForceMediaClientKeyHandling();

  // Gets the current media key delegate that we should forward media keys to.
  // This will be the delegate that is associated with the current active
  // browser. If there is no delegate registered or there is no active browser
  // then this will return |nullptr|.
  ui::MediaKeysListener::Delegate* GetCurrentMediaKeyDelegate() const;

  // Returns the media capture state for the current user at
  // |user_index|. (Note that this isn't stable, see implementation comment on
  // RequestCaptureState()).
  ash::MediaCaptureState GetMediaCaptureStateByIndex(int user_index);

  // Handles the media key action for the key with |code|. If there is a
  // |GetCurrentMediaKeyDelegate| then the action will be forwarded to the
  // delegate. Otherwise, we will forward the action to the extensions API.
  void HandleMediaAction(ui::KeyboardCode code);

  // Shows a notification informing the user that an app is trying to use the
  // camera while the camera hardware privacy switch is turned on. If
  // `resurface` is false the notification text will be updated but the
  // notification won't be brought to the users attention again.
  void ShowCameraOffNotification(const std::string& device_id,
                                 const std::string& device_name,
                                 bool resurface = true);

  // Removes the camera notification for device with id `device_id` and returns
  // iterator to the next device id in `devices_having_visible_notification_`.
  base::flat_set<std::string>::iterator RemoveCameraOffNotificationForDevice(
      const std::string& device_id);

  void OnGetSourceInfosByCameraHWPrivacySwitchStateChanged(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state,
      GetSourceInfosResult,
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  void OnGetSourceInfosByActiveClientChanged(
      const base::flat_set<std::string>& active_device_ids,
      GetSourceInfosResult,
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  // Returns true if the device (camera) with id `device_id` is being actively
  // used by a client.
  bool IsDeviceActive(const std::string& device_id);

  void OnGetSourceInfosByCameraSWPrivacySwitchStateChanged(
      GetSourceInfosResult,
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  void OnGetCameraSWPrivacySwitchState(
      cros::mojom::CameraPrivacySwitchState state);

  raw_ptr<ash::MediaController> media_controller_ = nullptr;

  base::flat_map<content::BrowserContext*,
                 raw_ptr<ui::MediaKeysListener::Delegate, CtnExperimental>>
      media_key_delegates_;

  // If true then ash will always forward media keys to |this| instead of trying
  // to handle them first.
  bool is_forcing_media_client_key_handling_ = false;

  raw_ptr<content::BrowserContext, DanglingUntriaged> active_context_ = nullptr;

  ash::MediaCaptureState vm_media_capture_state_ =
      ash::MediaCaptureState::kNone;

  // The most recent observed camera privacy switch state.
  base::flat_map<std::string, cros::mojom::CameraPrivacySwitchState>
      device_id_to_camera_privacy_switch_state_;

  int active_camera_client_count_ = 0;

  // Most recent time the notification that the camera privacy switch is on was
  // shown.
  base::TimeTicks camera_switch_notification_shown_timestamp_;

  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_remote_;

  // Points each CameraClientType to a set which contains the id of the devices
  // the CameraClientType is currently using.
  base::flat_map<cros::mojom::CameraClientType, base::flat_set<std::string>>
      devices_used_by_client_;

  // Set of IDs of the camera devices having a visible notification in the
  // message center.
  base::flat_set<std::string> devices_having_visible_notification_;

  // Stores the state of the camera software privacy switch state locally.
  cros::mojom::CameraPrivacySwitchState camera_sw_privacy_switch_state_ =
      cros::mojom::CameraPrivacySwitchState::UNKNOWN;

  ash::PrivacyHubNotification notification_;

  // Can be used to disable/enable the display of HW switch toasts.
  bool hw_switch_toasts_disabled_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MediaClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_CLIENT_MEDIA_CLIENT_IMPL_H_
