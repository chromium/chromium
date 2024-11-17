// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_STREAM_IMPL_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_STREAM_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/nearby/src/internal/platform/input_stream.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace nearby::chrome {

// An implementation of a Nearby Connections InputStream that reads from the
// Mojo DataPipe, |receive_stream|, passed into the constructor by the specified
// |medium| implementation.
//
// Because the InputStream interface is synchronous but the DataPipe interface
// is asynchronous, we block Read() on the calling thread while waiting for the
// DataPipe to become readable. We also block Close() on the calling thread
// while shutting down the DataPipe. While the calling thread is blocked,
// |task_runner| handles the actual read and cancel operations.
//
// This class must be created and destroyed on the |task_runner| because the
// mojo::SimpleWatcher expects to be created on the same sequence it is run on.
class InputStreamImpl : public InputStream {
 public:
  InputStreamImpl(connections::mojom::Medium medium,
                  scoped_refptr<base::SequencedTaskRunner> task_runner,
                  mojo::ScopedDataPipeConsumerHandle receive_stream);
  ~InputStreamImpl() override;
  InputStreamImpl(const InputStreamImpl&) = delete;
  InputStreamImpl& operator=(const InputStreamImpl&) = delete;

  // InputStream:
  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  // Must be run on the |task_runner_|.
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Returns true if |receive_stream_| is already closed.
  bool IsClosed() const;

  // Signals |task_run_waitable_event|, if not null, after the stream is closed.
  // Must be run on the |task_runner_|.
  void DoClose(base::WaitableEvent* task_run_waitable_event);

  connections::mojom::Medium medium_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;

  std::unique_ptr<ByteArray> pending_read_buffer_;
  size_t pending_read_buffer_pos_ = 0;
  ExceptionOr<ByteArray> exception_or_received_byte_array_;
  base::WaitableEvent read_waitable_event_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_STREAM_IMPL_H_
