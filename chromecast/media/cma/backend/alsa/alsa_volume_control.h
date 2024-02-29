// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_ALSA_VOLUME_CONTROL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_ALSA_VOLUME_CONTROL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/timer/timer.h"
#include "chromecast/media/cma/backend/system_volume_control.h"
#include "media/audio/alsa/alsa_wrapper.h"

namespace chromecast {
namespace media {
class ScopedAlsaMixer;

// SystemVolumeControl implementation for ALSA.
class AlsaVolumeControl : public SystemVolumeControl {
 public:
  AlsaVolumeControl(Delegate* delegate,
                    std::unique_ptr<::media::AlsaWrapper> alsa);

  AlsaVolumeControl(const AlsaVolumeControl&) = delete;
  AlsaVolumeControl& operator=(const AlsaVolumeControl&) = delete;

  ~AlsaVolumeControl() override;

  // SystemVolumeControl interface.
  float GetRoundtripVolume(float volume) override;
  float DbToVolumeLevel(float db_volume) override;
  float VolumeLevelToDb(float level) override;
  float GetVolume() override;
  void SetVolume(float level) override;
  bool IsMuted() override;
  void SetMuted(bool muted) override;
  void SetPowerSave(bool power_save_on) override;
  void SetLimit(float limit) override;

 private:
  static std::string GetVolumeElementName();
  static std::string GetVolumeDeviceName();
  static std::string GetMuteElementName(::media::AlsaWrapper* alsa,
                                        const std::string& mixer_card_name,
                                        const std::string& mixer_element_name,
                                        const std::string& mute_card_name);
  static std::string GetMuteDeviceName();
  static std::vector<std::string> GetAmpElementNames();
  static std::string GetAmpDeviceName();

  static int VolumeOrMuteChangeCallback(snd_mixer_elem_t* elem,
                                        unsigned int mask);

  bool SetElementMuted(ScopedAlsaMixer* mixer, bool muted);
  // Returns true if all channels are muted, returns std::nullopt if element
  // state is not accessible.
  std::optional<bool> IsElementAllMuted(ScopedAlsaMixer* mixer);

  void OnVolumeOrMuteChanged();

  void CheckPowerSave();

  Delegate* const delegate_;

  const std::unique_ptr<::media::AlsaWrapper> alsa_;
  const std::string volume_mixer_device_name_;
  const std::string volume_mixer_element_name_;
  const std::string mute_mixer_device_name_;
  const std::string mute_mixer_element_name_;
  const std::string amp_mixer_device_name_;
  const std::vector<std::string> amp_mixer_element_names_;

  long volume_range_min_;  // NOLINT(runtime/int)
  long volume_range_max_;  // NOLINT(runtime/int)

  std::unique_ptr<ScopedAlsaMixer> volume_mixer_;
  std::unique_ptr<ScopedAlsaMixer> mute_mixer_;
  ScopedAlsaMixer* mute_mixer_ptr_;
  std::vector<std::unique_ptr<ScopedAlsaMixer>> amp_mixers_;

  bool last_power_save_on_ = false;
  base::OneShotTimer power_save_timer_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_ALSA_VOLUME_CONTROL_H_
