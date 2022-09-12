// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/scoped_alsa_mixer.h"

#include "base/check.h"
#include "base/logging.h"

#define ALSA_ASSERT(func, ...)                                        \
  do {                                                                \
    int err = alsa_->func(__VA_ARGS__);                               \
    LOG_ASSERT(err >= 0) << #func " error: " << alsa_->StrError(err); \
  } while (0)

namespace chromecast {
namespace media {

ScopedAlsaMixer::ScopedAlsaMixer(::media::AlsaWrapper* alsa,
                                 const std::string& mixer_device_name,
                                 const std::string& mixer_element_name)
    : alsa_(alsa),
      mixer_device_name_(mixer_device_name),
      mixer_element_name_(mixer_element_name) {
  DCHECK(alsa_);
  Refresh();
}

ScopedAlsaMixer::~ScopedAlsaMixer() {
  if (mixer) {
    alsa_->MixerClose(mixer);
  }
}

void ScopedAlsaMixer::Refresh() {
  if (mixer) {
    alsa_->MixerClose(mixer);
    DVLOG(2) << "Reopening mixer element \"" << mixer_element_name_
             << "\" on device \"" << mixer_device_name_ << "\"";
  } else {
    LOG(INFO) << "Opening mixer element \"" << mixer_element_name_
              << "\" on device \"" << mixer_device_name_ << "\"";
  }
  mixer = nullptr;
  element = nullptr;
  int alsa_err = alsa_->MixerOpen(&mixer, 0);
  if (alsa_err < 0) {
    LOG(ERROR) << "MixerOpen error: " << alsa_->StrError(alsa_err);
    mixer = nullptr;
    return;
  }
  alsa_err = alsa_->MixerAttach(mixer, mixer_device_name_.c_str());
  if (alsa_err < 0) {
    LOG(ERROR) << "MixerAttach error: " << alsa_->StrError(alsa_err);
    alsa_->MixerClose(mixer);
    mixer = nullptr;
    return;
  }
  ALSA_ASSERT(MixerElementRegister, mixer, nullptr, nullptr);
  alsa_err = alsa_->MixerLoad(mixer);
  if (alsa_err < 0) {
    LOG(ERROR) << "MixerLoad error: " << alsa_->StrError(alsa_err);
    alsa_->MixerClose(mixer);
    mixer = nullptr;
    return;
  }
  snd_mixer_selem_id_t* sid = nullptr;
  ALSA_ASSERT(MixerSelemIdMalloc, &sid);
  alsa_->MixerSelemIdSetIndex(sid, 0);
  alsa_->MixerSelemIdSetName(sid, mixer_element_name_.c_str());
  element = alsa_->MixerFindSelem(mixer, sid);
  if (!element) {
    LOG(ERROR) << "Simple mixer control element \"" << mixer_element_name_
               << "\" not found.";
  }
  alsa_->MixerSelemIdFree(sid);
}

}  // namespace media
}  // namespace chromecast
