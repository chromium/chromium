// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/scoped_alsa_mixer.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/task/current_thread.h"
#include "base/types/fixed_array.h"

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
  RefreshElement();
}

ScopedAlsaMixer::~ScopedAlsaMixer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_descriptor_watchers_.clear();

  if (element) {
    alsa_->MixerElemSetCallback(element, nullptr);
    alsa_->MixerElemSetCallbackPrivate(element, nullptr);
  }
  if (mixer_) {
    alsa_->MixerClose(mixer_);
  }
}

snd_mixer_t* ScopedAlsaMixer::GetMixerForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mixer_;
}

void ScopedAlsaMixer::RefreshElement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (mixer_) {
    alsa_->MixerClose(mixer_);
    DVLOG(2) << "Reopening mixer element \"" << mixer_element_name_
             << "\" on device \"" << mixer_device_name_ << "\"";
  } else {
    LOG(INFO) << "Opening mixer element \"" << mixer_element_name_
              << "\" on device \"" << mixer_device_name_ << "\"";
  }
  mixer_ = nullptr;
  element = nullptr;
  int alsa_err = alsa_->MixerOpen(&mixer_, 0);
  if (alsa_err < 0) {
    LOG(ERROR) << "MixerOpen error: " << alsa_->StrError(alsa_err);
    mixer_ = nullptr;
    return;
  }
  alsa_err = alsa_->MixerAttach(mixer_, mixer_device_name_.c_str());
  if (alsa_err < 0) {
    LOG(ERROR) << "MixerAttach error: " << alsa_->StrError(alsa_err);
    alsa_->MixerClose(mixer_);
    mixer_ = nullptr;
    return;
  }
  ALSA_ASSERT(MixerElementRegister, mixer_, nullptr, nullptr);
  alsa_err = alsa_->MixerLoad(mixer_);
  if (alsa_err < 0) {
    LOG(ERROR) << "MixerLoad error: " << alsa_->StrError(alsa_err);
    alsa_->MixerClose(mixer_);
    mixer_ = nullptr;
    return;
  }
  snd_mixer_selem_id_t* sid = nullptr;
  ALSA_ASSERT(MixerSelemIdMalloc, &sid);
  alsa_->MixerSelemIdSetIndex(sid, 0);
  alsa_->MixerSelemIdSetName(sid, mixer_element_name_.c_str());
  element = alsa_->MixerFindSelem(mixer_, sid);
  if (!element) {
    LOG(ERROR) << "Simple mixer control element \"" << mixer_element_name_
               << "\" not found.";
  }
  alsa_->MixerSelemIdFree(sid);
}

void ScopedAlsaMixer::WatchForEvents(snd_mixer_elem_callback_t cb,
                                     void* cb_private_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!element || !cb) {
    return;
  }

  alsa_->MixerElemSetCallback(element, cb);
  alsa_->MixerElemSetCallbackPrivate(element, cb_private_value);

  int num_fds = alsa_->MixerPollDescriptorsCount(mixer_);
  DCHECK_GT(num_fds, 0);
  base::FixedArray<struct pollfd> pfds(num_fds);
  num_fds = alsa_->MixerPollDescriptors(mixer_, pfds.data(), num_fds);
  DCHECK_GT(num_fds, 0);
  file_descriptor_watchers_.clear();
  for (int i = 0; i < num_fds; ++i) {
    auto watcher =
        std::make_unique<base::MessagePumpForIO::FdWatchController>(FROM_HERE);
    base::CurrentIOThread::Get()->WatchFileDescriptor(
        pfds[i].fd, true /* persistent */, base::MessagePumpForIO::WATCH_READ,
        watcher.get(), this);
    file_descriptor_watchers_.push_back(std::move(watcher));
  }
}

void ScopedAlsaMixer::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!mixer_) {
    return;
  }
  alsa_->MixerHandleEvents(mixer_);
}

void ScopedAlsaMixer::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do.
}

}  // namespace media
}  // namespace chromecast
