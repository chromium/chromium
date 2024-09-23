// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/renderer/bound_session_credentials/bound_session_request_throttled_in_renderer_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/bound_session_request_throttled_handler.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using testing::FieldsAre;
using UnblockAction = BoundSessionRequestThrottledHandler::UnblockAction;
using ResumeBlockedRequestsTrigger =
    chrome::mojom::ResumeBlockedRequestsTrigger;

class FakeBoundSessionRequestThrottledHandler
    : public chrome::mojom::BoundSessionRequestThrottledHandler {
 public:
  FakeBoundSessionRequestThrottledHandler(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledHandler>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  void HandleRequestBlockedOnCookie(
      const GURL& untrusted_request_url,
      HandleRequestBlockedOnCookieCallback callback) override {
    request_urls_.emplace_back(untrusted_request_url);
    callbacks_.push(std::move(callback));
  }

  void SimulateHandleRequestBlockedOnCookieCompleted() {
    EXPECT_FALSE(callbacks_.empty());
    std::move(callbacks_.front())
        .Run(ResumeBlockedRequestsTrigger::kCookieAlreadyFresh);
    callbacks_.pop();
  }

  size_t NumberOfBlockedRequests() { return callbacks_.size(); }

  std::vector<GURL> TakeRequestUrls() {
    std::vector<GURL> args;
    std::swap(args, request_urls_);
    return args;
  }

 private:
  std::queue<HandleRequestBlockedOnCookieCallback> callbacks_;
  std::vector<GURL> request_urls_;
  mojo::Receiver<chrome::mojom::BoundSessionRequestThrottledHandler> receiver_;
};
}  // namespace

class BoundSessionRequestThrottledInRendererManagerTest
    : public ::testing::Test {
 public:
  const GURL kRequestGURLs[5] = {
      GURL("https://mail.google.com"), GURL("http://www.google.com"),
      GURL("about:blank"), GURL("https://origin.test/"),
      GURL("https://example.com")};
  BoundSessionRequestThrottledInRendererManagerTest() {
    mojo::PendingRemote<chrome::mojom::BoundSessionRequestThrottledHandler>
        remote;
    handler_ = std::make_unique<FakeBoundSessionRequestThrottledHandler>(
        remote.InitWithNewPipeAndPassReceiver());
    manager_ = new BoundSessionRequestThrottledInRendererManager();
    manager_->Initialize(std::move(remote));
  }

  ~BoundSessionRequestThrottledInRendererManagerTest() override = default;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  BoundSessionRequestThrottledInRendererManager* manager() const {
    return manager_.get();
  }

  FakeBoundSessionRequestThrottledHandler* handler() { return handler_.get(); }

  void ResetHandler() { handler_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeBoundSessionRequestThrottledHandler> handler_;
  scoped_refptr<BoundSessionRequestThrottledInRendererManager> manager_;
};

TEST_F(BoundSessionRequestThrottledInRendererManagerTest, SingleRequest) {
  base::test::TestFuture<UnblockAction, ResumeBlockedRequestsTrigger> future;
  manager()->HandleRequestBlockedOnCookie(kRequestGURLs[0],
                                          future.GetCallback());

  RunUntilIdle();
  EXPECT_EQ(handler()->NumberOfBlockedRequests(), 1U);
  EXPECT_THAT(handler()->TakeRequestUrls(),
              testing::ElementsAre(kRequestGURLs[0]));

  handler()->SimulateHandleRequestBlockedOnCookieCompleted();
  EXPECT_THAT(future.Get(),
              FieldsAre(UnblockAction::kResume,
                        ResumeBlockedRequestsTrigger::kCookieAlreadyFresh));
}

TEST_F(BoundSessionRequestThrottledInRendererManagerTest, MultipleRequests) {
  constexpr size_t kBlockedRequests = 5;
  std::array<
      base::test::TestFuture<UnblockAction, ResumeBlockedRequestsTrigger>,
      kBlockedRequests>
      futures;

  for (size_t i = 0; i < futures.size(); ++i) {
    manager()->HandleRequestBlockedOnCookie(kRequestGURLs[i],
                                            futures[i].GetCallback());
  }

  // Allow mojo message posting to complete.
  RunUntilIdle();
  EXPECT_EQ(handler()->NumberOfBlockedRequests(), kBlockedRequests);
  EXPECT_THAT(handler()->TakeRequestUrls(),
              testing::ElementsAreArray(kRequestGURLs));

  for (auto& future : futures) {
    EXPECT_FALSE(future.IsReady());
  }

  for (size_t i = 0; i < futures.size(); ++i) {
    handler()->SimulateHandleRequestBlockedOnCookieCompleted();
    EXPECT_THAT(futures[i].Get(),
                FieldsAre(UnblockAction::kResume,
                          ResumeBlockedRequestsTrigger::kCookieAlreadyFresh));
  }
}

TEST_F(BoundSessionRequestThrottledInRendererManagerTest,
       RemoteDisconnectedPendingBlockedRequestsAreCancelled) {
  constexpr size_t kBlockedRequests = 5;
  std::array<
      base::test::TestFuture<UnblockAction, ResumeBlockedRequestsTrigger>,
      kBlockedRequests>
      futures;
  for (auto& future : futures) {
    manager()->HandleRequestBlockedOnCookie(GURL(), future.GetCallback());
  }

  // Allow mojo message posting to complete.
  RunUntilIdle();
  EXPECT_EQ(handler()->NumberOfBlockedRequests(), kBlockedRequests);

  ResetHandler();
  for (auto& future : futures) {
    EXPECT_THAT(future.Get(),
                FieldsAre(UnblockAction::kCancel,
                          ResumeBlockedRequestsTrigger::kRendererDisconnected));
  }
}

TEST_F(BoundSessionRequestThrottledInRendererManagerTest,
       RemoteDisconnectedNewBlockedRequestsAreCancelled) {
  ResetHandler();

  base::test::TestFuture<UnblockAction, ResumeBlockedRequestsTrigger> future;
  manager()->HandleRequestBlockedOnCookie(GURL(), future.GetCallback());

  EXPECT_THAT(future.Get(),
              FieldsAre(UnblockAction::kCancel,
                        ResumeBlockedRequestsTrigger::kRendererDisconnected));
}
