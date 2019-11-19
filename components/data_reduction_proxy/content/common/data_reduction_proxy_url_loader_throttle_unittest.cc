// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_url_loader_throttle.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class MockMojoDataReductionProxy : public mojom::DataReductionProxy {
 public:
  MockMojoDataReductionProxy() = default;
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override {
    if (waiting_for_mark_as_bad_closure_) {
      // We are being called via mojo.
      std::move(callback).Run();
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(waiting_for_mark_as_bad_closure_));
      return;
    }

    // We are being called via the mojom::DataReductionProxy interface
    // directly.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  }

  void AddThrottleConfigObserver(
      mojo::PendingRemote<
          mojom::DataReductionProxyThrottleConfigObserver> /* observer */)
      override {
    ++observer_count_;
  }

  void Clone(
      mojo::PendingReceiver<mojom::DataReductionProxy> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  // When mojom::DataReductionProxy methods are called via mojo, things are
  // asynchronous by nature.  Test code can then use this function to wait for
  // MarkProxiesAsBad() to be called.
  void WaitUntilMarkProxiesAsBadCalled() {
    ASSERT_TRUE(waiting_for_mark_as_bad_closure_.is_null());

    base::RunLoop run_loop;
    waiting_for_mark_as_bad_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // +1 because MockMojoDataReductionProxy uses direct calls through the
  // mojom::DataReductionProxy interface by default, and mojo is only involved
  // after Clone().
  size_t pipe_count() const { return receivers_.size() + 1u; }

  size_t observer_count() const { return observer_count_; }

 private:
  mojo::ReceiverSet<mojom::DataReductionProxy> receivers_;
  size_t observer_count_ = 0;

  base::OnceClosure waiting_for_mark_as_bad_closure_;

  DISALLOW_COPY_AND_ASSIGN(MockMojoDataReductionProxy);
};

class MockDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  MockDelegate() = default;

  void CancelWithError(int error_code,
                       base::StringPiece custom_reason = nullptr) override {
    FAIL() << "Should not be reached";
  }

  void Resume() override {
    resume_called++;
    if (resume_callback)
      std::move(resume_callback).Run();
  }

  void RestartWithFlags(int additional_load_flags) override {
    restart_with_flags_called++;
    restart_additional_load_flags = additional_load_flags;
  }

  size_t resume_called = 0;
  size_t restart_with_flags_called = 0;
  int restart_additional_load_flags = 0;

  base::OnceClosure resume_callback;

  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// Creates a DataReductionProxyThrottleManager which is bound to a
// mojo::DataReductionProxy, and has an initial throttle configuration
// containing |initial_drp_servers|.
std::unique_ptr<DataReductionProxyThrottleManager> CreateManager(
    mojom::DataReductionProxy* mojo_data_reduction_proxy,
    const std::vector<DataReductionProxyServer>& initial_drp_servers = {}) {
  auto initial_throttle_config =
      initial_drp_servers.empty()
          ? mojom::DataReductionProxyThrottleConfigPtr()
          : DataReductionProxyThrottleManager::CreateConfig(
                initial_drp_servers);

  return std::make_unique<DataReductionProxyThrottleManager>(
      mojo_data_reduction_proxy, std::move(initial_throttle_config));
}

DataReductionProxyServer MakeCoreDrpServer(const std::string pac_string) {
  auto proxy_server = net::ProxyServer::FromPacString(pac_string);
  EXPECT_TRUE(proxy_server.is_valid());
  return DataReductionProxyServer(proxy_server);
}

class DataReductionProxyURLLoaderThrottleTest : public ::testing::Test {
 public:
  MockMojoDataReductionProxy* mock_mojo_data_reduction_proxy() {
    return &mock_mojo_data_reduction_proxy_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  MockMojoDataReductionProxy mock_mojo_data_reduction_proxy_;
};

TEST_F(DataReductionProxyURLLoaderThrottleTest, ThrottleDiesFirst) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
}

TEST_F(DataReductionProxyURLLoaderThrottleTest,
       ThrottleDiesOnDifferentSequence) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  auto throttle = std::make_unique<DataReductionProxyURLLoaderThrottle>(
      (net::HttpRequestHeaders()), manager.get());
  throttle->DetachFromCurrentSequence();

  auto task_runner = base::CreateSequencedTaskRunner({base::ThreadPool()});
  task_runner->DeleteSoon(FROM_HERE, throttle.release());
}

