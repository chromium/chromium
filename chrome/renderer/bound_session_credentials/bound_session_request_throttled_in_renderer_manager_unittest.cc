// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/bound_session_request_throttled_listener.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using UnblockAction = BoundSessionRequestThrottledListener::UnblockAction;

class FakeBoundSessionRequestThrottledListener
    : public chrome::mojom::BoundSessionRequestThrottledListener {
 public:
  FakeBoundSessionRequestThrottledListener(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledListener>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  void OnRequestBlockedOnCookie(
      OnRequestBlockedOnCookieCallback callback) override {
    // There shouldn't be more than one notification at a time of requests being
    // blocked.
    EXPECT_FALSE(callback_);
    callback_ = std::move(callback);
  }

  void SimulateOnRequestBlockedOnCookieCompleted() {
    EXPECT_TRUE(callback_);
    std::move(callback_).Run();
  }

  bool IsRequestBlocked() { return !callback_.is_null(); }

 private:
  OnRequestBlockedOnCookieCallback callback_;
  mojo::Receiver<chrome::mojom::BoundSessionRequestThrottledListener> receiver_;
};
}  // namespace

class BoundSessionRequestThrottledInRendererManagerTest
    : public ::testing::Test {
 public:
  BoundSessionRequestThrottledInRendererManagerTest() {
    mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledListener>
        remote;
    listener_ = std::make_unique<FakeBoundSessionRequestThrottledListener>(
        remote.InitWithNewPipeAndPassReceiver());
    manager_ = new BoundSessionRequestThrottledInRendererManager();
    manager_->Initialize(std::move(remote));
  }

  ~BoundSessionRequestThrottledInRendererManagerTest() override = default;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  BoundSessionRequestThrottledInRendererManager* manager() const {
    return manager_.get();
  }

  FakeBoundSessionRequestThrottledListener* listener() {
    return listener_.get();
  }

  void ResetListener() { listener_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeBoundSessionRequestThrottledListener> listener_;
  scoped_refptr<BoundSessionRequestThrottledInRendererManager> manager_;
};

TEST_F(BoundSessionRequestThrottledInRendererManagerTest, SingleRequest) {
  base::test::TestFuture<UnblockAction> future;
  manager()->OnRequestBlockedOnCookie(future.GetCallback());

  RunUntilIdle();
  EXPECT_TRUE(listener()->IsRequestBlocked());

  listener()->SimulateOnRequestBlockedOnCookieCompleted();
  EXPECT_EQ(future.Get(), UnblockAction::kResume);
}

TEST_F(BoundSessionRequestThrottledInRendererManagerTest, MultipleRequests) {
  constexpr size_t kBlockedRequests = 5;
  std::array<base::test::TestFuture<UnblockAction>, kBlockedRequests> futures;
  for (auto& future : futures) {
    manager()->OnRequestBlockedOnCookie(future.GetCallback());
  }

  // Allow mojo message posting to complete.
  RunUntilIdle();
  EXPECT_TRUE(listener()->IsRequestBlocked());
  for (auto& future : futures) {
    EXPECT_FALSE(future.IsReady());
  }

  listener()->SimulateOnRequestBlockedOnCookieCompleted();
  for (auto& future : futures) {
    EXPECT_EQ(future.Get(), UnblockAction::kResume);
  }
}

TEST_F(BoundSessionRequestThrottledInRendererManagerTest,
       RemoteDisconnectedPendingBlockedRequestsAreCancelled) {
  constexpr size_t kBlockedRequests = 5;
  std::array<base::test::TestFuture<UnblockAction>, kBlockedRequests> futures;
  for (auto& future : futures) {
    manager()->OnRequestBlockedOnCookie(future.GetCallback());
  }

  // Allow mojo message posting to complete.
  RunUntilIdle();
  EXPECT_TRUE(listener()->IsRequestBlocked());

  ResetListener();
  for (auto& future : futures) {
    EXPECT_EQ(future.Get(), UnblockAction::kCancel);
  }
}

TEST_F(BoundSessionRequestThrottledInRendererManagerTest,
       RemoteDisconnectedNewBlockedRequestsAreCancelled) {
  ResetListener();

  base::test::TestFuture<UnblockAction> future;
  manager()->OnRequestBlockedOnCookie(future.GetCallback());

  EXPECT_EQ(future.Get(), UnblockAction::kCancel);
}
