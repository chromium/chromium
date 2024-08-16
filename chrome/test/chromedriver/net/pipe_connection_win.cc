// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/net/pipe_connection_win.h"

#include <windows.h>

#include <io.h>
#include <stdlib.h>

#include <list>
#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/net/command_id.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "net/base/io_buffer.h"

namespace {

const size_t kWritePacketSize = 1 << 16;
const int kMinReadBufferCapacity = 4096;
const int kMaxReadBufferCapacity = 100 * 1024 * 1024;  // 100Mb

void DetermineRecipient(const std::string& message,
                        bool* send_to_chromedriver) {
  std::optional<base::Value> message_value =
      base::JSONReader::Read(message, base::JSON_REPLACE_INVALID_CHARACTERS);
  base::Value::Dict* message_dict =
      message_value ? message_value->GetIfDict() : nullptr;
  if (!message_dict) {
    *send_to_chromedriver = true;
    return;
  }
  base::Value* id = message_dict->Find("id");
  *send_to_chromedriver =
      id == nullptr ||
      (id->is_int() && CommandId::IsChromeDriverCommandId(id->GetInt()));
}

}  // namespace

class PipeReader {
 public:
  explicit PipeReader(base::WeakPtr<PipeConnectionWin> pipe_connection)
      : pipe_connection_(std::move(pipe_connection)),
        owning_sequence_(base::SequencedTaskRunner::GetCurrentDefault()),
        read_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()),
        thread_(std::make_unique<base::Thread>("PipeConnectionWinReadThread")) {
    DETACH_FROM_THREAD(io_thread_checker_);
    read_buffer_->SetCapacity(kMinReadBufferCapacity);
  }

  ~PipeReader() = default;

  bool Start(base::ScopedPlatformFile read_file) {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    is_connected_ = true;
    read_file_ = std::move(read_file);
    if (!thread_->StartWithOptions(std::move(options))) {
      is_connected_ = false;
      return false;
    }
    thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&PipeReader::ReadLoopOnIOThread,
                                  base::Unretained(this)));
    return true;
  }

  bool IsConnected() const {
    base::AutoLock lock(lock_);
    return is_connected_;
  }

  void SetNotificationCallback(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::AutoLock lock(lock_);
    notify_ = std::move(callback);
  }

  bool HasNextMessage() const {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::AutoLock lock(lock_);
    return !received_queue_.empty();
  }

  SyncWebSocket::StatusCode ReceiveNextMessage(std::string* message,
                                               const Timeout& timeout) {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::AutoLock lock(lock_);
    while (received_queue_.empty() && is_connected_) {
      base::TimeDelta next_wait = timeout.GetRemainingTime();
      if (next_wait <= base::TimeDelta()) {
        return SyncWebSocket::StatusCode::kTimeout;
      }
      on_update_event_.TimedWait(next_wait);
    }
    if (!received_queue_.empty()) {
      *message = received_queue_.front();
      received_queue_.pop_front();
      return SyncWebSocket::StatusCode::kOk;
    }
    DCHECK(!is_connected_);
    return SyncWebSocket::StatusCode::kDisconnected;
  }

  void ReadLoopOnIOThread() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    while (true) {
      if (read_buffer_->RemainingCapacity() == 0) {
        if (read_buffer_->capacity() >= kMaxReadBufferCapacity) {
          VLOG(logging::LOGGING_ERROR)
              << "Connection closed, not enough capacity";
          break;
        }
        read_buffer_->SetCapacity(2 * read_buffer_->capacity());
      }

      size_t bytes_read = ReadBytes(read_buffer_->data(),
                                    read_buffer_->RemainingCapacity(), false);
      if (!bytes_read) {
        break;
      }
      read_buffer_->set_offset(read_buffer_->offset() + bytes_read);

      // Go over the last read chunk, look for \0, extract messages.
      int offset = 0;
      for (int i = read_buffer_->offset() - bytes_read;
           i < read_buffer_->offset(); ++i) {
        if (read_buffer_->everything()[i] == '\0') {
          OnMessageReceivedOnIOThread(
              std::string(base::as_string_view(read_buffer_->everything())
                              .substr(offset, i - offset)));
          offset = i + 1;
        }
      }
      if (offset) {
        base::span<const uint8_t> subspan =
            read_buffer_->span_before_offset().subspan(offset);
        read_buffer_->everything().copy_prefix_from(subspan);
        read_buffer_->set_offset(subspan.size());
        int new_capacity = std::max(
            kMinReadBufferCapacity,
            std::min(read_buffer_->offset() * 2, read_buffer_->capacity()));
        if (new_capacity != read_buffer_->capacity()) {
          read_buffer_->SetCapacity(new_capacity);
        }
      }
    }
    owning_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&PipeConnectionWin::Shutdown, pipe_connection_));
  }

  size_t ReadBytes(char* buffer, size_t size, bool exact_size) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    size_t bytes_read = 0;
    base::PlatformFile file = base::kInvalidPlatformFile;
    {
      base::AutoLock lock(lock_);
      file = read_file_.get();
    }
    while (bytes_read < size) {
      DWORD size_read = 0;
      bool had_error = !ReadFile(file, buffer + bytes_read, size - bytes_read,
                                 &size_read, nullptr);
      if (had_error) {
        if (!shutting_down_.IsSet()) {
          VLOG(logging::LOGGING_ERROR)
              << "Connection terminated while reading from pipe";
          base::AutoLock lock(lock_);
          is_connected_ = false;
          on_update_event_.Signal();
        }
        return 0;
      }
      bytes_read += size_read;
      if (!exact_size) {
        break;
      }
    }
    return bytes_read;
  }

  void OnMessageReceivedOnIOThread(std::string message) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

    base::AutoLock lock(lock_);

    bool notification_is_needed = false;
    bool send_to_chromedriver;

    DetermineRecipient(message, &send_to_chromedriver);
    if (send_to_chromedriver) {
      notification_is_needed = received_queue_.empty();
      received_queue_.push_back(message);
    }
    on_update_event_.Signal();

    // The notification can be emitted sporadically but we explicitly allow
    // this.
    if (notification_is_needed && notify_) {
      owning_sequence_->PostTask(FROM_HERE, notify_);
    }
  }

  static void Shutdown(std::unique_ptr<PipeReader> pipe_io) {
    if (!pipe_io) {
      return;
    }
    auto thread = std::move(pipe_io->thread_);
    pipe_io->shutting_down_.Set();
    pipe_io->ClosePipe();
    // Post self destruction on the custom thread if it's running.
    if (thread->task_runner()) {
      thread->task_runner()->DeleteSoon(FROM_HERE, std::move(pipe_io));
    } else {
      pipe_io.reset();
    }
  }

 protected:
  // Concurrently discard the pipe handles to successfully join threads.
  void ClosePipe() {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::AutoLock lock(lock_);
    // Cancel pending synchronous read.
    CancelIoEx(read_file_.get(), nullptr);
    read_file_ = base::ScopedPlatformFile();
  }

  mutable base::Lock lock_;
  // Protected by |lock_|.
  bool is_connected_ = false;
  base::AtomicFlag shutting_down_;
  THREAD_CHECKER(session_thread_checker_);
  THREAD_CHECKER(io_thread_checker_);
  base::WeakPtr<PipeConnectionWin> pipe_connection_;
  // Sequence where the instance was created.
  // The notifications about new data are emitted in this sequence.
  scoped_refptr<base::SequencedTaskRunner> owning_sequence_;
  base::ScopedPlatformFile read_file_;
  // Protected by |lock_|.
  std::list<std::string> received_queue_;
  // Protected by |lock_|.
  // Signaled when the pipe closes or a message is received.
  base::ConditionVariable on_update_event_{&lock_};
  // Protected by |lock_|.
  // Notifies that the queue is not empty.
  base::RepeatingClosure notify_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  // Thread is the last member, to be destroyed first.
  // This ensures that there will be no races in the destructor.
  std::unique_ptr<base::Thread> thread_;
};

