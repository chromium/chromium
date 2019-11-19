// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_FUCHSIA_VOLUME_CONTROL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_FUCHSIA_VOLUME_CONTROL_H_

#include "chromecast/media/cma/backend/system_volume_control.h"

namespace chromecast {
namespace media {

// SystemVolumeControl implementation for Fuchsia. Current implementation is
// just a stub that doesn't do anything. Volume is still applied in the
// StreamMixer.
class FuchsiaVolumeControl : public SystemVolumeControl {
 public:
  FuchsiaVolumeControl();
  ~FuchsiaVolumeControl() override;

  // SystemVolumeControl interface.
  float GetRoundtripVolume(float volume) override;
  float GetVolume() override;
  void SetVolume(float level) override;
  bool IsMuted() override;
  void SetMuted(bool muted) override;
  void SetPowerSave(bool power_save_on) override;
  void SetLimit(float limit) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FuchsiaVolumeControl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_FUCHSIA_VOLUME_CONTROL_H_
