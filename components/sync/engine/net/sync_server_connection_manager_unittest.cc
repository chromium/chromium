// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/sync_server_connection_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/net/http_post_provider.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class BlockingHttpPost : public HttpPostProvider {
 public:
  BlockingHttpPost()
      : wait_for_abort_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const GURL& url) override {}
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
  ~BlockingHttpPost() override = default;

  base::WaitableEvent wait_for_abort_;
};

class BlockingHttpPostFactory : public HttpPostProviderFactory {
 public:
  ~BlockingHttpPostFactory() override = default;

  scoped_refptr<HttpPostProvider> Create() override {
    return new BlockingHttpPost();
  }
};

}  // namespace

// Ask the ServerConnectionManager to stop before it is created.
TEST(SyncServerConnectionManagerTest, VeryEarlyAbortPost) {
  CancelationSignal signal;
  signal.Signal();
  SyncServerConnectionManager server(
      GURL("https://server"), std::make_unique<BlockingHttpPostFactory>(),
      &signal);

  std::string buffer_out;
  HttpResponse http_response = server.PostBuffer("", "testauth", &buffer_out);

  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE, http_response.server_status);
}

// Ask the ServerConnectionManager to stop before its first request is made.
TEST(SyncServerConnectionManagerTest, EarlyAbortPost) {
  CancelationSignal signal;
  SyncServerConnectionManager server(
      GURL("https://server"), std::make_unique<BlockingHttpPostFactory>(),
      &signal);

  signal.Signal();
  std::string buffer_out;
  HttpResponse http_response = server.PostBuffer("", "testauth", &buffer_out);

  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE, http_response.server_status);
}

// Ask the ServerConnectionManager to stop during a request.
TEST(SyncServerConnectionManagerTest, AbortPost) {
  CancelationSignal signal;
  SyncServerConnectionManager server(
      GURL("https://server"), std::make_unique<BlockingHttpPostFactory>(),
      &signal);

  base::Thread abort_thread("Test_AbortThread");
  ASSERT_TRUE(abort_thread.Start());
  abort_thread.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancelationSignal::Signal, base::Unretained(&signal)),
      TestTimeouts::tiny_timeout());

  std::string buffer_out;
  HttpResponse http_response = server.PostBuffer("", "testauth", &buffer_out);

  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE, http_response.server_status);
  abort_thread.Stop();
}

namespace {

class FailingHttpPost : public HttpPostProvider {
 public:
  explicit FailingHttpPost(int net_error_code)
      : net_error_code_(net_error_code) {}

  void SetExtraRequestHeaders(const char* headers) override {}
  void SetURL(const GURL& url) override {}
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
  ~FailingHttpPost() override = default;

  int net_error_code_;
};

class FailingHttpPostFactory : public HttpPostProviderFactory {
 public:
  explicit FailingHttpPostFactory(int net_error_code)
      : net_error_code_(net_error_code) {}
  ~FailingHttpPostFactory() override = default;

  scoped_refptr<HttpPostProvider> Create() override {
    return new FailingHttpPost(net_error_code_);
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
      GURL("https://server"),
      std::make_unique<FailingHttpPostFactory>(net::ERR_TIMED_OUT), &signal);

  std::string buffer_out;
  HttpResponse http_response = server.PostBuffer("", "testauth", &buffer_out);

  EXPECT_EQ(HttpResponse::CONNECTION_UNAVAILABLE, http_response.server_status);
}

}  // namespace syncer