class PipeWriter {
 public:
  explicit PipeWriter(base::WeakPtr<PipeConnectionWin> pipe_connection)
      : owning_sequence_(base::SequencedTaskRunner::GetCurrentDefault()),
        pipe_connection_(std::move(pipe_connection)),
        thread_(new base::Thread("PipeConnectionWinWriteThread")) {
    DETACH_FROM_THREAD(io_thread_checker_);
  }

  virtual ~PipeWriter() = default;

  bool IsConnected() {
    base::AutoLock lock(lock_);
    return is_connected_;
  }

  void WriteIntoPipeOnIOThread(std::string message,
                               bool* success,
                               base::WaitableEvent* event) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    // Trying to guess if the connection is still there
    {
      base::AutoLock lock(lock_);
      *success = is_connected_;
    }
    event->Signal();
    // The rest is done without blocking the session thread
    bool ok = WriteBytesOnIOThread(message.data(), message.size());
    ok = ok && WriteBytesOnIOThread("\0", 1);

    if (!ok) {
      owning_sequence_->PostTask(
          FROM_HERE,
          base::BindOnce(&PipeConnectionWin::Shutdown, pipe_connection_));
    }
  }

  bool Start(base::ScopedPlatformFile write_file) {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    is_connected_ = true;
    write_file_ = std::move(write_file);
    if (!thread_->StartWithOptions(std::move(options))) {
      is_connected_ = false;
      return false;
    }
    return true;
  }

  bool Write(std::string message) {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    // This is mostly for the case when the thread is not yet / no longer
    // running. Otherwise PostTask would crash.
    if (!IsConnected()) {
      return false;
    }
    base::TaskRunner* task_runner = thread_->task_runner().get();
    base::WaitableEvent event{base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED};
    bool success = false;
    if (!task_runner->PostTask(
            FROM_HERE, base::BindOnce(&PipeWriter::WriteIntoPipeOnIOThread,
                                      base::Unretained(this),
                                      std::move(message), &success, &event))) {
      return false;
    }
    event.Wait();
    return success;
  }

  void ClosePipe() {
    base::AutoLock lock(lock_);
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    write_file_ = base::ScopedPlatformFile();
  }

  bool WriteBytesOnIOThread(const char* bytes, size_t size) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    size_t total_written = 0;
    base::PlatformFile file = base::kInvalidPlatformFile;
    {
      base::AutoLock lock(lock_);
      file = write_file_.get();
    }
    while (total_written < size) {
      size_t length = size - total_written;
      if (length > kWritePacketSize) {
        length = kWritePacketSize;
      }
      DWORD bytes_written = 0;
      bool had_error =
          !WriteFile(file, bytes + total_written, static_cast<DWORD>(length),
                     &bytes_written, nullptr);
      if (had_error) {
        if (!shutting_down_.IsSet()) {
          VLOG(logging::LOGGING_ERROR) << "Could not write into pipe";
        }
        base::AutoLock lock(lock_);
        is_connected_ = false;
        return false;
      }
      total_written += bytes_written;
    }
    return true;
  }

  static void Shutdown(std::unique_ptr<PipeWriter> pipe_io) {
    if (!pipe_io) {
      return;
    }
    auto thread = std::move(pipe_io->thread_);
    pipe_io->shutting_down_.Set();
    pipe_io->ClosePipe();
    // Post self destruction on the custom thread if it's running.
    if (thread->task_runner()) {
      thread->task_runner()->DeleteSoon(FROM_HERE, std::move(pipe_io));
    } else {
      pipe_io.reset();
    }
  }

 private:
  base::Lock lock_;
  // Protected by |lock_|.
  bool is_connected_ = false;
  // Sequence where the instance was created.
  // The notifications about new data are emitted in this sequence.
  scoped_refptr<base::SequencedTaskRunner> owning_sequence_;
  base::AtomicFlag shutting_down_;
  THREAD_CHECKER(session_thread_checker_);
  THREAD_CHECKER(io_thread_checker_);
  base::WeakPtr<PipeConnectionWin> pipe_connection_;
  base::ScopedPlatformFile write_file_;
  // Thread is the last member, to be destroyed first.
  // This ensures that there will be no races in the destructor.
  std::unique_ptr<base::Thread> thread_;
};

