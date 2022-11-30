// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_io_thread.h"

#include "base/check.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"

namespace chromecast {

// static
AudioIoThread* AudioIoThread::Get() {
  static base::NoDestructor<AudioIoThread> instance;
  return instance.get();
}

AudioIoThread::AudioIoThread() : thread_("AudioIO") {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  options.thread_type = base::ThreadType::kRealtimeAudio;
  CHECK(thread_.StartWithOptions(std::move(options)));
}

AudioIoThread::~AudioIoThread() = default;

}  // namespace chromecast
