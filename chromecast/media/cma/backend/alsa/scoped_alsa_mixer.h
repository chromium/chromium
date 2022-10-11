// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SCOPED_ALSA_MIXER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SCOPED_ALSA_MIXER_H_

#include <string>

#include "base/message_loop/message_pump_for_io.h"
#include "base/sequence_checker.h"
#include "media/audio/alsa/alsa_wrapper.h"

namespace chromecast {
namespace media {

class ScopedAlsaMixer : public base::MessagePumpForIO::FdWatcher {
 public:
  ScopedAlsaMixer(::media::AlsaWrapper* alsa,
                  const std::string& mixer_device_name,
                  const std::string& mixer_element_name);
  ~ScopedAlsaMixer() override;
  ScopedAlsaMixer(const ScopedAlsaMixer&) = delete;
  ScopedAlsaMixer& operator=(const ScopedAlsaMixer&) = delete;

  // (Re)open the mixer and update the |element| pointer to a value that is
  // either nullable or valid.
  void RefreshElement();
  // Register a callback to invoke on new mixer events.
  void WatchForEvents(snd_mixer_elem_callback_t cb, void* cb_private_value);

  // base::MessagePumpForIO::FdWatcher implementation:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  snd_mixer_t* GetMixerForTest();
  // TODO(jyw) stop exposing bare |element|
  snd_mixer_elem_t* element = nullptr;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  ::media::AlsaWrapper* const alsa_;
  snd_mixer_t* mixer_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  const std::string mixer_device_name_;
  const std::string mixer_element_name_;

  std::vector<std::unique_ptr<base::MessagePumpForIO::FdWatchController>>
      file_descriptor_watchers_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_SCOPED_ALSA_MIXER_H_
