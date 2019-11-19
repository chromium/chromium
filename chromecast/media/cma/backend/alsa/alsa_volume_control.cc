// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/alsa_volume_control.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/string_split.h"
#include "chromecast/base/chromecast_switches.h"
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

}  // namespace

class AlsaVolumeControl::ScopedAlsaMixer {
 public:
  ScopedAlsaMixer(::media::AlsaWrapper* alsa,
                  const std::string& mixer_device_name,
                  const std::string& mixer_element_name)
      : alsa_(alsa) {
    DCHECK(alsa_);
    LOG(INFO) << "Opening mixer element \"" << mixer_element_name
              << "\" on device \"" << mixer_device_name << "\"";
    int alsa_err = alsa_->MixerOpen(&mixer, 0);
    if (alsa_err < 0) {
      LOG(ERROR) << "MixerOpen error: " << alsa_->StrError(alsa_err);
      mixer = nullptr;
      return;
    }
    alsa_err = alsa_->MixerAttach(mixer, mixer_device_name.c_str());
    if (alsa_err < 0) {
      LOG(ERROR) << "MixerAttach error: " << alsa_->StrError(alsa_err);
      alsa_->MixerClose(mixer);
      mixer = nullptr;
      return;
    }
    ALSA_ASSERT(MixerElementRegister, mixer, NULL, NULL);
    alsa_err = alsa->MixerLoad(mixer);
    if (alsa_err < 0) {
      LOG(ERROR) << "MixerLoad error: " << alsa_->StrError(alsa_err);
      alsa_->MixerClose(mixer);
      mixer = nullptr;
      return;
    }

    snd_mixer_selem_id_t* sid = NULL;
    ALSA_ASSERT(MixerSelemIdMalloc, &sid);
    alsa_->MixerSelemIdSetIndex(sid, 0);
    alsa_->MixerSelemIdSetName(sid, mixer_element_name.c_str());
    element = alsa_->MixerFindSelem(mixer, sid);
    if (!element) {
      LOG(ERROR) << "Simple mixer control element \"" << mixer_element_name
                 << "\" not found.";
    }
    alsa_->MixerSelemIdFree(sid);
  }

  ~ScopedAlsaMixer() {
    if (mixer) {
      alsa_->MixerClose(mixer);
    }
  }

  snd_mixer_elem_t* element = nullptr;
  snd_mixer_t* mixer = nullptr;

 private:
  ::media::AlsaWrapper* const alsa_;
};

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
      switches::kAlsaAmpElementName), ",", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

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

AlsaVolumeControl::AlsaVolumeControl(Delegate* delegate)
    : delegate_(delegate),
      alsa_(std::make_unique<::media::AlsaWrapper>()),
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
    alsa_->MixerElemSetCallback(volume_mixer_->element,
                                &AlsaVolumeControl::VolumeOrMuteChangeCallback);
    alsa_->MixerElemSetCallbackPrivate(volume_mixer_->element,
                                       reinterpret_cast<void*>(this));
    RefreshMixerFds(volume_mixer_.get());
  }

  if (mute_mixer_element_name_ != volume_mixer_element_name_) {
    mute_mixer_ = std::make_unique<ScopedAlsaMixer>(
        alsa_.get(), mute_mixer_device_name_, mute_mixer_element_name_);
    if (mute_mixer_->element) {
      mute_mixer_ptr_ = mute_mixer_.get();

      alsa_->MixerElemSetCallback(
          mute_mixer_->element, &AlsaVolumeControl::VolumeOrMuteChangeCallback);
      alsa_->MixerElemSetCallbackPrivate(mute_mixer_->element,
                                         reinterpret_cast<void*>(this));
      RefreshMixerFds(mute_mixer_.get());
    }
  } else {
    mute_mixer_ptr_ = volume_mixer_.get();
  }

  for (const auto& amp_mixer_element_name : amp_mixer_element_names_) {
    amp_mixers_.emplace_back(std::make_unique<ScopedAlsaMixer>(
        alsa_.get(), amp_mixer_device_name_, amp_mixer_element_name));
    if (amp_mixers_.back()->element) {
      RefreshMixerFds(amp_mixers_.back().get());
    }
  }
}

