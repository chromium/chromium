// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/strings/grit/extensions_strings.h"
#endif

namespace {

const char kAudio[] = "mic";
const char kVideo[] = "camera";

std::vector<media::AudioDeviceDescription>::const_iterator
GetPreferredDeviceInfoIter(
    const std::string& id,
    const std::vector<media::AudioDeviceDescription>& infos) {
  auto preferred_iter =
      std::find_if(infos.begin(), infos.end(),
                   [id](const media::AudioDeviceDescription& info) {
                     return info.unique_id == id;
                   });
  CHECK(preferred_iter < infos.end());
  return preferred_iter;
}

std::vector<media::VideoCaptureDeviceInfo>::const_iterator
GetPreferredDeviceInfoIter(
    const std::string& id,
    const std::vector<media::VideoCaptureDeviceInfo>& infos) {
  auto preferred_iter =
      std::find_if(infos.begin(), infos.end(),
                   [id](const media::VideoCaptureDeviceInfo& info) {
                     return info.descriptor.device_id == id;
                   });
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
  observation_.Observe(media_effects::MediaDeviceInfo::GetInstance());
}

void MediaDevicesSelectionHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void MediaDevicesSelectionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeCaptureDevices",
      base::BindRepeating(
          &MediaDevicesSelectionHandler::InitializeCaptureDevices,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPreferredCaptureDevice",
      base::BindRepeating(
          &MediaDevicesSelectionHandler::SetPreferredCaptureDevice,
          base::Unretained(this)));
}

void MediaDevicesSelectionHandler::OnAudioDevicesChanged(
    const std::optional<std::vector<media::AudioDeviceDescription>>& devices) {
  PrefService* prefs = profile_->GetPrefs();
  audio_device_infos_ =
      devices.value_or(std::vector<media::AudioDeviceDescription>{});
  media_prefs::PreferenceRankAudioDeviceInfos(*prefs, audio_device_infos_);
  UpdateDevicesMenu(audio_device_infos_);
}

void MediaDevicesSelectionHandler::OnVideoDevicesChanged(
    const std::optional<std::vector<media::VideoCaptureDeviceInfo>>& devices) {
  PrefService* prefs = profile_->GetPrefs();
  video_device_infos_ =
      devices.value_or(std::vector<media::VideoCaptureDeviceInfo>{});
  media_prefs::PreferenceRankVideoDeviceInfos(*prefs, video_device_infos_);
  UpdateDevicesMenu(video_device_infos_);
}

void MediaDevicesSelectionHandler::SetWebUiForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void MediaDevicesSelectionHandler::InitializeCaptureDevices(
    const base::Value::List& args) {
  DCHECK_EQ(1U, args.size());
  if (!args[0].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const std::string& type = args[0].GetString();
  DCHECK(!type.empty());

  if (type == kAudio) {
    OnAudioDevicesChanged(
        media_effects::MediaDeviceInfo::GetInstance()->GetAudioDeviceInfos());
  } else if (type == kVideo) {
    OnVideoDevicesChanged(
        media_effects::MediaDeviceInfo::GetInstance()->GetVideoDeviceInfos());
  }
}

void MediaDevicesSelectionHandler::SetPreferredCaptureDevice(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  if (!args[0].is_string() || !args[1].is_string()) {
    NOTREACHED_IN_MIGRATION();
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
    NOTREACHED_IN_MIGRATION();
  }
}

void MediaDevicesSelectionHandler::UpdateDevicesMenu(
    const std::vector<media::AudioDeviceDescription>& devices) {
  AllowJavascript();

  auto real_default_device_id = media_effects::GetRealDefaultDeviceId(devices);

  std::string selected_device_id;
  // Build the list of devices to send to JS.
  base::Value::List device_list;
  for (const auto& device : devices) {
    if (real_default_device_id.has_value() &&
        media::AudioDeviceDescription::IsDefaultDevice(device.unique_id)) {
      continue;
    }
    if (selected_device_id.empty()) {
      selected_device_id = device.unique_id;
    }
    base::Value::Dict entry;
    entry.Set("name", GetDeviceDisplayName(device));
    entry.Set("id", device.unique_id);
    device_list.Append(std::move(entry));
  }

  base::Value selected_value(selected_device_id);
  base::Value type_value(kAudio);

  FireWebUIListener("updateDevicesMenu", type_value, device_list,
                    selected_value);
}

void MediaDevicesSelectionHandler::UpdateDevicesMenu(
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  AllowJavascript();

  // Build the list of devices to send to JS.
  base::Value::List device_list;
  for (const auto& device : devices) {
    base::Value::Dict entry;
    entry.Set("name", GetDeviceDisplayName(device));
    entry.Set("id", device.descriptor.device_id);
    device_list.Append(std::move(entry));
  }

  base::Value selected_device_id(
      devices.empty() ? "" : devices.front().descriptor.device_id);
  base::Value type_value(kVideo);

  FireWebUIListener("updateDevicesMenu", type_value, device_list,
                    selected_device_id);
}

std::string MediaDevicesSelectionHandler::GetDeviceDisplayName(
    const media::AudioDeviceDescription& device) const {
  const auto is_virtual_default_device =
      media::AudioDeviceDescription::IsDefaultDevice(device.unique_id);
  if (is_virtual_default_device) {
    return l10n_util::GetStringUTF8(IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC);
  }
  if (device.is_system_default) {
    return l10n_util::GetStringFUTF8(
        IDS_MEDIA_PREVIEW_SYSTEM_DEFAULT_MIC_PARENTHETICAL,
        base::UTF8ToUTF16(device.device_name));
  }
  return device.device_name;
}

std::string MediaDevicesSelectionHandler::GetDeviceDisplayName(
    const media::VideoCaptureDeviceInfo& device) const {
  std::string facing_info;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  switch (device.descriptor.facing) {
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_USER:
      facing_info = l10n_util::GetStringUTF8(IDS_CAMERA_FACING_USER);
      break;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT:
      facing_info = l10n_util::GetStringUTF8(IDS_CAMERA_FACING_ENVIRONMENT);
      break;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
#endif

  if (facing_info.empty())
    return device.descriptor.display_name();
  return device.descriptor.display_name() + " " + facing_info;
}

}  // namespace settings
