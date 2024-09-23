// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/media_effects/media_device_info.h"

namespace settings {

// Handler for media devices selection in content settings.
class MediaDevicesSelectionHandler
    : public media_effects::MediaDeviceInfo::Observer,
      public SettingsPageUIHandler {
 public:
  explicit MediaDevicesSelectionHandler(Profile* profile);

  MediaDevicesSelectionHandler(const MediaDevicesSelectionHandler&) = delete;
  MediaDevicesSelectionHandler& operator=(const MediaDevicesSelectionHandler&) =
      delete;

  ~MediaDevicesSelectionHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // MediaDeviceInfo::Observer:
  void OnAudioDevicesChanged(
      const std::optional<std::vector<media::AudioDeviceDescription>>& devices)
      override;
  void OnVideoDevicesChanged(
      const std::optional<std::vector<media::VideoCaptureDeviceInfo>>& devices)
      override;

  void SetWebUiForTest(content::WebUI* web_ui);

 private:
  // Requests initialization of the devices menu.
  void InitializeCaptureDevices(const base::Value::List& args);

  // Sets the preferred audio/video capture device for media. |args| includes
  // the media type (kAuudio/kVideo) and the unique id of the new default device
  // that the user has chosen.
  void SetPreferredCaptureDevice(const base::Value::List& args);

  // Helpers methods to update the device menus.
  void UpdateDevicesMenu(
      const std::vector<media::AudioDeviceDescription>& devices);
  void UpdateDevicesMenu(
      const std::vector<media::VideoCaptureDeviceInfo>& devices);

  // Gets the human readable name of the device.
  std::string GetDeviceDisplayName(
      const media::AudioDeviceDescription& device) const;
  // Gets the human readable name of the device.
  std::string GetDeviceDisplayName(
      const media::VideoCaptureDeviceInfo& device) const;

  raw_ptr<Profile> profile_;  // Weak pointer.

  base::ScopedObservation<media_effects::MediaDeviceInfo,
                          media_effects::MediaDeviceInfo::Observer>
      observation_{this};

  std::vector<media::AudioDeviceDescription> audio_device_infos_;
  std::vector<media::VideoCaptureDeviceInfo> video_device_infos_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_
