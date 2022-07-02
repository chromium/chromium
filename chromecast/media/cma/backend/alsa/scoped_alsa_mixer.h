// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SCOPED_ALSA_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SCOPED_ALSA_MIXER_H_

#include <string>

#include "media/audio/alsa/alsa_wrapper.h"

namespace chromecast {
namespace media {

class ScopedAlsaMixer {
 public:
  ScopedAlsaMixer(::media::AlsaWrapper* alsa,
                  const std::string& mixer_device_name,
                  const std::string& mixer_element_name);
  ~ScopedAlsaMixer();
  ScopedAlsaMixer(const ScopedAlsaMixer&) = delete;
  ScopedAlsaMixer& operator=(const ScopedAlsaMixer&) = delete;

  void Refresh();

  snd_mixer_elem_t* element = nullptr;
  snd_mixer_t* mixer = nullptr;

 private:
  ::media::AlsaWrapper* const alsa_;

  const std::string mixer_device_name_;
  const std::string mixer_element_name_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SCOPED_ALSA_MIXER_H_
