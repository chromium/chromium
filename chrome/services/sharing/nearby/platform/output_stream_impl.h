// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_OUTPUT_STREAM_IMPL_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_OUTPUT_STREAM_IMPL_H_

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
#include "third_party/nearby/src/internal/platform/output_stream.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace nearby::chrome {

// An implementation of a Nearby Connections OutputStream that writes to the
// Mojo DataPipe, |send_stream|, passed into the constructor by the specified
// |medium| implementation.
//
// Because the OutputStream interface is synchronous but the DataPipe interface
// is asynchronous, we block Write() on the calling thread while waiting for the
// DataPipe to become writable. We also block Close() on the calling thread
// while shutting down the DataPipe. While the calling thread is blocked,
// |task_runner| handles the actual write and cancel operations.
//
// This class must be created and destroyed on the |task_runner| because the
// mojo::SimpleWatcher expects to be created on the same sequence it is run on.
class OutputStreamImpl : public OutputStream {
 public:
  OutputStreamImpl(connections::mojom::Medium medium,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   mojo::ScopedDataPipeProducerHandle send_stream);
  ~OutputStreamImpl() override;
  OutputStreamImpl(const OutputStreamImpl&) = delete;
  OutputStreamImpl& operator=(const OutputStreamImpl&) = delete;

  // OutputStream:
  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

  // Blocks until all in-flight Write() calls have returned. Must be called
  // after Close() and from a sequence other than |task_runner_| (e.g., from
  // BidirectionalStream::~BidirectionalStream() before destroying this object
  // on |task_runner_|).
  void WaitForInFlightWrites();

  // Runs |callback| on |task_runner_| once the next Write() has registered its
  // pending wait on |task_runner_|.
  void SetOnWritePendingForTesting(base::OnceClosure callback);

 private:
  class ScopedInFlightWrite;

  // Starts an asynchronous write of |data| on |task_runner_| and signals
  // |write_waitable_event| once |write_success| and |closed_during_write| are
  // populated. Must be run on |task_runner_|.
  void StartWrite(std::unique_ptr<ByteArray> data,
                  base::WaitableEvent* write_waitable_event,
                  bool* write_success,
                  bool* closed_during_write);

  // Must be run on the |task_runner_|.
  void SendMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Completes the currently pending write (if any) and signals its caller.
  // Must be run on |task_runner_|.
  void FinishPendingWrite(bool success, bool closed);

  // Returns true if Close() has been initiated. Thread-safe.
  bool IsClosed() const;

  // Decrements |writes_in_flight_| and signals |drain_waitable_event_| if no
  // writes remain in flight.
  void OnWriteFinished();

  // Signals |task_run_waitable_event|, if not null, after the stream is closed.
  // Must be run on the |task_runner_|.
  void DoClose(base::WaitableEvent* task_run_waitable_event);

  connections::mojom::Medium medium_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Only accessed on |task_runner_|.
  mojo::ScopedDataPipeProducerHandle send_stream_;
  mojo::SimpleWatcher send_stream_watcher_;
  std::unique_ptr<ByteArray> pending_write_buffer_;
  size_t pending_write_buffer_pos_ = 0;
  raw_ptr<base::WaitableEvent> pending_write_waitable_event_ = nullptr;
  raw_ptr<bool> pending_write_success_ = nullptr;
  raw_ptr<bool> pending_write_closed_ = nullptr;

  // Protects lifecycle state shared across calling threads and |task_runner_|.
  mutable base::Lock lock_;
  bool is_closed_ GUARDED_BY(lock_) = false;
  size_t writes_in_flight_ GUARDED_BY(lock_) = 0;
  raw_ptr<base::WaitableEvent> drain_waitable_event_ GUARDED_BY(lock_) =
      nullptr;
  base::OnceClosure on_write_pending_for_testing_ GUARDED_BY(lock_);
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_OUTPUT_STREAM_IMPL_H_
