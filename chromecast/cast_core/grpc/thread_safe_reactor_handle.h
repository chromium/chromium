// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_THREAD_SAFE_REACTOR_HANDLE_H_
#define CHROMECAST_CAST_CORE_GRPC_THREAD_SAFE_REACTOR_HANDLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"

namespace cast::utils {

// A thread-safe handle for accessing a gRPC Reactor.
//
// This class is used to share ownership of a reactor pointer between the gRPC
// thread (where the reactor lives and is destroyed) and the main thread (where
// tasks using the reactor are executed). It protects the raw pointer with a
// lock to prevent Use-After-Free (UAF) races.
template <typename TReactor>
class ThreadSafeReactorHandle
    : public base::RefCountedThreadSafe<ThreadSafeReactorHandle<TReactor>> {
 public:
  explicit ThreadSafeReactorHandle(TReactor* reactor) : reactor_(reactor) {}

  void Write(const grpc::Status& status) {
    TReactor* reactor = nullptr;
    {
      base::AutoLock lock(lock_);
      reactor = reactor_;
      reactor_ = nullptr;
    }

    if (reactor) {
      reactor->Write(status);
    }
  }

  template <typename TResponse>
  void Write(TResponse response) {
    TReactor* reactor = nullptr;
    {
      base::AutoLock lock(lock_);
      reactor = reactor_;
      reactor_ = nullptr;
    }

    if (reactor) {
      reactor->Write(std::move(response));
    }
  }

  // Resets the reactor pointer to nullptr. This should be called when the
  // reactor is about to be destroyed.
  void Reset() {
    base::AutoLock lock(lock_);
    reactor_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeReactorHandle<TReactor>>;
  ~ThreadSafeReactorHandle() = default;

  base::Lock lock_;
  raw_ptr<TReactor> reactor_ GUARDED_BY(lock_);
};

}  // namespace cast::utils

#endif  // CHROMECAST_CAST_CORE_GRPC_THREAD_SAFE_REACTOR_HANDLE_H_
