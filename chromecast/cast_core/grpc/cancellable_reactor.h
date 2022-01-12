// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_CANCELLABLE_REACTOR_H_
#define CHROMECAST_CAST_CORE_GRPC_CANCELLABLE_REACTOR_H_

#include <grpcpp/grpcpp.h>

#include <atomic>

#include "base/logging.h"

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

  template <typename... TArgs>
  explicit CancellableReactor(TArgs&&... args)
      : TReactor(std::forward<TArgs&&>(args)...) {}

 protected:
  // Implements GrpcServerReactor APIs.
  void FinishWriting(const grpc::ByteBuffer* buffer,
                     grpc::Status status) override {
    bool expected = false;
    if (!finished_.compare_exchange_strong(expected, true)) {
      LOG(WARNING) << "Reactor was already cancelled: " << name();
      return;
    }

    TReactor::FinishWriting(buffer, std::move(status));
  }

  // Implements grpc::ServerGenericBidiReactor APIs.
  // OnCancel is called on pending reactors while the gRPC server is
  // shutting down. At this point, we don't expect users to call the
  // reactors. If they were not finished properly, we must finish them
  // forcefully to unblock server shutdown process.
  void OnCancel() override {
    if (finished_.load()) {
      LOG(INFO) << "Reactor cancelled in finished state: " << name();
      return;
    }

    LOG(WARNING) << "Pending reactor got cancelled: " << name();
    FinishWriting(nullptr, grpc::Status(grpc::StatusCode::ABORTED,
                                        "Reactor was cancelled"));
  }

 private:
  std::atomic<bool> finished_ = {false};
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_CANCELLABLE_REACTOR_H_
