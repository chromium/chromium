// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace {

const char kAudio[] = "mic";
const char kVideo[] = "camera";

blink::MediaStreamDevices::const_iterator GetPreferredDeviceInfoIter(
    const std::string& id,
    const blink::MediaStreamDevices& infos) {
  auto preferred_iter = std::find_if(
      infos.begin(), infos.end(),
      [id](const blink::MediaStreamDevice& info) { return info.id == id; });
  CHECK(preferred_iter < infos.end());
  return preferred_iter;
}

}  // namespace

namespace settings {

MediaDevicesSelectionHandler::MediaDevicesSelectionHandler(Profile* profile)
    : profile_(profile) {}

MediaDevicesSelectionHandler::~MediaDevicesSelectionHandler() = default;

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
  PrefService* prefs = profile_->GetPrefs();
  audio_device_infos_ = devices;
  media_prefs::PreferenceRankAudioDeviceInfos(*prefs, audio_device_infos_);
  UpdateDevicesMenu(kAudio, audio_device_infos_);
}

void MediaDevicesSelectionHandler::OnUpdateVideoDevices(
    const blink::MediaStreamDevices& devices) {
  PrefService* prefs = profile_->GetPrefs();
  video_device_infos_ = devices;
  media_prefs::PreferenceRankVideoDeviceInfos(*prefs, video_device_infos_);
  UpdateDevicesMenu(kVideo, video_device_infos_);
}

void MediaDevicesSelectionHandler::SetWebUiForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
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

  if (type == kAudio) {
    OnUpdateAudioDevices(
        MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices());
  } else if (type == kVideo) {
    OnUpdateVideoDevices(
        MediaCaptureDevicesDispatcher::GetInstance()->GetVideoCaptureDevices());
  }
}

void MediaDevicesSelectionHandler::SetDefaultCaptureDevice(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  if (!args[0].is_string() || !args[1].is_string()) {
    NOTREACHED();
    return;
  }
  const std::string& type = args[0].GetString();
  const std::string& device_id = args[1].GetString();

  CHECK(!type.empty());
  CHECK(!device_id.empty());

  PrefService* prefs = profile_->GetPrefs();
  if (type == kAudio) {
    auto preferred_iter =
        GetPreferredDeviceInfoIter(device_id, audio_device_infos_);
    media_prefs::UpdateAudioDevicePreferenceRanking(*prefs, preferred_iter,
                                                    audio_device_infos_);
  } else if (type == kVideo) {
    auto preferred_iter =
        GetPreferredDeviceInfoIter(device_id, video_device_infos_);
    media_prefs::UpdateVideoDevicePreferenceRanking(*prefs, preferred_iter,
                                                    video_device_infos_);
  } else {
    NOTREACHED();
  }
}

void MediaDevicesSelectionHandler::UpdateDevicesMenu(
    std::string type,
    const blink::MediaStreamDevices& devices) {
  AllowJavascript();

  // Build the list of devices to send to JS.
  base::Value::List device_list;
  for (const auto& device : devices) {
    base::Value::Dict entry;
    entry.Set("name", GetDeviceDisplayName(device));
    entry.Set("id", device.id);
    device_list.Append(std::move(entry));
  }

  base::Value default_value(devices.empty() ? "" : devices.front().id);
  base::Value type_value(type);

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
    default:
      NOTREACHED();
      break;
  }
#endif

  if (facing_info.empty())
    return device.name;
  return device.name + " " + facing_info;
}

}  // namespace settings
