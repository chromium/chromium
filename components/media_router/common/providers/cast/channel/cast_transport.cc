// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_transport.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/media_router/common/providers/cast/channel/cast_framer.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "net/base/net_errors.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

#define VLOG_WITH_CONNECTION(level) \
  VLOG(level) << "[" << ip_endpoint_.ToString() << ", auth=SSL_VERIFIED] "

namespace cast_channel {

namespace {

#if DCHECK_IS_ON()
// Used to filter out PING and PONG message from logs, since there are a lot of
// them and they're not interesting.
bool IsPingPong(const CastMessage& message) {
  return message.has_payload_utf8() &&
         (message.payload_utf8() == R"({"type":"PING"})" ||
          message.payload_utf8() == R"({"type":"PONG"})");
}
#endif  // DCHECK_IS_ON()

}  // namespace

CastTransport::~CastTransport() = default;
CastTransport::Delegate::~Delegate() = default;
CastTransportImpl::Channel::~Channel() = default;

CastTransportImpl::CastTransportImpl(Channel* channel,
                                     int channel_id,
                                     const net::IPEndPoint& ip_endpoint,
                                     scoped_refptr<Logger> logger)
    : started_(false),
      channel_(channel),
      write_state_(WriteState::IDLE),
      read_state_(ReadState::READ),
      error_state_(ChannelError::NONE),
      channel_id_(channel_id),
      ip_endpoint_(ip_endpoint),
      logger_(logger) {
  // Buffer is reused across messages to minimize unnecessary buffer
  // [re]allocations.
  read_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
  read_buffer_->SetCapacity(MessageFramer::MessageHeader::max_message_size());
  framer_ = std::make_unique<MessageFramer>(read_buffer_);
}

CastTransportImpl::~CastTransportImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushWriteQueue();
}

bool CastTransportImpl::IsTerminalWriteState(WriteState write_state) {
  return write_state == WriteState::WRITE_ERROR ||
         write_state == WriteState::IDLE;
}

bool CastTransportImpl::IsTerminalReadState(ReadState read_state) {
  return read_state == ReadState::READ_ERROR;
}

void CastTransportImpl::SetReadDelegate(std::unique_ptr<Delegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  delegate_ = std::move(delegate);
  if (started_) {
    delegate_->Start();
  }
}

void CastTransportImpl::FlushWriteQueue() {
  for (; !write_queue_.empty(); write_queue_.pop()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(write_queue_.front().callback),
                                  net::ERR_FAILED));
  }
}

void CastTransportImpl::SendMessage(const CastMessage& message,
                                    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsCastMessageValid(message));
  DVLOG_IF(1, !IsPingPong(message)) << "Sending: " << message;
  std::string serialized_message;
  if (!MessageFramer::Serialize(message, &serialized_message)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::ERR_FAILED));
    return;
  }

  write_queue_.emplace(message.namespace_(), serialized_message,
                       std::move(callback));
  if (write_state_ == WriteState::IDLE) {
    SetWriteState(WriteState::WRITE);
    OnWriteResult(net::OK);
  }
}

CastTransportImpl::WriteRequest::WriteRequest(
    const std::string& namespace_,
    const std::string& payload,
    net::CompletionOnceCallback callback)
    : message_namespace(namespace_), callback(std::move(callback)) {
  VLOG(2) << "WriteRequest size: " << payload.size();
  io_buffer = base::MakeRefCounted<net::DrainableIOBuffer>(
      base::MakeRefCounted<net::StringIOBuffer>(payload), payload.size());
}

CastTransportImpl::WriteRequest::WriteRequest(WriteRequest&& other) = default;

CastTransportImpl::WriteRequest::~WriteRequest() = default;

void CastTransportImpl::SetReadState(ReadState read_state) {
  if (read_state_ != read_state)
    read_state_ = read_state;
}

void CastTransportImpl::SetWriteState(WriteState write_state) {
  if (write_state_ != write_state)
    write_state_ = write_state;
}

void CastTransportImpl::SetErrorState(ChannelError error_state) {
  VLOG_WITH_CONNECTION(2) << "SetErrorState: "
                          << ::cast_channel::ChannelErrorToString(error_state);
  error_state_ = error_state;
}

