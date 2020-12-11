// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "content/public/browser/web_contents.h"

namespace settings {

// Handler for media devices selection in content settings.
class MediaDevicesSelectionHandler
    : public MediaCaptureDevicesDispatcher::Observer,
      public SettingsPageUIHandler {
 public:
  explicit MediaDevicesSelectionHandler(Profile* profile);
  ~MediaDevicesSelectionHandler() override;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // MediaCaptureDevicesDispatcher::Observer:
  void OnUpdateAudioDevices(const blink::MediaStreamDevices& devices) override;
  void OnUpdateVideoDevices(const blink::MediaStreamDevices& devices) override;

 private:
  enum DeviceType {
    AUDIO,
    VIDEO,
  };

  // Fetches the list of default capture devices.
  void GetDefaultCaptureDevices(const base::ListValue* args);

  // Sets the default audio/video capture device for media. |args| includes the
  // media type (kAuudio/kVideo) and the unique id of the new default device
  // that the user has chosen.
  void SetDefaultCaptureDevice(const base::ListValue* args);

  // Helpers methods to update the device menus.
  void UpdateDevicesMenuForType(DeviceType type);
  void UpdateDevicesMenu(DeviceType type,
                         const blink::MediaStreamDevices& devices);

  // Gets the human readable name of the device.
  std::string GetDeviceDisplayName(
      const blink::MediaStreamDevice& device) const;

  Profile* profile_;  // Weak pointer.

  ScopedObserver<MediaCaptureDevicesDispatcher,
                 MediaCaptureDevicesDispatcher::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesSelectionHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MEDIA_DEVICES_SELECTION_HANDLER_H_
