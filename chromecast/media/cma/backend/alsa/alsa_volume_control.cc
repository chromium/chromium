// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/alsa_volume_control.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/task/current_thread.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/media/cma/backend/alsa/scoped_alsa_mixer.h"
#include "media/base/media_switches.h"

#define ALSA_ASSERT(func, ...)                                        \
  do {                                                                \
    int err = alsa_->func(__VA_ARGS__);                               \
    LOG_ASSERT(err >= 0) << #func " error: " << alsa_->StrError(err); \
  } while (0)

namespace chromecast {
namespace media {

namespace {

const char kAlsaDefaultDeviceName[] = "default";
const char kAlsaDefaultVolumeElementName[] = "Master";
const char kAlsaMuteMixerElementName[] = "Mute";

constexpr base::TimeDelta kPowerSaveCheckTime = base::Minutes(5);

}  // namespace

// static
std::string AlsaVolumeControl::GetVolumeElementName() {
  std::string mixer_element_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlsaVolumeElementName);
  if (mixer_element_name.empty()) {
    mixer_element_name = kAlsaDefaultVolumeElementName;
  }
  return mixer_element_name;
}

// static
std::string AlsaVolumeControl::GetVolumeDeviceName() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string mixer_device_name =
      command_line->GetSwitchValueASCII(switches::kAlsaVolumeDeviceName);
  if (!mixer_device_name.empty()) {
    return mixer_device_name;
  }

  // If the output device was overridden, then the mixer should default to
  // that device.
  mixer_device_name =
      command_line->GetSwitchValueASCII(switches::kAlsaOutputDevice);
  if (!mixer_device_name.empty()) {
    return mixer_device_name;
  }
  return kAlsaDefaultDeviceName;
}

// Mixers that are implemented with ALSA's softvol plugin don't have mute
// switches available. This function allows ALSA-based AvSettings to fall back
// on another mixer which solely implements mute for the system.
// static
std::string AlsaVolumeControl::GetMuteElementName(
    ::media::AlsaWrapper* alsa,
    const std::string& mixer_device_name,
    const std::string& mixer_element_name,
    const std::string& mute_device_name) {
  DCHECK(alsa);
  std::string mute_element_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlsaMuteElementName);
  if (!mute_element_name.empty()) {
    return mute_element_name;
  }

  ScopedAlsaMixer mixer(alsa, mixer_device_name, mixer_element_name);
  if (!mixer.element) {
    LOG(WARNING) << "The default ALSA mixer element does not exist.";
    return mixer_element_name;
  }
  if (alsa->MixerSelemHasPlaybackSwitch(mixer.element)) {
    return mixer_element_name;
  }

  ScopedAlsaMixer mute(alsa, mute_device_name, kAlsaMuteMixerElementName);
  if (!mute.element) {
    LOG(WARNING) << "The default ALSA mixer does not have a playback switch "
                    "and a fallback mute element was not found, "
                    "mute will not work.";
    return mixer_element_name;
  }
  if (alsa->MixerSelemHasPlaybackSwitch(mute.element)) {
    return kAlsaMuteMixerElementName;
  }

  LOG(WARNING) << "The default ALSA mixer does not have a playback switch "
                  "and the fallback mute element does not have a playback "
                  "switch, mute will not work.";
  return mixer_element_name;
}

// static
std::string AlsaVolumeControl::GetMuteDeviceName() {
  std::string mute_device_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlsaMuteDeviceName);
  if (!mute_device_name.empty()) {
    return mute_device_name;
  }

  // If the mute mixer device was not specified directly, use the same device as
  // the volume mixer.
  return GetVolumeDeviceName();
}

// static
std::vector<std::string> AlsaVolumeControl::GetAmpElementNames() {
  std::vector<std::string> mixer_element_names = base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlsaAmpElementName),
      ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return mixer_element_names;
}

// static
std::string AlsaVolumeControl::GetAmpDeviceName() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string mixer_device_name =
      command_line->GetSwitchValueASCII(switches::kAlsaAmpDeviceName);
  if (!mixer_device_name.empty()) {
    return mixer_device_name;
  }

  // If the amp mixer device was not specified directly, use the same device as
  // the volume mixer.
  return GetVolumeDeviceName();
}