void CastTransportImpl::OnWriteResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(WriteState::IDLE, write_state_);
  if (write_queue_.empty()) {
    SetWriteState(WriteState::IDLE);
    return;
  }

  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    VLOG_WITH_CONNECTION(2)
        << "OnWriteResult (state=" << AsInteger(write_state_) << ", "
        << "result=" << rv << ", "
        << "queue size=" << write_queue_.size() << ")";

    WriteState state = write_state_;
    write_state_ = WriteState::UNKNOWN;
    switch (state) {
      case WriteState::WRITE:
        rv = DoWrite();
        break;
      case WriteState::WRITE_COMPLETE:
        rv = DoWriteComplete(rv);
        break;
      case WriteState::DO_CALLBACK:
        rv = DoWriteCallback();
        break;
      case WriteState::HANDLE_ERROR:
        rv = DoWriteHandleError(rv);
        DCHECK_EQ(WriteState::WRITE_ERROR, write_state_);
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unknown state in write state machine: " << AsInteger(state);
        SetWriteState(WriteState::WRITE_ERROR);
        SetErrorState(ChannelError::UNKNOWN);
        rv = net::ERR_FAILED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && !IsTerminalWriteState(write_state_));

  if (write_state_ == WriteState::WRITE_ERROR) {
    FlushWriteQueue();
    DCHECK_NE(ChannelError::NONE, error_state_);
    VLOG_WITH_CONNECTION(2) << "Sending OnError().";
    delegate_->OnError(error_state_);
  }
}

int CastTransportImpl::DoWrite() {
  DCHECK(!write_queue_.empty());
  net::DrainableIOBuffer* io_buffer = write_queue_.front().io_buffer.get();

  VLOG_WITH_CONNECTION(2) << "WriteData byte_count = " << io_buffer->size()
                          << " bytes_written " << io_buffer->BytesConsumed();

  SetWriteState(WriteState::WRITE_COMPLETE);

  // TODO(mfoltz): Improve APIs for CastTransportImpl::Channel::{Read|Write} so
  // that they don't expect raw pointers but handle movable parameters instead.
  channel_->Write(io_buffer, io_buffer->BytesRemaining(),
                  base::BindOnce(&CastTransportImpl::OnWriteResult,
                                 base::Unretained(this)));
  return net::ERR_IO_PENDING;
}

int CastTransportImpl::DoWriteComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteComplete result=" << result;
  DCHECK(!write_queue_.empty());
  if (result <= 0) {  // NOTE that 0 also indicates an error
    logger_->LogSocketEventWithRv(channel_id_, ChannelEvent::SOCKET_WRITE,
                                  result);
    SetErrorState(ChannelError::CAST_SOCKET_ERROR);
    SetWriteState(WriteState::HANDLE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  // Some bytes were successfully written
  net::DrainableIOBuffer* io_buffer = write_queue_.front().io_buffer.get();
  io_buffer->DidConsume(result);
  if (io_buffer->BytesRemaining() == 0) {  // Message fully sent
    SetWriteState(WriteState::DO_CALLBACK);
  } else {
    SetWriteState(WriteState::WRITE);
  }

  return net::OK;
}

int CastTransportImpl::DoWriteCallback() {
  VLOG_WITH_CONNECTION(2) << "DoWriteCallback";
  DCHECK(!write_queue_.empty());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(write_queue_.front().callback), net::OK));
  write_queue_.pop();

  if (write_queue_.empty()) {
    SetWriteState(WriteState::IDLE);
  } else {
    SetWriteState(WriteState::WRITE);
  }

  return net::OK;
}

int CastTransportImpl::DoWriteHandleError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoWriteHandleError result=" << result;
  DCHECK_NE(ChannelError::NONE, error_state_);
  DCHECK_LT(result, 0);
  SetWriteState(WriteState::WRITE_ERROR);
  return net::ERR_FAILED;
}

void CastTransportImpl::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!started_);
  DCHECK_EQ(ReadState::READ, read_state_);
  DCHECK(delegate_) << "Read delegate must be set prior to calling Start()";
  started_ = true;
  delegate_->Start();
  SetReadState(ReadState::READ);

  // Start the read state machine.
  OnReadResult(net::OK);
}

