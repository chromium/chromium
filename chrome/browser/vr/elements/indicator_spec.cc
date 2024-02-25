// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/indicator_spec.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"

namespace vr {

IndicatorSpec::IndicatorSpec(UiElementName name,
                             UiElementName webvr_name,
                             const gfx::VectorIcon& icon,
                             int resource_string,
                             int background_resource_string,
                             int potential_resource_string,
                             CapturingStateModelMemberPtr signal)
    : name(name),
      webvr_name(webvr_name),
      icon(icon),
      resource_string(resource_string),
      background_resource_string(background_resource_string),
      potential_resource_string(potential_resource_string),
      signal(signal) {}

IndicatorSpec::IndicatorSpec(const IndicatorSpec& other)
    : name(other.name),
      webvr_name(other.webvr_name),
      icon(*other.icon),
      resource_string(other.resource_string),
      background_resource_string(other.background_resource_string),
      potential_resource_string(other.potential_resource_string),
      signal(other.signal) {}

IndicatorSpec::~IndicatorSpec() {}

// clang-format off
std::vector<IndicatorSpec> GetIndicatorSpecs() {

  std::vector<IndicatorSpec> specs = {
      {kLocationAccessIndicator, kWebVrLocationAccessIndicator,
       kMyLocationIcon,
       IDS_VR_SHELL_SITE_IS_TRACKING_LOCATION,
       // Background tabs cannot track high accuracy location.
       0,
       IDS_VR_SHELL_SITE_CAN_TRACK_LOCATION,
       &CapturingStateModel::location_access_enabled},

      {kAudioCaptureIndicator, kWebVrAudioCaptureIndicator,
       vector_icons::kMicIcon,
       IDS_VR_SHELL_SITE_IS_USING_MICROPHONE,
       IDS_VR_SHELL_BG_IS_USING_MICROPHONE,
       IDS_VR_SHELL_SITE_CAN_USE_MICROPHONE,
       &CapturingStateModel::audio_capture_enabled},

      {kVideoCaptureIndicator, kWebVrVideoCaptureIndicator,
       vector_icons::kVideocamIcon,
       IDS_VR_SHELL_SITE_IS_USING_CAMERA,
       IDS_VR_SHELL_BG_IS_USING_CAMERA,
       IDS_VR_SHELL_SITE_CAN_USE_CAMERA,
       &CapturingStateModel::video_capture_enabled},

      {kBluetoothConnectedIndicator, kWebVrBluetoothConnectedIndicator,
       vector_icons::kBluetoothConnectedIcon,
       IDS_VR_SHELL_SITE_IS_USING_BLUETOOTH,
#if BUILDFLAG(IS_ANDROID)
       IDS_VR_SHELL_BG_IS_USING_BLUETOOTH,
#else
       0,
#endif
       IDS_VR_SHELL_SITE_CAN_USE_BLUETOOTH,
       &CapturingStateModel::bluetooth_connected},

      {kScreenCaptureIndicator, kWebVrScreenCaptureIndicator,
       vector_icons::kScreenShareIcon,
       IDS_VR_SHELL_SITE_IS_SHARING_SCREEN,
       IDS_VR_SHELL_BG_IS_SHARING_SCREEN,
       IDS_VR_SHELL_SITE_CAN_SHARE_SCREEN,
       &CapturingStateModel::screen_capture_enabled},

#if !BUILDFLAG(IS_ANDROID)
      {kUsbConnectedIndicator, kWebXrUsbConnectedIndicator,
       vector_icons::kUsbIcon,
       IDS_VR_SHELL_SITE_IS_USING_USB,
       0,
       0,
       &CapturingStateModel::usb_connected},

       {kMidiConnectedIndicator, kWebXrMidiConnectedIndicator,
       vector_icons::kMidiIcon,
       IDS_VR_SHELL_SITE_IS_USING_MIDI,
       0,
       IDS_VR_SHELL_SITE_CAN_USE_MIDI,
       &CapturingStateModel::midi_connected},
#endif
  };

  return specs;
}
// clang-format on

}  // namespace vr