AlsaVolumeControl::AlsaVolumeControl(Delegate* delegate,
                                     std::unique_ptr<::media::AlsaWrapper> alsa)
    : delegate_(delegate),
      alsa_(std::move(alsa)),
      volume_mixer_device_name_(GetVolumeDeviceName()),
      volume_mixer_element_name_(GetVolumeElementName()),
      mute_mixer_device_name_(GetMuteDeviceName()),
      mute_mixer_element_name_(GetMuteElementName(alsa_.get(),
                                                  volume_mixer_device_name_,
                                                  volume_mixer_element_name_,
                                                  mute_mixer_device_name_)),
      amp_mixer_device_name_(GetAmpDeviceName()),
      amp_mixer_element_names_(GetAmpElementNames()),
      volume_range_min_(0),
      volume_range_max_(0),
      mute_mixer_ptr_(nullptr) {
  DCHECK(delegate_);
  LOG(INFO) << "Volume device = " << volume_mixer_device_name_
            << ", element = " << volume_mixer_element_name_;
  LOG(INFO) << "Mute device = " << mute_mixer_device_name_
            << ", element = " << mute_mixer_element_name_;

  std::string amp_element_name_list = "[";
  for (const auto& amp_mixer_element_name : amp_mixer_element_names_) {
    amp_element_name_list += amp_mixer_element_name;
    amp_element_name_list += ",";
  }
  if (!amp_mixer_element_names_.empty()) {
    amp_element_name_list.pop_back();
  }
  amp_element_name_list += "]";

  LOG(INFO) << "Idle device = " << amp_mixer_device_name_
            << ", elements = " << amp_element_name_list;

  volume_mixer_ = std::make_unique<ScopedAlsaMixer>(
      alsa_.get(), volume_mixer_device_name_, volume_mixer_element_name_);
  if (volume_mixer_->element) {
    ALSA_ASSERT(MixerSelemGetPlaybackVolumeRange, volume_mixer_->element,
                &volume_range_min_, &volume_range_max_);
    volume_mixer_->WatchForEvents(
        &AlsaVolumeControl::VolumeOrMuteChangeCallback,
        reinterpret_cast<void*>(this));
  }

  if (mute_mixer_element_name_ != volume_mixer_element_name_) {
    mute_mixer_ = std::make_unique<ScopedAlsaMixer>(
        alsa_.get(), mute_mixer_device_name_, mute_mixer_element_name_);
    if (mute_mixer_->element) {
      mute_mixer_ptr_ = mute_mixer_.get();
      mute_mixer_->WatchForEvents(
          &AlsaVolumeControl::VolumeOrMuteChangeCallback,
          reinterpret_cast<void*>(this));
    }
  } else {
    mute_mixer_ptr_ = volume_mixer_.get();
  }

  for (const auto& amp_mixer_element_name : amp_mixer_element_names_) {
    amp_mixers_.emplace_back(std::make_unique<ScopedAlsaMixer>(
        alsa_.get(), amp_mixer_device_name_, amp_mixer_element_name));
    if (amp_mixers_.back()->element) {
      amp_mixers_.back()->WatchForEvents(nullptr, nullptr);
    }
  }
}

AlsaVolumeControl::~AlsaVolumeControl() = default;

float AlsaVolumeControl::GetRoundtripVolume(float volume) {
  if (volume_range_max_ == volume_range_min_) {
    return 0.0f;
  }

  long level = 0;  // NOLINT(runtime/int)
  level = std::round((std::clamp(volume, 0.0f, 1.0f) *
                      (volume_range_max_ - volume_range_min_)) +
                     volume_range_min_);
  return static_cast<float>(level - volume_range_min_) /
         static_cast<float>(volume_range_max_ - volume_range_min_);
}

float AlsaVolumeControl::VolumeLevelToDb(float volume) {
  long level = 0;  // NOLINT(runtime/int)
  if (volume_range_max_ == volume_range_min_) {
    level = volume_range_max_;
  } else {
    level = std::round((volume * (volume_range_max_ - volume_range_min_)) +
                       volume_range_min_);
  }
  long volume_db = 0;  // NOLINT(runtime/int)
  ALSA_ASSERT(MixerSelemAskPlaybackVolDb, volume_mixer_->element, level,
              &volume_db);
  return static_cast<float>(volume_db * 0.01f);
}

float AlsaVolumeControl::DbToVolumeLevel(float volume_db) {
  if (volume_range_max_ == volume_range_min_) {
    return 0.0f;
  }
  long level = 0.0f;  // NOLINT(runtime/int)
  ALSA_ASSERT(MixerSelemAskPlaybackDbVol, volume_mixer_->element,
              std::round(volume_db * 100.0f), &level);
  return static_cast<float>(level - volume_range_min_) /
         static_cast<float>(volume_range_max_ - volume_range_min_);
}

float AlsaVolumeControl::GetVolume() {
  if (!volume_mixer_->element) {
    return 0.0f;
  }
  long level = 0;  // NOLINT(runtime/int)
  ALSA_ASSERT(MixerSelemGetPlaybackVolume, volume_mixer_->element,
              SND_MIXER_SCHN_MONO, &level);
  return static_cast<float>(level - volume_range_min_) /
         static_cast<float>(volume_range_max_ - volume_range_min_);
}

void AlsaVolumeControl::SetVolume(float level) {
  if (!volume_mixer_->element) {
    return;
  }
  float volume = std::round((level * (volume_range_max_ - volume_range_min_)) +
                            volume_range_min_);
  ALSA_ASSERT(MixerSelemSetPlaybackVolumeAll, volume_mixer_->element, volume);
}