TEST_F(DataReductionProxyURLLoaderThrottleTest, ManagerDiesFirst) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  manager.reset();
}

TEST_F(DataReductionProxyURLLoaderThrottleTest, AcceptTransformHeaderSet) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  DataReductionProxyURLLoaderThrottle throttle(net::HttpRequestHeaders(),
                                               manager.get());
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMedia);
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_pre_cache_headers.GetHeader(
      chrome_proxy_accept_transform_header(), &value));
  EXPECT_EQ(value, compressed_video_directive());
}

TEST_F(DataReductionProxyURLLoaderThrottleTest,
       AcceptTransformHeaderSetForMainFrame) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.previews_state = content::SERVER_LITE_PAGE_ON;
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_pre_cache_headers.GetHeader(
      chrome_proxy_accept_transform_header(), &value));
  EXPECT_EQ(value, lite_page_directive());
}

TEST_F(DataReductionProxyURLLoaderThrottleTest,
       ConstructorHeadersAddedToPostCacheHeaders) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());

  net::HttpRequestHeaders headers;
  headers.SetHeader("foo", "bar");
  DataReductionProxyURLLoaderThrottle throttle(headers, manager.get());
  network::ResourceRequest request;
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(defer);
  std::string value;
  EXPECT_TRUE(request.custom_proxy_post_cache_headers.GetHeader("foo", &value));
  EXPECT_EQ(value, "bar");
}

TEST_F(DataReductionProxyURLLoaderThrottleTest, UseAlternateProxyList) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  network::ResourceRequest request;
  request.resource_type = static_cast<int>(content::ResourceType::kMedia);
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_TRUE(request.custom_proxy_use_alternate_proxy_list);
}

TEST_F(DataReductionProxyURLLoaderThrottleTest, DontUseAlternateProxyList) {
  auto manager = CreateManager(mock_mojo_data_reduction_proxy());
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  network::ResourceRequest request;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.url = GURL("http://example.com");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);

  EXPECT_FALSE(request.custom_proxy_use_alternate_proxy_list);
}

void RestartBypassProxyAndCacheHelper(
    mojom::DataReductionProxy* mojo_data_reduction_proxy,
    bool response_came_from_drp) {
  base::HistogramTester histogram_tester;
  auto drp_server = MakeCoreDrpServer("QUIC proxy.googlezip.net:443");

  auto manager = CreateManager(mojo_data_reduction_proxy, {drp_server});
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  throttle.set_delegate(&delegate);

  network::ResourceRequest request;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.url = GURL("http://example.com/");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  net::ProxyServer proxy_used_for_response =
      response_came_from_drp
          ? drp_server.proxy_server()
          : net::ProxyServer::FromPacString("HTTPS otherproxy");

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->proxy_server = proxy_used_for_response;
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->AddHeader("Chrome-Proxy: block-once");

  throttle.BeforeWillProcessResponse(request.url, *response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);

  // If the response came from a DRP server, the Chrome-Proxy response header
  // should result in the request being restarted. Otherwise the Chrome-Proxy
  // response header is disregarded.
  if (response_came_from_drp) {
    EXPECT_EQ(1u, delegate.restart_with_flags_called);
    EXPECT_EQ(net::LOAD_BYPASS_PROXY | net::LOAD_BYPASS_CACHE,
              delegate.restart_additional_load_flags);
    histogram_tester.ExpectUniqueSample("DataReductionProxy.Quic.ProxyStatus",
                                        QUIC_PROXY_STATUS_AVAILABLE, 1);
  } else {
    EXPECT_EQ(0u, delegate.restart_with_flags_called);
    histogram_tester.ExpectUniqueSample("DataReductionProxy.Quic.ProxyStatus",
                                        QUIC_PROXY_NOT_SUPPORTED, 1);
  }
}

// Tests that when "Chrome-Proxy: block-once" is received from a DRP response
// the request is immediately restarted.
TEST_F(DataReductionProxyURLLoaderThrottleTest, RestartBypassProxyAndCache) {
  RestartBypassProxyAndCacheHelper(mock_mojo_data_reduction_proxy(),
                                   /*response_was_from_drp=*/true);
}

