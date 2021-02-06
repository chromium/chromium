// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/net_util.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FetchUrlTest : public testing::Test,
                     public net::HttpServer::Delegate {
 public:
  FetchUrlTest()
      : io_thread_("io"),
        response_(kSendHello),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    CHECK(io_thread_.StartWithOptions(options));

    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FetchUrlTest::InitOnIO,
                                  base::Unretained(this), &event));

    event.Wait();
  }

  ~FetchUrlTest() override {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FetchUrlTest::DestroyServerOnIO,
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

    std::unique_ptr<net::ServerSocket> server_socket(
        new net::TCPServerSocket(NULL, net::NetLogSource()));
    server_socket->ListenWithAddressAndPort("127.0.0.1", 0, 1);
    server_.reset(new net::HttpServer(std::move(server_socket), this));
    net::IPEndPoint address;
    CHECK_EQ(net::OK, server_->GetLocalAddress(&address));
    server_url_ = base::StringPrintf("http://127.0.0.1:%d", address.port());
    event->Signal();
  }

  void DestroyServerOnIO(base::WaitableEvent* event) {
    url_loader_factory_owner_.reset();
    server_.reset(NULL);
    event->Signal();
  }

  // Overridden from net::HttpServer::Delegate:
  void OnConnect(int connection_id) override {}

  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override {
    switch (response_) {
      case kSendHello:
        server_->Send200(connection_id, "hello", "text/plain",
                         TRAFFIC_ANNOTATION_FOR_TESTS);
        break;
      case kSend404:
        server_->Send404(connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
        break;
      case kClose:
        server_->Close(connection_id);
        break;
      default:
        break;
    }
  }

  bool DoFetchURL(const std::string& server_url, std::string* response) {
    SetIOCapableTaskRunnerForTest(io_thread_.task_runner());
    return FetchUrl(server_url, url_loader_factory_, response);
  }

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override {}
  void OnWebSocketMessage(int connection_id, std::string data) override {}
  void OnClose(int connection_id) override {}

 protected:
  enum ServerResponse {
    kSendHello = 0,
    kSend404,
    kClose,
  };

  base::Thread io_thread_;
  ServerResponse response_;
  std::unique_ptr<net::HttpServer> server_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  network::mojom::URLLoaderFactory* url_loader_factory_;
  std::string server_url_;
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

#if !defined(THREAD_SANITIZER)
TEST_F(FetchUrlTest, Http200) {
  std::string response("stuff");
  ASSERT_TRUE(DoFetchURL(server_url_, &response));
  ASSERT_STREQ("hello", response.c_str());
}
#endif

#if !defined(THREAD_SANITIZER)
TEST_F(FetchUrlTest, HttpNon200) {
  response_ = kSend404;
  std::string response("stuff");
  ASSERT_FALSE(DoFetchURL(server_url_, &response));
  ASSERT_STREQ("stuff", response.c_str());
}
#endif

#if !defined(THREAD_SANITIZER)
TEST_F(FetchUrlTest, ConnectionClose) {
  response_ = kClose;
  std::string response("stuff");
  ASSERT_FALSE(DoFetchURL(server_url_, &response));
  ASSERT_STREQ("stuff", response.c_str());
}
#endif

#if !defined(THREAD_SANITIZER)
TEST_F(FetchUrlTest, NoServer) {
  std::string response("stuff");
  ASSERT_FALSE(DoFetchURL("http://localhost:33333", &response));
  ASSERT_STREQ("stuff", response.c_str());
}
#endif
