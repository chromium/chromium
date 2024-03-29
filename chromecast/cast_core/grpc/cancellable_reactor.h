// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_CANCELLABLE_REACTOR_H_
#define CHROMECAST_CAST_CORE_GRPC_CANCELLABLE_REACTOR_H_

#include <grpcpp/grpcpp.h>

#include <atomic>

#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace grpc {
class ByteBuffer;
}

namespace cast {
namespace utils {

// A facade around reactor implementation that allows to finish the reactor in
// case it was cancelled. No actions will happen if reactor was already
// finished.
template <typename TReactor>
class CancellableReactor : public TReactor {
 public:
  using TReactor::name;
  using typename TReactor::RequestType;

  template <typename... TArgs>
  explicit CancellableReactor(TArgs&&... args)
      : TReactor(std::forward<TArgs&&>(args)...) {}

  bool is_done() override {
    base::AutoLock l(lock_);
    return done_deferred_ || finished_;
  }

 protected:
  // Implements GrpcServerReactor APIs.
  void WriteResponse(const grpc::ByteBuffer* buffer) override {
    {
      base::ReleasableAutoLock l(&lock_);
      request_pending_ = false;
      if (done_deferred_) {
        // The gRPC framework tried to finalize the reactor, but it was pending
        // the handler response. Now it can be fully finalized.
        LOG(INFO) << "Finalizing deferred reactor: " << name();

        // The lock must be released, so that the object can be destroyed
        // without debug warnings.
        l.Release();

        // Need to call OnResponseDone to let the caller know in the write
        // callback that the reactor is finished.
        TReactor::OnResponseDone(
            grpc::Status(grpc::StatusCode::ABORTED, "Reactor done"));
        TReactor::OnDone();
        return;
      }
      if (finished_) {
        // The reactor is already being finalized - ignore.
        LOG(WARNING)
            << "Not writing response to reactor as it's already finished: "
            << name();
        return;
      }
    }

    TReactor::WriteResponse(buffer);
  }

  void FinishWriting(const grpc::ByteBuffer* buffer,
                     const grpc::Status& status) override {
    {
      base::ReleasableAutoLock l(&lock_);
      request_pending_ = false;
      if (done_deferred_) {
        // The gRPC framework tried to finalize the reactor, but it was pending
        // the handler response. Now it can be fully finalized.
        LOG(INFO) << "Finalizing deferred reactor: " << name();

        // The lock must be released, so that the object can be destroyed
        // without debug warnings.
        l.Release();

        TReactor::OnDone();
        return;
      }
      if (finished_) {
        // The reactor is already being finalized - ignore.
        LOG(WARNING)
            << "Not finishing the reactor as it's already being finished: "
            << name();
        return;
      }
      finished_ = true;
    }

    TReactor::FinishWriting(buffer, status);
  }

  void OnRequestDone(GrpcStatusOr<RequestType> request) override {
    {
      base::AutoLock l(lock_);
      if (finished_) {
        // Do not process the request if reactor has been cancelled/finished.
        LOG(WARNING) << "Got a request, but reactor was already finished: "
                     << name();
        return;
      }
      request_pending_ = true;
    }

    TReactor::OnRequestDone(std::move(request));
  }

  // Implements grpc::ServerGenericBidiReactor APIs.
  // OnCancel is called on pending reactors while the gRPC server is
  // shutting down. At this point, we don't expect users to call the
  // reactors. If they were not finished properly, we must finish them
  // forcefully to unblock server shutdown process.
  void OnCancel() override {
    {
      base::AutoLock l(lock_);
      if (finished_) {
        LOG(WARNING) << "Not cancelling the reactor as it's already finished: "
                     << name();
        return;
      }
      LOG(WARNING) << "Active reactor got cancelled: " << name();
      finished_ = true;
    }

    TReactor::FinishWriting(nullptr, grpc::Status(grpc::StatusCode::ABORTED,
                                                  "Reactor was cancelled"));
  }

  void OnDone() override {
    {
      base::AutoLock l(lock_);
      if (request_pending_) {
        // Need to wait for the client to release the reactor.
        done_deferred_ = true;
        LOG(WARNING)
            << "Waiting for client write before finishing the reactor: "
            << name();
        return;
      }
    }

    TReactor::OnDone();
  }

 private:
  base::Lock lock_;
  bool done_deferred_ GUARDED_BY(lock_) = false;
  bool finished_ GUARDED_BY(lock_) = false;
  bool request_pending_ GUARDED_BY(lock_) = false;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_CANCELLABLE_REACTOR_H_
