// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace {

const char kAudio[] = "mic";
const char kVideo[] = "camera";

}  // namespace

namespace settings {

MediaDevicesSelectionHandler::MediaDevicesSelectionHandler(Profile* profile)
    : profile_(profile) {}

MediaDevicesSelectionHandler::~MediaDevicesSelectionHandler() {
}

void MediaDevicesSelectionHandler::OnJavascriptAllowed() {
  // Register to the device observer list to get up-to-date device lists.
  observation_.Observe(MediaCaptureDevicesDispatcher::GetInstance());
}

void MediaDevicesSelectionHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void MediaDevicesSelectionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDefaultCaptureDevices",
      base::BindRepeating(
          &MediaDevicesSelectionHandler::GetDefaultCaptureDevices,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDefaultCaptureDevice",
      base::BindRepeating(
          &MediaDevicesSelectionHandler::SetDefaultCaptureDevice,
          base::Unretained(this)));
}

void MediaDevicesSelectionHandler::OnUpdateAudioDevices(
    const blink::MediaStreamDevices& devices) {
  UpdateDevicesMenu(AUDIO, devices);
}

void MediaDevicesSelectionHandler::OnUpdateVideoDevices(
    const blink::MediaStreamDevices& devices) {
  UpdateDevicesMenu(VIDEO, devices);
}

void MediaDevicesSelectionHandler::GetDefaultCaptureDevices(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  if (!args[0].is_string()) {
    NOTREACHED();
    return;
  }
  const std::string& type = args[0].GetString();
  DCHECK(!type.empty());

  if (type == kAudio)
    UpdateDevicesMenuForType(AUDIO);
  else if (type == kVideo)
    UpdateDevicesMenuForType(VIDEO);
}

void MediaDevicesSelectionHandler::SetDefaultCaptureDevice(
    const base::Value::List& args) {
  DCHECK_EQ(2U, args.size());
  if (!args[0].is_string() || !args[1].is_string()) {
    NOTREACHED();
    return;
  }
  const std::string& type = args[0].GetString();
  const std::string& device = args[1].GetString();

  DCHECK(!type.empty());
  DCHECK(!device.empty());

  PrefService* prefs = profile_->GetPrefs();
  if (type == kAudio)
    prefs->SetString(prefs::kDefaultAudioCaptureDevice, device);
  else if (type == kVideo)
    prefs->SetString(prefs::kDefaultVideoCaptureDevice, device);
  else
    NOTREACHED();
}

void MediaDevicesSelectionHandler::UpdateDevicesMenu(
    DeviceType type,
    const blink::MediaStreamDevices& devices) {
  AllowJavascript();

  // Get the default device unique id from prefs.
  PrefService* prefs = profile_->GetPrefs();
  std::string default_device;
  std::string device_type;
  switch (type) {
    case AUDIO:
      default_device = prefs->GetString(prefs::kDefaultAudioCaptureDevice);
      device_type = kAudio;
      break;
    case VIDEO:
      default_device = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
      device_type = kVideo;
      break;
  }

  // Build the list of devices to send to JS.
  std::string default_id;
  base::Value::List device_list;
  for (const auto& device : devices) {
    base::Value::Dict entry;
    entry.Set("name", GetDeviceDisplayName(device));
    entry.Set("id", device.id);
    device_list.Append(std::move(entry));
    if (device.id == default_device)
      default_id = default_device;
  }

  // Use the first device as the default device if the preferred default device
  // does not exist in the OS.
  if (!devices.empty() && default_id.empty())
    default_id = devices[0].id;

  base::Value default_value(default_id);
  base::Value type_value(device_type);

  FireWebUIListener("updateDevicesMenu", type_value, device_list,
                    default_value);
}

std::string MediaDevicesSelectionHandler::GetDeviceDisplayName(
    const blink::MediaStreamDevice& device) const {
  std::string facing_info;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  switch (device.video_facing) {
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_USER:
      facing_info = l10n_util::GetStringUTF8(IDS_CAMERA_FACING_USER);
      break;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT:
      facing_info = l10n_util::GetStringUTF8(IDS_CAMERA_FACING_ENVIRONMENT);
      break;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE:
      break;
    case media::VideoFacingMode::NUM_MEDIA_VIDEO_FACING_MODES:
      NOTREACHED();
      break;
  }
#endif

  if (facing_info.empty())
    return device.name;
  return device.name + " " + facing_info;
}

void MediaDevicesSelectionHandler::UpdateDevicesMenuForType(DeviceType type) {
  blink::MediaStreamDevices devices;
  switch (type) {
    case AUDIO:
      devices = MediaCaptureDevicesDispatcher::GetInstance()->
          GetAudioCaptureDevices();
      break;
    case VIDEO:
      devices = MediaCaptureDevicesDispatcher::GetInstance()->
          GetVideoCaptureDevices();
      break;
  }

  UpdateDevicesMenu(type, devices);
}

}  // namespace settings
