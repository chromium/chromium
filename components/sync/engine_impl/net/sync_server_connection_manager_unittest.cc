// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/net/sync_server_connection_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/net/http_post_provider_interface.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::TimeDelta;

class BlockingHttpPost : public HttpPostProviderInterface {
 public:
  BlockingHttpPost()
      : wait_for_abort_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~BlockingHttpPost() override {}

  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const char* url, int port) override {}
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override {}
  bool MakeSynchronousPost(int* net_error_code,
                           int* http_status_code) override {
    wait_for_abort_.TimedWait(TestTimeouts::action_max_timeout());
    *net_error_code = net::ERR_ABORTED;
    return false;
  }
  int GetResponseContentLength() const override { return 0; }
  const char* GetResponseContent() const override { return ""; }
  const std::string GetResponseHeaderValue(
      const std::string& name) const override {
    return std::string();
  }
  void Abort() override { wait_for_abort_.Signal(); }

 private:
  base::WaitableEvent wait_for_abort_;
};

class BlockingHttpPostFactory : public HttpPostProviderFactory {
 public:
  ~BlockingHttpPostFactory() override {}

  HttpPostProviderInterface* Create() override {
    return new BlockingHttpPost();
  }
  void Destroy(HttpPostProviderInterface* http) override {
    delete static_cast<BlockingHttpPost*>(http);
  }
};

}  // namespace

// Ask the ServerConnectionManager to stop before it is created.
TEST(SyncServerConnectionManagerTest, VeryEarlyAbortPost) {
  CancelationSignal signal;
  signal.Signal();
  SyncServerConnectionManager server(
      "server", 0, true, std::make_unique<BlockingHttpPostFactory>(), &signal);

  ServerConnectionManager::PostBufferParams params;

  bool result = server.PostBufferToPath(&params, "/testpath", "testauth");

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
}

// Ask the ServerConnectionManager to stop before its first request is made.
TEST(SyncServerConnectionManagerTest, EarlyAbortPost) {
  CancelationSignal signal;
  SyncServerConnectionManager server(
      "server", 0, true, std::make_unique<BlockingHttpPostFactory>(), &signal);

  ServerConnectionManager::PostBufferParams params;

  signal.Signal();
  bool result = server.PostBufferToPath(&params, "/testpath", "testauth");

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
}

// Ask the ServerConnectionManager to stop during a request.
TEST(SyncServerConnectionManagerTest, AbortPost) {
  CancelationSignal signal;
  SyncServerConnectionManager server(
      "server", 0, true, std::make_unique<BlockingHttpPostFactory>(), &signal);

  ServerConnectionManager::PostBufferParams params;

  base::Thread abort_thread("Test_AbortThread");
  ASSERT_TRUE(abort_thread.Start());
  abort_thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelationSignal::Signal, base::Unretained(&signal)),
      TestTimeouts::tiny_timeout());

  bool result = server.PostBufferToPath(&params, "/testpath", "testauth");

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
  abort_thread.Stop();
}

namespace {

class FailingHttpPost : public HttpPostProviderInterface {
 public:
  explicit FailingHttpPost(int net_error_code)
      : net_error_code_(net_error_code) {}
  ~FailingHttpPost() override {}

  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const char* url, int port) override {}
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override {}
  bool MakeSynchronousPost(int* net_error_code,
                           int* http_status_code) override {
    *net_error_code = net_error_code_;
    return false;
  }
  int GetResponseContentLength() const override { return 0; }
  const char* GetResponseContent() const override { return ""; }
  const std::string GetResponseHeaderValue(
      const std::string& name) const override {
    return std::string();
  }
  void Abort() override {}

 private:
  int net_error_code_;
};

class FailingHttpPostFactory : public HttpPostProviderFactory {
 public:
  explicit FailingHttpPostFactory(int net_error_code)
      : net_error_code_(net_error_code) {}
  ~FailingHttpPostFactory() override {}

  HttpPostProviderInterface* Create() override {
    return new FailingHttpPost(net_error_code_);
  }
  void Destroy(HttpPostProviderInterface* http) override {
    delete static_cast<FailingHttpPost*>(http);
  }

 private:
  int net_error_code_;
};

}  // namespace

// Fail request with TIMED_OUT error. Make sure server status is
// CONNECTION_UNAVAILABLE and therefore request will be retried after network
// change.
TEST(SyncServerConnectionManagerTest, FailPostWithTimedOut) {
  CancelationSignal signal;
  SyncServerConnectionManager server(
      "server", 0, true,
      std::make_unique<FailingHttpPostFactory>(net::ERR_TIMED_OUT), &signal);

  ServerConnectionManager::PostBufferParams params;

  bool result = server.PostBufferToPath(&params, "/testpath", "testauth");

  EXPECT_FALSE(result);
  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE,
            params.response.server_status);
}

}  // namespace syncer
