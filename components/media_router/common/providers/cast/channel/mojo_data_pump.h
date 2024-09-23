// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_MOJO_DATA_PUMP_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_MOJO_DATA_PUMP_H_

#include "base/memory/ref_counted.h"
#include "components/media_router/common/providers/cast/channel/cast_transport.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/completion_once_callback.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace cast_channel {

// Helper class to read from a mojo consumer handle and write to mojo producer
// handle.
class MojoDataPump : public CastTransportImpl::Channel {
 public:
  MojoDataPump(mojo::ScopedDataPipeConsumerHandle receive_stream,
               mojo::ScopedDataPipeProducerHandle send_stream);

  MojoDataPump(const MojoDataPump&) = delete;
  MojoDataPump& operator=(const MojoDataPump&) = delete;

  ~MojoDataPump() override;

  // CastTransportImpl::Channel implementation:
  void Read(net::IOBuffer* io_buffer,
            int count,
            net::CompletionOnceCallback callback) override;
  void Write(net::IOBuffer* io_buffer,
             int io_buffer_size,
             net::CompletionOnceCallback callback) override;

  // Returns whether a read is pending.
  bool HasPendingRead() const { return !read_callback_.is_null(); }

  // Returns whether a write is pending.
  bool HasPendingWrite() const { return !write_callback_.is_null(); }

 private:
  void OnReadComplete(int result);
  void StartWatching();
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);
  void SendMore(MojoResult result, const mojo::HandleSignalsState& state);

  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  mojo::SimpleWatcher send_stream_watcher_;

  net::CompletionOnceCallback read_callback_;
  net::CompletionOnceCallback write_callback_;
  scoped_refptr<net::IOBuffer> pending_read_buffer_;
  scoped_refptr<net::IOBuffer> pending_write_buffer_;
  size_t pending_write_buffer_size_ = 0;
  size_t read_size_ = 0;
};

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_MOJO_DATA_PUMP_H_
