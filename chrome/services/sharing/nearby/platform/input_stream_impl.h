// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_STREAM_IMPL_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_STREAM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
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

  // Blocks until all in-flight Read() calls have returned. Must be called
  // after Close() and from a sequence other than |task_runner_| (e.g., from
  // BidirectionalStream::~BidirectionalStream() before destroying this object
  // on |task_runner_|).
  void WaitForInFlightReads();

  // Runs |callback| on |task_runner_| once the next Read() has registered its
  // pending wait on |task_runner_|.
  void SetOnReadPendingForTesting(base::OnceClosure callback);

 private:
  class ScopedInFlightRead;

  // Starts an asynchronous read of |size| bytes on |task_runner_| and signals
  // |read_waitable_event| once |result| and |closed_during_read| are populated.
  // Must be run on |task_runner_|.
  void StartRead(size_t size,
                 base::WaitableEvent* read_waitable_event,
                 ExceptionOr<ByteArray>* result,
                 bool* closed_during_read);

  // Must be run on the |task_runner_|.
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Completes the currently pending read (if any) and signals its caller.
  // Must be run on |task_runner_|.
  void FinishPendingRead(ExceptionOr<ByteArray> result, bool closed);

  // Returns true if Close() has been initiated. Thread-safe.
  bool IsClosed() const;

  // Decrements |reads_in_flight_| and signals |drain_waitable_event_| if no
  // reads remain in flight.
  void OnReadFinished();

  // Signals |task_run_waitable_event|, if not null, after the stream is closed.
  // Must be run on the |task_runner_|.
  void DoClose(base::WaitableEvent* task_run_waitable_event);

  connections::mojom::Medium medium_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Only accessed on |task_runner_|.
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::SimpleWatcher receive_stream_watcher_;
  std::unique_ptr<ByteArray> pending_read_buffer_;
  size_t pending_read_buffer_pos_ = 0;
  raw_ptr<base::WaitableEvent> pending_read_waitable_event_ = nullptr;
  raw_ptr<ExceptionOr<ByteArray>> pending_read_result_ = nullptr;
  raw_ptr<bool> pending_read_closed_ = nullptr;

  // Protects lifecycle state shared across calling threads and |task_runner_|.
  mutable base::Lock lock_;
  bool is_closed_ GUARDED_BY(lock_) = false;
  size_t reads_in_flight_ GUARDED_BY(lock_) = 0;
  raw_ptr<base::WaitableEvent> drain_waitable_event_ GUARDED_BY(lock_) =
      nullptr;
  base::OnceClosure on_read_pending_for_testing_ GUARDED_BY(lock_);
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_STREAM_IMPL_H_
