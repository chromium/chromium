// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_io_thread.h"

#include "base/logging.h"
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
  options.priority = base::ThreadPriority::REALTIME_AUDIO;
  CHECK(thread_.StartWithOptions(options));
}

AudioIoThread::~AudioIoThread() = default;

}  // namespace chromecast
