// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/mojo_data_pump.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace cast_channel {

MojoDataPump::MojoDataPump(mojo::ScopedDataPipeConsumerHandle receive_stream,
                           mojo::ScopedDataPipeProducerHandle send_stream)
    : receive_stream_(std::move(receive_stream)),
      receive_stream_watcher_(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      send_stream_(std::move(send_stream)),
      send_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  DCHECK(receive_stream_.is_valid());
  DCHECK(send_stream_.is_valid());
  receive_stream_watcher_.Watch(
      receive_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MojoDataPump::ReceiveMore, base::Unretained(this)));
  send_stream_watcher_.Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MojoDataPump::SendMore, base::Unretained(this)));
}

MojoDataPump::~MojoDataPump() = default;

void MojoDataPump::Read(net::IOBuffer* io_buffer,
                        int count,
                        net::CompletionOnceCallback callback) {
  DCHECK(callback);
  DCHECK(!read_callback_);

  if (count <= 0) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
    return;
  }
  // Safe because of the `count <= 0` check above.
  read_size_ = base::checked_cast<size_t>(count);

  pending_read_buffer_ = io_buffer;
  read_callback_ = std::move(callback);
  receive_stream_watcher_.ArmOrNotify();
}

void MojoDataPump::Write(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         net::CompletionOnceCallback callback) {
  DCHECK(callback);
  DCHECK(!write_callback_);

  write_callback_ = std::move(callback);
  pending_write_buffer_ = io_buffer;
  pending_write_buffer_size_ = base::checked_cast<size_t>(io_buffer_size);
  send_stream_watcher_.ArmOrNotify();
}

void MojoDataPump::ReceiveMore(MojoResult result,
                               const mojo::HandleSignalsState& state) {
  DCHECK(read_callback_);
  DCHECK_NE(0u, read_size_);

  size_t num_bytes = read_size_;

  if (result == MOJO_RESULT_OK) {
    result = receive_stream_->ReadData(
        MOJO_READ_DATA_FLAG_NONE, pending_read_buffer_->span().first(num_bytes),
        num_bytes);
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    receive_stream_watcher_.ArmOrNotify();
    return;
  }
  read_size_ = 0;
  if (result != MOJO_RESULT_OK) {
    std::move(read_callback_).Run(net::ERR_FAILED);
    return;
  }
  std::move(read_callback_).Run(base::checked_cast<int>(num_bytes));
}

void MojoDataPump::SendMore(MojoResult result,
                            const mojo::HandleSignalsState& state) {
  DCHECK(write_callback_);

  size_t actually_written_bytes = 0;
  if (result == MOJO_RESULT_OK) {
    base::span<const uint8_t> data_to_write =
        pending_write_buffer_->span().first(pending_write_buffer_size_);
    result = send_stream_->WriteData(data_to_write, MOJO_WRITE_DATA_FLAG_NONE,
                                     actually_written_bytes);
  }
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_stream_watcher_.ArmOrNotify();
    return;
  }
  pending_write_buffer_ = nullptr;
  pending_write_buffer_size_ = 0;
  if (result != MOJO_RESULT_OK) {
    std::move(write_callback_).Run(net::ERR_FAILED);
    return;
  }
  std::move(write_callback_)
      .Run(base::checked_cast<int>(actually_written_bytes));
}

}  // namespace cast_channel
