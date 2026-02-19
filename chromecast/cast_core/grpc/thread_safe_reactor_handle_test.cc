// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/grpc/thread_safe_reactor_handle.h"

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast::utils {

class MockReactor {
 public:
  void Write(const grpc::Status&) {
    if (on_write_) {
      on_write_.Run();
    }
  }

  void SetOnDestroyCallback(base::OnceClosure callback) {
    on_destroy_ = std::move(callback);
  }

  void Destroy() {
    if (on_destroy_) {
      std::move(on_destroy_).Run();
    }
  }

  base::OnceClosure on_destroy_;
  base::RepeatingClosure on_write_;
};

TEST(ThreadSafeReactorHandleTest, DeadlockCheck) {
  auto* reactor = new MockReactor();
  auto handle =
      base::MakeRefCounted<ThreadSafeReactorHandle<MockReactor>>(reactor);

  reactor->SetOnDestroyCallback(
      base::BindOnce(&ThreadSafeReactorHandle<MockReactor>::Reset, handle));

  reactor->on_write_ = base::BindLambdaForTesting([&]() {
    // This simulates the case where Write() triggers OnDone() synchronously,
    // which in turn calls Reset().
    reactor->Destroy();
  });

  // This would deadlock if the lock were held while calling into the reactor.
  handle->Write(grpc::Status::OK);

  delete reactor;
}

TEST(ThreadSafeReactorHandleTest, ReactorDiesFirst) {
  auto* reactor = new MockReactor();
  auto handle =
      base::MakeRefCounted<ThreadSafeReactorHandle<MockReactor>>(reactor);

  reactor->SetOnDestroyCallback(
      base::BindOnce(&ThreadSafeReactorHandle<MockReactor>::Reset, handle));

  // Simulate reactor destruction.
  reactor->Destroy();
  delete reactor;

  // Handle should now have a null pointer and skip the write.
  handle->Write(grpc::Status::OK);
}

}  // namespace cast::utils