bool AlsaVolumeControl::IsMuted() {
  return IsElementAllMuted(mute_mixer_ptr_).value_or(false);
}

void AlsaVolumeControl::SetMuted(bool muted) {
  if (!SetElementMuted(mute_mixer_ptr_, muted)) {
    LOG(ERROR) << "Mute failed: no mute switch on mixer element.";
  }
}

void AlsaVolumeControl::SetPowerSave(bool power_save_on) {
  for (const auto& amp_mixer : amp_mixers_) {
    amp_mixer->RefreshElement();
    if (IsElementAllMuted(amp_mixer.get()).value_or(false) == power_save_on) {
      LOG(INFO) << "Power Save already set to: " << power_save_on;
      continue;
    }
    if (last_power_save_on_ == power_save_on) {
      LOG(WARNING) << "Power Save was set to: " << !last_power_save_on_
                   << " by others";
      metrics::CastMetricsHelper::GetInstance()->RecordSimpleAction(
          (last_power_save_on_
               ? "Cast.Platform.VolumeControl.PowerSaveDisturbedOff"
               : "Cast.Platform.VolumeControl.PowerSaveDisturbedOn"));
    }
    if (!SetElementMuted(amp_mixer.get(), power_save_on)) {
      LOG(ERROR) << "Failed to set Power Save to " << power_save_on
                 << ": no amp switch on mixer element.";
      metrics::CastMetricsHelper::GetInstance()->RecordSimpleAction(
          (power_save_on ? "Cast.Platform.VolumeControl.PowerSaveFailedOn"
                         : "Cast.Platform.VolumeControl.PowerSaveFailedOff"));
    } else {
      LOG(INFO) << "Set Power Save to: " << power_save_on;
    }
  }
  last_power_save_on_ = power_save_on;
  if (last_power_save_on_) {
    // Schedule a checker so underruns will not wake up the amplifier
    // for a long time.
    power_save_timer_.Start(FROM_HERE, kPowerSaveCheckTime, this,
                            &AlsaVolumeControl::CheckPowerSave);
  } else {
    power_save_timer_.Stop();
  }
}

void AlsaVolumeControl::SetLimit(float limit) {}

bool AlsaVolumeControl::SetElementMuted(ScopedAlsaMixer* mixer, bool muted) {
  if (!mixer || !mixer->element ||
      !alsa_->MixerSelemHasPlaybackSwitch(mixer->element)) {
    return false;
  }
  bool success = true;
  for (int32_t channel = 0; channel <= SND_MIXER_SCHN_LAST; ++channel) {
    int err = alsa_->MixerSelemSetPlaybackSwitch(
        mixer->element, static_cast<snd_mixer_selem_channel_id_t>(channel),
        !muted);
    if (err != 0) {
      success = false;
      LOG(ERROR) << "MixerSelemSetPlaybackSwitch: " << alsa_->StrError(err);
    }
  }
  return success;
}

std::optional<bool> AlsaVolumeControl::IsElementAllMuted(
    ScopedAlsaMixer* mixer) {
  if (!mixer || !mixer->element ||
      !alsa_->MixerSelemHasPlaybackSwitch(mixer->element)) {
    return std::nullopt;
  }
  for (int32_t channel = 0; channel <= SND_MIXER_SCHN_LAST; ++channel) {
    int channel_unmuted;
    int err = alsa_->MixerSelemGetPlaybackSwitch(
        mixer->element, static_cast<snd_mixer_selem_channel_id_t>(channel),
        &channel_unmuted);
    if (err != 0) {
      LOG(ERROR) << "MixerSelemGetPlaybackSwitch: " << alsa_->StrError(err);
      return std::nullopt;
    }
    if (channel_unmuted) {
      return false;
    }
  }
  return true;
}

void AlsaVolumeControl::OnVolumeOrMuteChanged() {
  delegate_->OnSystemVolumeOrMuteChange(GetVolume(), IsMuted());
}

void AlsaVolumeControl::CheckPowerSave() {
  SetPowerSave(last_power_save_on_);
}

// static
int AlsaVolumeControl::VolumeOrMuteChangeCallback(snd_mixer_elem_t* elem,
                                                  unsigned int mask) {
  if (!(mask & SND_CTL_EVENT_MASK_VALUE))
    return 0;

  AlsaVolumeControl* instance = static_cast<AlsaVolumeControl*>(
      snd_mixer_elem_get_callback_private(elem));
  instance->OnVolumeOrMuteChanged();
  return 0;
}

// static
std::unique_ptr<SystemVolumeControl> SystemVolumeControl::Create(
    Delegate* delegate) {
  return std::make_unique<AlsaVolumeControl>(
      delegate, std::make_unique<::media::AlsaWrapper>());
}

}  // namespace media
}  // namespace chromecast
