// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/indicator_spec.h"

#include "build/build_config.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"

namespace vr {

IndicatorSpec::IndicatorSpec(UiElementName name,
                             UiElementName webvr_name,
                             const gfx::VectorIcon& icon,
                             int resource_string,
                             int background_resource_string,
                             int potential_resource_string,
                             CapturingStateModelMemberPtr signal,
                             bool is_url)
    : name(name),
      webvr_name(webvr_name),
      icon(icon),
      resource_string(resource_string),
      background_resource_string(background_resource_string),
      potential_resource_string(potential_resource_string),
      signal(signal),
      is_url(is_url) {}

IndicatorSpec::IndicatorSpec(const IndicatorSpec& other)
    : name(other.name),
      webvr_name(other.webvr_name),
      icon(other.icon),
      resource_string(other.resource_string),
      background_resource_string(other.background_resource_string),
      potential_resource_string(other.potential_resource_string),
      signal(other.signal),
      is_url(other.is_url) {}

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
       &CapturingStateModel::location_access_enabled,
       false},

      {kAudioCaptureIndicator, kWebVrAudioCaptureIndicator,
       vector_icons::kMicIcon,
       IDS_VR_SHELL_SITE_IS_USING_MICROPHONE,
       IDS_VR_SHELL_BG_IS_USING_MICROPHONE,
       IDS_VR_SHELL_SITE_CAN_USE_MICROPHONE,
       &CapturingStateModel::audio_capture_enabled,
       false},

      {kVideoCaptureIndicator, kWebVrVideoCaptureIndicator,
       vector_icons::kVideocamIcon,
       IDS_VR_SHELL_SITE_IS_USING_CAMERA,
       IDS_VR_SHELL_BG_IS_USING_CAMERA,
       IDS_VR_SHELL_SITE_CAN_USE_CAMERA,
       &CapturingStateModel::video_capture_enabled,
       false},

      {kBluetoothConnectedIndicator, kWebVrBluetoothConnectedIndicator,
       vector_icons::kBluetoothConnectedIcon,
       IDS_VR_SHELL_SITE_IS_USING_BLUETOOTH,
#if defined(OS_ANDROID)
       IDS_VR_SHELL_BG_IS_USING_BLUETOOTH,
#else
       0,
#endif
       IDS_VR_SHELL_SITE_CAN_USE_BLUETOOTH,
       &CapturingStateModel::bluetooth_connected,
       false},

      {kScreenCaptureIndicator, kWebVrScreenCaptureIndicator,
       vector_icons::kScreenShareIcon,
       IDS_VR_SHELL_SITE_IS_SHARING_SCREEN,
       IDS_VR_SHELL_BG_IS_SHARING_SCREEN,
       IDS_VR_SHELL_SITE_CAN_SHARE_SCREEN,
       &CapturingStateModel::screen_capture_enabled,
       false},

#if !defined(OS_ANDROID)
      {kUsbConnectedIndicator, kWebXrUsbConnectedIndicator,
       vector_icons::kUsbIcon,
       IDS_VR_SHELL_SITE_IS_USING_USB,
       0,
       0,
       &CapturingStateModel::usb_connected,
       false},

       {kMidiConnectedIndicator, kWebXrMidiConnectedIndicator,
       vector_icons::kMidiIcon,
       IDS_VR_SHELL_SITE_IS_USING_MIDI,
       0,
       IDS_VR_SHELL_SITE_CAN_USE_MIDI,
       &CapturingStateModel::midi_connected,
       false},
#endif
  };

  return specs;
}
// clang-format on

}  // namespace vr
