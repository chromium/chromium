// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/net/pipe_connection_posix.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/net/command_id.h"
#include "chrome/test/chromedriver/net/pipe_reader_posix.h"
#include "chrome/test/chromedriver/net/pipe_writer_posix.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace {

const int kMinReadBufferCapacity = 4096;

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
  explicit PipeReader(base::WeakPtr<PipeConnectionPosix> pipe_connection)
      : pipe_connection_(std::move(pipe_connection)),
        owning_sequence_(base::SequencedTaskRunner::GetCurrentDefault()),
        read_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()),
        thread_(
            std::make_unique<base::Thread>("PipeConnectionPosixReadThread")) {
    DETACH_FROM_THREAD(io_thread_checker_);
    read_buffer_->SetCapacity(kMinReadBufferCapacity);
  }

  ~PipeReader() = default;

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

  bool Start(base::ScopedPlatformFile read_fd) {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    is_connected_ = true;
    reader_.Bind(std::move(read_fd));
    reader_.DetachFromThread();
    if (!thread_->StartWithOptions(std::move(options))) {
      is_connected_ = false;
      return false;
    }
    thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&PipeReader::ReadLoopOnIOThread,
                                  base::Unretained(this)));
    return true;
  }

  void ReadLoopOnIOThread() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    while (true) {
      int rv = reader_.Read(
          read_buffer_.get(), read_buffer_->capacity(),
          base::BindOnce(&PipeReader::OnRead, base::Unretained(this), true));
      if (rv <= 0) {
        if (rv != net::ERR_IO_PENDING && !shutting_down_.IsSet()) {
          VLOG(logging::LOGGING_ERROR)
              << "Connection terminated while reading from pipe";
          base::AutoLock lock(lock_);
          is_connected_ = false;
          on_update_event_.Signal();
          owning_sequence_->PostTask(
              FROM_HERE,
              base::BindOnce(&PipeConnectionPosix::Shutdown, pipe_connection_));
        }
        break;
      }
      OnRead(false, rv);
    }
  }

  void OnRead(bool read_again, int rv) {
    if (rv <= 0) {
      if (rv != net::ERR_IO_PENDING && !shutting_down_.IsSet()) {
        VLOG(logging::LOGGING_ERROR)
            << "Connection terminated while reading from pipe";
        base::AutoLock lock(lock_);
        is_connected_ = false;
        on_update_event_.Signal();
        owning_sequence_->PostTask(
            FROM_HERE,
            base::BindOnce(&PipeConnectionPosix::Shutdown, pipe_connection_));
      }
      return;
    }
    base::span<uint8_t> buffer =
        read_buffer_->everything().first(base::checked_cast<size_t>(rv));
    auto iter = buffer.begin();
    while (iter != buffer.end()) {
      auto pos = std::find(iter, buffer.end(), '\0');
      next_message_.insert(next_message_.end(), iter, pos);
      if (pos != buffer.end()) {
        OnMessageReceivedOnIOThread(std::move(next_message_));
        next_message_ = std::string();
        iter = pos + 1;
      } else {
        break;
      }
    }
    if (read_again) {
      ReadLoopOnIOThread();
    }
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
  void ClosePipe() { DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_); }

  mutable base::Lock lock_;
  // Protected by |lock_|.
  bool is_connected_ = false;
  base::AtomicFlag shutting_down_;
  THREAD_CHECKER(session_thread_checker_);
  THREAD_CHECKER(io_thread_checker_);
  base::WeakPtr<PipeConnectionPosix> pipe_connection_;
  // Sequence where the instance was created.
  // The notifications about new data are emitted in this sequence.
  scoped_refptr<base::SequencedTaskRunner> owning_sequence_;
  base::ScopedPlatformFile read_file_;
  std::string next_message_;
  // Protected by |lock_|.
  std::list<std::string> received_queue_;
  // Protected by |lock_|.
  // Signaled when the pipe closes or a message is received.
  base::ConditionVariable on_update_event_{&lock_};
  // Protected by |lock_|.
  // Notifies that the queue is not empty.
  base::RepeatingClosure notify_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  PipeReaderPosix reader_;
  // Thread is the last member, to be destroyed first.
  // This ensures that there will be no races in the destructor.
  std::unique_ptr<base::Thread> thread_;
};

class PipeWriter {
 public:
  explicit PipeWriter(base::WeakPtr<PipeConnectionPosix> pipe_connection)
      : owning_sequence_(base::SequencedTaskRunner::GetCurrentDefault()),
        pipe_connection_(std::move(pipe_connection)),
        write_buffer_(base::MakeRefCounted<net::DrainableIOBuffer>(
            base::MakeRefCounted<net::IOBufferWithSize>(),
            0)),
        thread_(new base::Thread("PipeConnectionPosixWriteThread")) {
    DETACH_FROM_THREAD(io_thread_checker_);
  }

  virtual ~PipeWriter() = default;

  bool Start(base::ScopedPlatformFile write_fd) {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    is_connected_ = true;
    writer_.Bind(std::move(write_fd));
    writer_.DetachFromThread();
    if (!thread_->StartWithOptions(std::move(options))) {
      is_connected_ = false;
      return false;
    }
    return true;
  }

  bool IsConnected() {
    base::AutoLock lock(lock_);
    return is_connected_;
  }

  void WriteIntoPipeOnIOThread(std::string message,
                               bool* success,
                               base::WaitableEvent* event) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    // Trying to guess if the connection is still there
    bool is_connected = IsConnected();
    *success = is_connected;
    event->Signal();

    if (!is_connected) {
      return;
    }

