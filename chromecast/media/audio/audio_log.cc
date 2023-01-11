// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_log.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace logging {

namespace {
constexpr int kBufferSize = 256;
constexpr int kMaxBuffers = 32;
}  // namespace

class AudioLogMessage::StreamBuf : public std::streambuf {
 public:
  StreamBuf() = default;

  StreamBuf(const StreamBuf&) = delete;
  StreamBuf& operator=(const StreamBuf&) = delete;

  void Initialize(const char* file, int line, LogSeverity severity) {
    file_ = file;
    line_ = line;
    severity_ = severity;
    setp(buffer_, buffer_ + kBufferSize);
  }

  void Log() {
    if (!file_) {
      // Cancelled.
      return;
    }

    ::logging::LogMessage message(file_, line_, severity_);
    int size = pptr() - pbase();
    message.stream().write(buffer_, size);
  }

  void Cancel() { file_ = nullptr; }

 private:
  const char* file_ = nullptr;
  int line_;
  LogSeverity severity_;
  char buffer_[kBufferSize];
};

class AudioLogMessage::BufferManager {
 public:
  static BufferManager* Get();

  void SetOwner(BufferManager* owner) { owner_ = owner; }

  void Setup() {
    base::AutoLock lock(lock_);
    if (ready_) {
      return;
    }

    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    dispose_callback_ = base::BindRepeating(
        &BufferManager::HandleDisposedBuffers, base::Unretained(this));

    for (int i = 0; i < kMaxBuffers; ++i) {
      free_buffers_[i] = &buffers_[i];
    }
    num_free_buffers_ = kMaxBuffers;
    ready_ = true;
  }

  AudioLogMessage::StreamBuf* GetBuffer(const char* file,
                                        int line,
                                        LogSeverity severity) {
    if (owner_) {
      return owner_->GetBuffer(file, line, severity);
    }

    AudioLogMessage::StreamBuf* buffer;
    {
      base::AutoLock lock(lock_);
      if (num_free_buffers_ == 0) {
        ++num_missing_buffers_;
        return nullptr;
      }

      --num_free_buffers_;
      buffer = free_buffers_[num_free_buffers_];
    }
    buffer->Initialize(file, line, severity);
    return buffer;
  }

  void Dispose(AudioLogMessage::StreamBuf* buffer) {
    if (!buffer) {
      return;
    }

    if (owner_) {
      return owner_->Dispose(buffer);
    }

    {
      base::AutoLock lock(lock_);
      DCHECK_LT(num_disposed_buffers_, kMaxBuffers);
      disposed_buffers_[num_disposed_buffers_] = buffer;
      ++num_disposed_buffers_;
    }
    DCHECK(task_runner_);
    task_runner_->PostTask(FROM_HERE, dispose_callback_);
  }

 private:
  void HandleDisposedBuffers() {
    AudioLogMessage::StreamBuf* buffers[kMaxBuffers];
    int num_buffers;
    int num_missing;
    {
      base::AutoLock lock(lock_);
      std::copy_n(disposed_buffers_, num_disposed_buffers_, buffers);
      num_buffers = num_disposed_buffers_;
      num_disposed_buffers_ = 0;

      num_missing = num_missing_buffers_;
      num_missing_buffers_ = 0;
    }

    for (int i = 0; i < num_buffers; ++i) {
      buffers[i]->Log();
      {
        base::AutoLock lock(lock_);
        free_buffers_[num_free_buffers_] = buffers[i];
        ++num_free_buffers_;
      }
    }

    LOG_IF(ERROR, num_missing > 0)
        << num_missing << " log messages lost due to lack of buffers";
  }

  AudioLogMessage::StreamBuf buffers_[kMaxBuffers];
  BufferManager* owner_ = nullptr;

  base::Lock lock_;
  bool ready_ GUARDED_BY(lock_) = false;
  AudioLogMessage::StreamBuf* free_buffers_[kMaxBuffers] GUARDED_BY(lock_);
  int num_free_buffers_ GUARDED_BY(lock_) = 0;

  AudioLogMessage::StreamBuf* disposed_buffers_[kMaxBuffers] GUARDED_BY(lock_);
  int num_disposed_buffers_ GUARDED_BY(lock_) = 0;

  int num_missing_buffers_ GUARDED_BY(lock_) = 0;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::RepeatingClosure dispose_callback_;
};

// static
AudioLogMessage::BufferManager* AudioLogMessage::BufferManager::Get() {
  static base::NoDestructor<BufferManager> g_buffer_manager;
  return g_buffer_manager.get();
}

// static
AudioLogMessage::BufferManager* AudioLogMessage::GetBufferManager() {
  return BufferManager::Get();
}

AudioLogMessage::AudioLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity)
    : buffer_(BufferManager::Get()->GetBuffer(file, line, severity)),
      stream_(buffer_) {}

AudioLogMessage::~AudioLogMessage() {
  BufferManager::Get()->Dispose(buffer_);
}

void AudioLogMessage::Cancel() {
  if (buffer_) {
    buffer_->Cancel();
  }
}

void InitializeAudioLog() {
  AudioLogMessage::BufferManager::Get()->Setup();
}

void InitializeShlibAudioLog(AudioLogMessage::BufferManager* manager) {
  AudioLogMessage::BufferManager::Get()->SetOwner(manager);
}

}  // namespace logging
