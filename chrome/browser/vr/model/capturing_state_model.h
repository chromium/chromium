// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_CAPTURING_STATE_MODEL_H_
#define CHROME_BROWSER_VR_MODEL_CAPTURING_STATE_MODEL_H_

#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

struct VR_BASE_EXPORT CapturingStateModel {
  bool audio_capture_enabled = false;
  bool video_capture_enabled = false;
  bool screen_capture_enabled = false;
  bool location_access_enabled = false;
  bool bluetooth_connected = false;
  bool usb_connected = false;
  bool midi_connected = false;

  bool operator==(const CapturingStateModel& rhs) const {
    return audio_capture_enabled == rhs.audio_capture_enabled &&
           video_capture_enabled == rhs.video_capture_enabled &&
           screen_capture_enabled == rhs.screen_capture_enabled &&
           location_access_enabled == rhs.location_access_enabled &&
           bluetooth_connected == rhs.bluetooth_connected &&
           usb_connected == rhs.usb_connected &&
           midi_connected == rhs.midi_connected;
  }

  bool operator!=(const CapturingStateModel& rhs) const {
    return !(*this == rhs);
  }

  bool IsAtleastOnePermissionGrantedOrInUse() const {
    return audio_capture_enabled || video_capture_enabled ||
           screen_capture_enabled || location_access_enabled ||
           bluetooth_connected || usb_connected || midi_connected;
  }

  // Returns a model with only those flags set that were false in old but
  // turned true in *this.
  CapturingStateModel NewlyUpdatedPermissions(const CapturingStateModel& old) {
    return {!old.audio_capture_enabled && audio_capture_enabled,
            !old.video_capture_enabled && video_capture_enabled,
            !old.screen_capture_enabled && screen_capture_enabled,
            !old.location_access_enabled && location_access_enabled,
            !old.bluetooth_connected && bluetooth_connected,
            !old.usb_connected && usb_connected,
            !old.midi_connected && midi_connected};
  }
};

typedef bool CapturingStateModel::*CapturingStateModelMemberPtr;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_CAPTURING_STATE_MODEL_H_
