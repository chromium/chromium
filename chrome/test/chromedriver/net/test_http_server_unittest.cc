// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/test_http_server.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "mojo/core/embedder/embedder.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct ServerGuard {
  explicit ServerGuard(TestHttpServer& server) : server_(server) {}
  ~ServerGuard() { server_->Stop(); }
  raw_ref<TestHttpServer> server_;
};

class TestHttpServerTest : public testing::Test {
 public:
  TestHttpServerTest()
      : io_thread_("io"),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    CHECK(io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));

    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TestHttpServerTest::InitOnIO,
                                  base::Unretained(this), &event));

    event.Wait();
  }

  void TearDown() override {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&TestHttpServerTest::DestroyServerOnIO,
                                  base::Unretained(this), &event));
    event.Wait();
  }

  static void SetUpTestSuite() { mojo::core::Init(); }

  void InitOnIO(base::WaitableEvent* event) {
    scoped_refptr<URLRequestContextGetter> context_getter =
        new URLRequestContextGetter(io_thread_.task_runner());
    url_loader_factory_owner_ =
        std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
            context_getter);
    url_loader_factory_ =
        url_loader_factory_owner_->GetURLLoaderFactory().get();
    event->Signal();
  }

  void DestroyServerOnIO(base::WaitableEvent* event) {
    url_loader_factory_ = nullptr;
    url_loader_factory_owner_.reset();
    event->Signal();
  }

  bool DoFetchURL(const GURL& server_url, std::string* response) {
    SetIOCapableTaskRunnerForTest(io_thread_.task_runner());
    return FetchUrl(server_url, url_loader_factory_, response);
  }

 protected:
  base::Thread io_thread_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_;
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(TestHttpServerTest, Start) {
  TestHttpServer server;
  ServerGuard server_guard(server);
  ASSERT_TRUE(server.Start());
  EXPECT_NE("", server.http_url());
  EXPECT_NE("", server.web_socket_url());
}

TEST_F(TestHttpServerTest, ResourceNotFound) {
  TestHttpServer server;
  ServerGuard server_guard(server);
  ASSERT_TRUE(server.Start());
  GURL url = server.http_url().Resolve("no-such-resource");
  std::string response;
  ASSERT_FALSE(DoFetchURL(url, &response));
}

TEST_F(TestHttpServerTest, SetDataForPath) {
  TestHttpServer server;
  ServerGuard server_guard(server);
  ASSERT_TRUE(server.Start());
  server.SetDataForPath("hello", "<p>Hello!</p>");
  server.SetDataForPath("/world", "<p>World!</p>");
  server.SetDataForPath("one/two/three", "<span>six</span>");
  server.SetDataForPath("/eno/owt/eerht", "<span>xis</span>");
  server.SetDataForPath("", "empty");
  // This instruction must override the instruction with the empty path
  server.SetDataForPath("/", "slash");
  GURL url = server.http_url();
  std::string response;
  ASSERT_TRUE(DoFetchURL(url.Resolve("hello"), &response));
  EXPECT_EQ("<p>Hello!</p>", response);
  ASSERT_TRUE(DoFetchURL(url.Resolve("one/two/three"), &response));
  EXPECT_EQ("<span>six</span>", response);
  ASSERT_TRUE(DoFetchURL(url.Resolve("world"), &response));
  EXPECT_EQ("<p>World!</p>", response);
  ASSERT_TRUE(DoFetchURL(url.Resolve("eno/owt/eerht"), &response));
  EXPECT_EQ("<span>xis</span>", response);
  ASSERT_TRUE(DoFetchURL(url, &response));
  EXPECT_EQ("slash", response);
  ASSERT_FALSE(DoFetchURL(url.Resolve("no-such-resource"), &response));
}