void CastTransportImpl::OnReadResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Network operations can either finish synchronously or asynchronously.
  // This method executes the state machine transitions in a loop so that
  // write state transitions happen even when network operations finish
  // synchronously.
  int rv = result;
  do {
    VLOG_WITH_CONNECTION(2) << "OnReadResult(state=" << AsInteger(read_state_)
                            << ", result=" << rv << ")";
    ReadState state = read_state_;
    read_state_ = ReadState::UNKNOWN;

    switch (state) {
      case ReadState::READ:
        rv = DoRead();
        break;
      case ReadState::READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case ReadState::DO_CALLBACK:
        rv = DoReadCallback();
        break;
      case ReadState::HANDLE_ERROR:
        rv = DoReadHandleError(rv);
        DCHECK_EQ(read_state_, ReadState::READ_ERROR);
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unknown state in read state machine: " << AsInteger(state);
        SetReadState(ReadState::READ_ERROR);
        SetErrorState(ChannelError::UNKNOWN);
        rv = net::ERR_FAILED;
        break;
    }
  } while (rv != net::ERR_IO_PENDING && !IsTerminalReadState(read_state_));

  if (IsTerminalReadState(read_state_)) {
    DCHECK_EQ(ReadState::READ_ERROR, read_state_);
    VLOG_WITH_CONNECTION(2) << "Sending OnError().";
    delegate_->OnError(error_state_);
  }
}

int CastTransportImpl::DoRead() {
  VLOG_WITH_CONNECTION(2) << "DoRead";
  SetReadState(ReadState::READ_COMPLETE);

  // Determine how many bytes need to be read.
  size_t num_bytes_to_read = framer_->BytesRequested();
  DCHECK_GT(num_bytes_to_read, 0u);

  // Read up to num_bytes_to_read into |current_read_buffer_|.
  channel_->Read(
      read_buffer_.get(), base::checked_cast<uint32_t>(num_bytes_to_read),
      base::BindOnce(&CastTransportImpl::OnReadResult, base::Unretained(this)));
  return net::ERR_IO_PENDING;
}

int CastTransportImpl::DoReadComplete(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadComplete result = " << result;
  if (result <= 0) {
    logger_->LogSocketEventWithRv(channel_id_, ChannelEvent::SOCKET_READ,
                                  result);
    VLOG_WITH_CONNECTION(1) << "Read error, peer closed the socket.";
    SetErrorState(ChannelError::CAST_SOCKET_ERROR);
    SetReadState(ReadState::HANDLE_ERROR);
    return result == 0 ? net::ERR_FAILED : result;
  }

  size_t message_size;
  DCHECK(!current_message_);
  ChannelError framing_error;
  current_message_ = framer_->Ingest(result, &message_size, &framing_error);
  if (current_message_.get() && (framing_error == ChannelError::NONE)) {
    DCHECK_GT(message_size, static_cast<size_t>(0));
    SetReadState(ReadState::DO_CALLBACK);
  } else if (framing_error != ChannelError::NONE) {
    DCHECK(!current_message_);
    SetErrorState(ChannelError::INVALID_MESSAGE);
    SetReadState(ReadState::HANDLE_ERROR);
  } else {
    DCHECK(!current_message_);
    SetReadState(ReadState::READ);
  }
  return net::OK;
}

int CastTransportImpl::DoReadCallback() {
  VLOG_WITH_CONNECTION(2) << "DoReadCallback";
  if (!IsCastMessageValid(*current_message_)) {
    SetReadState(ReadState::HANDLE_ERROR);
    SetErrorState(ChannelError::INVALID_MESSAGE);
    return net::ERR_INVALID_RESPONSE;
  }
  SetReadState(ReadState::READ);
  DVLOG_IF(1, !IsPingPong(*current_message_))
      << "Received: " << *current_message_;
  delegate_->OnMessage(*current_message_);
  current_message_.reset();
  return net::OK;
}

int CastTransportImpl::DoReadHandleError(int result) {
  VLOG_WITH_CONNECTION(2) << "DoReadHandleError";
  DCHECK_NE(ChannelError::NONE, error_state_);
  DCHECK_LE(result, 0);
  SetReadState(ReadState::READ_ERROR);
  return net::ERR_FAILED;
}

}  // namespace cast_channel
