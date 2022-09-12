// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_IO_THREAD_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_IO_THREAD_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromecast {

// Provides process-wide access to a single kRealtimeAudio type IO thread.
class AudioIoThread {
 public:
  static AudioIoThread* Get();

  AudioIoThread();

  AudioIoThread(const AudioIoThread&) = delete;
  AudioIoThread& operator=(const AudioIoThread&) = delete;

  ~AudioIoThread();

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return thread_.task_runner();
  }

 private:
  base::Thread thread_;
};

}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_IO_THREAD_H_