    // enqueue with the trailing zero character
    queued_.insert(queued_.end(), message.c_str(),
                   message.c_str() + message.size() + 1);
    if (!write_buffer_->BytesRemaining()) {
      const size_t queued_size = queued_.size();
      write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::StringIOBuffer>(std::move(queued_)),
          queued_size);
      queued_ = std::string();
      WriteFromBuffer();
    }
  }

  void OnError(int rv) {
    base::AutoLock lock(lock_);
    is_connected_ = false;
    if (!shutting_down_.IsSet()) {
      VLOG(logging::LOGGING_ERROR) << "Could not write into pipe";
      owning_sequence_->PostTask(
          FROM_HERE,
          base::BindOnce(&PipeConnectionPosix::Shutdown, pipe_connection_));
    }
  }

  int WriteFromBuffer() {
    int sent = 0;
    while (write_buffer_->BytesRemaining()) {
      int rv = writer_.Write(
          write_buffer_.get(), write_buffer_->BytesRemaining(),
          base::BindOnce(&PipeWriter::OnWrite, base::Unretained(this)));
      if (rv < 0) {
        sent = rv;
        break;
      }
      sent += rv;
      write_buffer_->DidConsume(rv);
    }
    if (sent < 0 && sent != net::ERR_IO_PENDING) {
      OnError(sent);
    }
    return sent;
  }

  void OnWrite(int rv) {
    if (rv <= 0) {
      OnError(rv);
      return;
    }
    write_buffer_->DidConsume(rv);
    int sent = WriteFromBuffer();
    if (sent >= 0 && !write_buffer_->BytesRemaining() && !queued_.empty()) {
      const size_t queued_size = queued_.size();
      write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::StringIOBuffer>(std::move(queued_)),
          queued_size);
      queued_ = std::string();
      WriteFromBuffer();
    }
  }

  bool Write(std::string message) {
    DCHECK_CALLED_ON_VALID_THREAD(session_thread_checker_);
    base::TaskRunner* task_runner = thread_->task_runner().get();
    base::WaitableEvent event{base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED};
    bool success = false;
    if (!task_runner ||
        !task_runner->PostTask(
            FROM_HERE, base::BindOnce(&PipeWriter::WriteIntoPipeOnIOThread,
                                      base::Unretained(this),
                                      std::move(message), &success, &event))) {
      return false;
    }
    event.Wait();
    return success;
  }

  static void Shutdown(std::unique_ptr<PipeWriter> pipe_io) {
    if (!pipe_io) {
      return;
    }
    auto thread = std::move(pipe_io->thread_);
    pipe_io->shutting_down_.Set();
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
  THREAD_CHECKER(session_thread_checker_);
  THREAD_CHECKER(io_thread_checker_);
  // Sequence where the instance was created.
  // The notifications about new data are emitted in this sequence.
  scoped_refptr<base::SequencedTaskRunner> owning_sequence_;
  base::AtomicFlag shutting_down_;
  std::string queued_;
  base::WeakPtr<PipeConnectionPosix> pipe_connection_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  PipeWriterPosix writer_;
  // Thread is the last member, to be destroyed first.
  // This ensures that there will be no races in the destructor.
  std::unique_ptr<base::Thread> thread_;
};

PipeConnectionPosix::PipeConnectionPosix(base::ScopedPlatformFile read_file,
                                         base::ScopedPlatformFile write_file)
    : read_file_(std::move(read_file)), write_file_(std::move(write_file)) {
  pipe_reader_ = std::make_unique<PipeReader>(weak_factory_.GetWeakPtr());
  pipe_writer_ = std::make_unique<PipeWriter>(weak_factory_.GetWeakPtr());
  pipe_reader_->SetNotificationCallback(base::BindRepeating(
      &PipeConnectionPosix::SendNotification, weak_factory_.GetWeakPtr()));
}

PipeConnectionPosix::~PipeConnectionPosix() {
  Shutdown();
}

bool PipeConnectionPosix::IsConnected() {
  return pipe_reader_ && pipe_reader_->IsConnected() && pipe_writer_ &&
         pipe_writer_->IsConnected();
}

bool PipeConnectionPosix::Connect(const GURL& url) {
  if (connection_requested_) {
    return IsConnected();
  }
  connection_requested_ = true;
  bool reader_started = pipe_reader_->Start(std::move(read_file_));
  bool writer_started = pipe_writer_->Start(std::move(write_file_));
  read_file_ = base::ScopedPlatformFile();
  write_file_ = base::ScopedPlatformFile();
  if (!reader_started || !writer_started) {
    Shutdown();
    return false;
  }
  return true;
}

bool PipeConnectionPosix::Send(const std::string& message) {
  // pipe_writer_ is nullptr only after Shutdown
  if (!pipe_writer_) {
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

SyncWebSocket::StatusCode PipeConnectionPosix::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  if (!pipe_reader_) {
    return SyncWebSocket::StatusCode::kDisconnected;
  }
  return pipe_reader_->ReceiveNextMessage(message, timeout);
}

bool PipeConnectionPosix::HasNextMessage() {
  if (!pipe_reader_) {
    return false;
  }
  return pipe_reader_->HasNextMessage();
}

void PipeConnectionPosix::SetNotificationCallback(
    base::RepeatingClosure callback) {
  notify_ = std::move(callback);
}

void PipeConnectionPosix::Shutdown() {
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;

  PipeWriter::Shutdown(std::move(pipe_writer_));
  pipe_writer_ = std::unique_ptr<PipeWriter>();
  PipeReader::Shutdown(std::move(pipe_reader_));
  pipe_reader_ = std::unique_ptr<PipeReader>();
}

bool PipeConnectionPosix::IsNull() const {
  return !pipe_reader_ && !pipe_writer_;
}

void PipeConnectionPosix::SendNotification() {
  if (shutting_down_ || !notify_) {
    return;
  }
  notify_.Run();
}