// Tests that when "Chrome-proxy: block-once" is received from a non-DRP server
// it is not interpreted as a special DRP directive (the request is NOT
// restarted).
TEST_F(DataReductionProxyURLLoaderThrottleTest,
       ChromeProxyDisregardedForNonDrp) {
  RestartBypassProxyAndCacheHelper(mock_mojo_data_reduction_proxy(),
                                   /*response_was_from_drp=*/false);
}

TEST_F(DataReductionProxyURLLoaderThrottleTest,
       DisregardChromeProxyFromDirect) {
  base::HistogramTester histogram_tester;
  auto drp_server = MakeCoreDrpServer("HTTPS localhost");

  auto manager = CreateManager(mock_mojo_data_reduction_proxy(), {drp_server});
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  throttle.set_delegate(&delegate);

  network::ResourceRequest request;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.url = GURL("http://example.com/");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  // Construct a response that did not come from a proxy server, however
  // includes a Chrome-Proxy header.
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->proxy_server = net::ProxyServer::Direct();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->AddHeader("Chrome-Proxy: block-once");

  throttle.BeforeWillProcessResponse(request.url, *response_head, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);
  histogram_tester.ExpectTotalCount("DataReductionProxy.Quic.ProxyStatus", 0);
}

TEST_F(DataReductionProxyURLLoaderThrottleTest, MarkProxyAsBadAndRestart) {
  auto drp_server = MakeCoreDrpServer("HTTPS localhost");

  auto manager = CreateManager(mock_mojo_data_reduction_proxy(), {drp_server});
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  throttle.set_delegate(&delegate);

  network::ResourceRequest request;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.url = GURL("http://www.example.com/");
  bool defer = false;

  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->proxy_server = drp_server.proxy_server();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 500 Server error\n");

  throttle.BeforeWillProcessResponse(request.url, *response_head, &defer);

  // The throttle should have marked the proxy as bad.
  EXPECT_TRUE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  // The throttle should restart and resume.
  base::RunLoop run_loop;
  delegate.resume_callback = run_loop.QuitClosure();
  run_loop.Run();

  EXPECT_EQ(1u, delegate.resume_called);
  EXPECT_EQ(1u, delegate.restart_with_flags_called);
  EXPECT_EQ(0, delegate.restart_additional_load_flags);
}

TEST_F(DataReductionProxyURLLoaderThrottleTest, MarkProxyAsBadOnNewSequence) {
  auto drp_server = MakeCoreDrpServer("HTTPS localhost");

  auto manager = CreateManager(mock_mojo_data_reduction_proxy(), {drp_server});
  MockDelegate delegate;
  DataReductionProxyURLLoaderThrottle throttle((net::HttpRequestHeaders()),
                                               manager.get());
  throttle.set_delegate(&delegate);

  // The URLLoaderThrottle is about to move to another sequence.  It should
  // create a private pipe to the DRP interface in preparation.
  throttle.DetachFromCurrentSequence();
  EXPECT_EQ(mock_mojo_data_reduction_proxy()->pipe_count(), 2u);

  network::ResourceRequest request;
  request.resource_type = static_cast<int>(content::ResourceType::kMainFrame);
  request.url = GURL("http://www.example.com/");
  bool defer = false;

  // WillStartRequest() is the first call on the new sequence.  This is when
  // the throttle should bind the private pipes to the sequence.
  throttle.WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->proxy_server = drp_server.proxy_server();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 500 Server error\n");

  throttle.BeforeWillProcessResponse(request.url, *response_head, &defer);

  // The throttle should have marked the proxy as bad.
  EXPECT_TRUE(defer);
  EXPECT_EQ(0u, delegate.resume_called);
  EXPECT_EQ(0u, delegate.restart_with_flags_called);

  // The DRP calls are carried by mojo now, which makes them asynchronous.
  mock_mojo_data_reduction_proxy()->WaitUntilMarkProxiesAsBadCalled();

  EXPECT_EQ(mock_mojo_data_reduction_proxy()->observer_count(), 2u);

  // The throttle should restart and resume.
  EXPECT_EQ(1u, delegate.resume_called);
  EXPECT_EQ(1u, delegate.restart_with_flags_called);
  EXPECT_EQ(0, delegate.restart_additional_load_flags);
}

}  // namespace

}  // namespace data_reduction_proxy
