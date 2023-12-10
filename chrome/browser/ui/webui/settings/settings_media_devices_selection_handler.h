// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"

namespace settings {

// Handler for media devices selection in content settings.
class MediaDevicesSelectionHandler
    : public MediaCaptureDevicesDispatcher::Observer,
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

  // MediaCaptureDevicesDispatcher::Observer:
  void OnUpdateAudioDevices(const blink::MediaStreamDevices& devices) override;
  void OnUpdateVideoDevices(const blink::MediaStreamDevices& devices) override;

  void SetWebUiForTest(content::WebUI* web_ui);

 private:
  // Fetches the list of default capture devices.
  void GetDefaultCaptureDevices(const base::Value::List& args);

  // Sets the default audio/video capture device for media. |args| includes the
  // media type (kAuudio/kVideo) and the unique id of the new default device
  // that the user has chosen.
  void SetDefaultCaptureDevice(const base::Value::List& args);

  // Helpers methods to update the device menus.
  void UpdateDevicesMenu(std::string type,
                         const blink::MediaStreamDevices& devices);

  // Gets the human readable name of the device.
  std::string GetDeviceDisplayName(
      const blink::MediaStreamDevice& device) const;

  raw_ptr<Profile> profile_;  // Weak pointer.

  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          MediaCaptureDevicesDispatcher::Observer>
      observation_{this};

  blink::MediaStreamDevices audio_device_infos_;
  blink::MediaStreamDevices video_device_infos_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_
