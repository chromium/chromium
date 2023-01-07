// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AVSETTINGS_AVSETTINGS_DUMMY_H_
#define CHROMECAST_MEDIA_AVSETTINGS_AVSETTINGS_DUMMY_H_

#include "chromecast/public/avsettings.h"

namespace chromecast {

// Dummy implementation of AvSettings.
class AvSettingsDummy : public AvSettings {
 public:
  AvSettingsDummy();
  ~AvSettingsDummy() override;

  // AvSettings implementation:
  void Initialize(Delegate* delegate) override;
  void Finalize() override;
  ActiveState GetActiveState() override;
  bool TurnActive(bool switch_to_cast) override;
  bool TurnStandby() override;
  bool KeepSystemAwake(int time_millis) override;
  AudioVolumeControlType GetAudioVolumeControlType() override;
  bool GetAudioVolumeStepInterval(float* step_interval) override;
  int GetAudioCodecsSupported() override;
  int GetMaxAudioChannels(AudioCodec codec) override;
  bool GetScreenResolution(int* width, int* height) override;
  int GetHDCPVersion() override;
  int GetSupportedEotfs() override;
  int GetDolbyVisionFlags() override;
  int GetScreenWidthMm() override;
  int GetScreenHeightMm() override;
  bool GetOutputRestrictions(OutputRestrictions* restrictions) override;
  void ApplyOutputRestrictions(const OutputRestrictions& restrictions) override;
  WakeOnCastStatus GetWakeOnCastStatus() override;
  bool EnableWakeOnCast(bool enabled) override;
  HdrOutputType GetHdrOutputType() override;
  bool SetHdmiVideoMode(bool allow_4k,
                        int optimize_for_fps,
                        HdrOutputType output_type) override;
  bool IsHdrOutputSupportedByCurrentHdmiVideoMode(
      HdrOutputType output_type) override;

 private:
  Delegate* delegate_;

  // Disallow copy and assign.
  AvSettingsDummy(const AvSettingsDummy&) = delete;
  AvSettingsDummy& operator=(const AvSettingsDummy&) = delete;
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AVSETTINGS_AVSETTINGS_DUMMY_H_
