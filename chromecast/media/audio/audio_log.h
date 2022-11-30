// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_LOG_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_LOG_H_

#include <ostream>

#include "base/logging.h"

namespace logging {

#define AUDIO_LOG_STREAM(severity) \
  COMPACT_GOOGLE_LOG_EX_##severity(AudioLogMessage).stream()

#define AUDIO_LOG(severity) \
  LAZY_STREAM(AUDIO_LOG_STREAM(severity), LOG_IS_ON(severity))

#define AUDIO_LOG_IF(severity, condition) \
  LAZY_STREAM(AUDIO_LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

class AudioLogMessage {
 public:
  class BufferManager;
  static BufferManager* GetBufferManager();

  AudioLogMessage(const char* file, int line, LogSeverity severity);
  ~AudioLogMessage();

  AudioLogMessage(const AudioLogMessage&) = delete;
  AudioLogMessage& operator=(const AudioLogMessage&) = delete;

  std::ostream& stream() { return stream_; }

  // Cancels any log output for this message.
  void Cancel();

 private:
  class StreamBuf;

  StreamBuf* buffer_;
  std::ostream stream_;
};

// Should be called on a lower-priority thread. Actual output of log messages
// will be done on this thread. Note that any use of AudioLogMessage prior to
// InitializeAudioLog() will not produce any output.
void InitializeAudioLog();

// Initializes audio log with an existing buffer manager. May be called from any
// thread (the thread that originally called InitializeAudioLog() to create the
// buffer manager will be used for log output). Useful for logging in shlibs.
void InitializeShlibAudioLog(AudioLogMessage::BufferManager* manager);

}  // namespace logging

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_LOG_H_