AlsaVolumeControl::~AlsaVolumeControl() = default;

float AlsaVolumeControl::GetRoundtripVolume(float volume) {
  if (volume_range_max_ == volume_range_min_) {
    return 0.0f;
  }

  long level = 0;  // NOLINT(runtime/int)
  level = std::round((volume * (volume_range_max_ - volume_range_min_)) +
                     volume_range_min_);
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
  if (!mute_mixer_ptr_->element ||
      !alsa_->MixerSelemHasPlaybackSwitch(mute_mixer_ptr_->element)) {
    LOG(ERROR) << "Mute failed: no mute switch on mixer element.";
    return false;
  }

  bool muted = false;
  for (int32_t channel = 0; channel <= SND_MIXER_SCHN_LAST; ++channel) {
    int channel_enabled = 0;
    int err = alsa_->MixerSelemGetPlaybackSwitch(
        mute_mixer_ptr_->element,
        static_cast<snd_mixer_selem_channel_id_t>(channel), &channel_enabled);
    if (err == 0) {
      muted = muted || (channel_enabled == 0);
    }
  }
  return muted;
}

void AlsaVolumeControl::SetMuted(bool muted) {
  if (!SetElementMuted(mute_mixer_ptr_, muted)) {
    LOG(ERROR) << "Mute failed: no mute switch on mixer element.";
  }
}

void AlsaVolumeControl::SetPowerSave(bool power_save_on) {
  for (const auto& amp_mixer : amp_mixers_) {
    if (!SetElementMuted(amp_mixer.get(), power_save_on)) {
      LOG(INFO) << "Amp toggle failed: no amp switch on mixer element.";
    } else {
      LOG(INFO) << "Set Power Save to: " << power_save_on;
    }
  }
}

void AlsaVolumeControl::SetLimit(float limit) {}

bool AlsaVolumeControl::SetElementMuted(ScopedAlsaMixer* mixer, bool muted) {
  if (!mixer || !mixer->element ||
      !alsa_->MixerSelemHasPlaybackSwitch(mixer->element)) {
    return false;
  }
  for (int32_t channel = 0; channel <= SND_MIXER_SCHN_LAST; ++channel) {
    alsa_->MixerSelemSetPlaybackSwitch(
        mixer->element, static_cast<snd_mixer_selem_channel_id_t>(channel),
        !muted);
  }
  return true;
}

void AlsaVolumeControl::RefreshMixerFds(ScopedAlsaMixer* mixer) {
  int num_fds = alsa_->MixerPollDescriptorsCount(mixer->mixer);
  DCHECK_GT(num_fds, 0);
  struct pollfd pfds[num_fds];
  num_fds = alsa_->MixerPollDescriptors(mixer->mixer, pfds, num_fds);
  DCHECK_GT(num_fds, 0);
  for (int i = 0; i < num_fds; ++i) {
    auto watcher =
        std::make_unique<base::MessagePumpForIO::FdWatchController>(FROM_HERE);
    base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
        pfds[i].fd, true /* persistent */, base::MessagePumpForIO::WATCH_READ,
        watcher.get(), this);
    file_descriptor_watchers_.push_back(std::move(watcher));
  }
}

void AlsaVolumeControl::OnFileCanReadWithoutBlocking(int fd) {
  if (volume_mixer_->mixer) {
    alsa_->MixerHandleEvents(volume_mixer_->mixer);
  }
  if (mute_mixer_ && mute_mixer_->mixer) {
    alsa_->MixerHandleEvents(mute_mixer_->mixer);
  }
  for (const auto& amp_mixer : amp_mixers_) {
    if (amp_mixer->mixer) {
      // amixer locks up if we don't call this for unknown reasons.
      alsa_->MixerHandleEvents(amp_mixer->mixer);
    }
  }
}

void AlsaVolumeControl::OnFileCanWriteWithoutBlocking(int fd) {
  // Nothing to do.
}

void AlsaVolumeControl::OnVolumeOrMuteChanged() {
  delegate_->OnSystemVolumeOrMuteChange(GetVolume(), IsMuted());
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
  return std::make_unique<AlsaVolumeControl>(delegate);
}

}  // namespace media
}  // namespace chromecast