PipeConnectionWin::PipeConnectionWin(base::ScopedPlatformFile read_file,
                                     base::ScopedPlatformFile write_file)
    : read_file_(std::move(read_file)), write_file_(std::move(write_file)) {
  pipe_reader_ = std::make_unique<PipeReader>(weak_factory_.GetWeakPtr());
  pipe_writer_ = std::make_unique<PipeWriter>(weak_factory_.GetWeakPtr());
  pipe_reader_->SetNotificationCallback(base::BindRepeating(
      &PipeConnectionWin::SendNotification, weak_factory_.GetWeakPtr()));
}

PipeConnectionWin::~PipeConnectionWin() {
  Shutdown();
}

bool PipeConnectionWin::IsConnected() {
  return pipe_reader_ && pipe_reader_->IsConnected() && pipe_writer_ &&
         pipe_writer_->IsConnected();
}

bool PipeConnectionWin::Connect(const GURL& url) {
  if (connection_requested_) {
    return IsConnected();
  }
  connection_requested_ = true;
  if (!pipe_reader_ || !pipe_writer_) {
    return false;
  }
  bool reader_started = pipe_reader_->Start(std::move(read_file_));
  bool writer_started = pipe_writer_->Start(std::move(write_file_));
  if (!reader_started || !writer_started) {
    Shutdown();
    return false;
  }
  return true;
}

bool PipeConnectionWin::Send(const std::string& message) {
  // If the remote reading end is closed the local end should stop sending
  // messages.
  if (!pipe_writer_ || !pipe_writer_->IsConnected()) {
    return false;
  }
  // If the remote writing end is closed, this is a signal for the local end
  // for shutting down the communication.
  if (pipe_reader_ && !pipe_reader_->IsConnected()) {
    Shutdown();
    return false;
  }
  return pipe_writer_->Write(message);
}

SyncWebSocket::StatusCode PipeConnectionWin::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  if (!pipe_reader_) {
    return SyncWebSocket::StatusCode::kDisconnected;
  }
  return pipe_reader_->ReceiveNextMessage(message, timeout);
}

bool PipeConnectionWin::HasNextMessage() {
  if (!pipe_reader_) {
    return false;
  }
  return pipe_reader_->HasNextMessage();
}

void PipeConnectionWin::SetNotificationCallback(
    base::RepeatingClosure callback) {
  notify_ = std::move(callback);
}

void PipeConnectionWin::Shutdown() {
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;

  PipeWriter::Shutdown(std::move(pipe_writer_));
  PipeReader::Shutdown(std::move(pipe_reader_));
}

bool PipeConnectionWin::IsNull() const {
  return !pipe_reader_ && !pipe_writer_;
}

void PipeConnectionWin::SendNotification() {
  if (shutting_down_ || !notify_) {
    return;
  }
  notify_.Run();
}
