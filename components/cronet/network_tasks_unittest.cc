// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_context.h"

#include <latch>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_verifier.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace cronet {

namespace {

class NoOpCronetContextCallback : public CronetContext::Callback {
 public:
  NoOpCronetContextCallback() = default;

  NoOpCronetContextCallback(const NoOpCronetContextCallback&) = delete;
  NoOpCronetContextCallback& operator=(const NoOpCronetContextCallback&) =
      delete;

  void OnInitNetworkThread() override {}

  void OnDestroyNetworkThread() override {}

  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType effective_connection_type) override {}

  void OnRTTOrThroughputEstimatesComputed(
      int32_t http_rtt_ms,
      int32_t transport_rtt_ms,
      int32_t downstream_throughput_kbps) override {}

  void OnRTTObservation(int32_t rtt_ms,
                        int32_t timestamp_ms,
                        net::NetworkQualityObservationSource source) override {}

  void OnThroughputObservation(
      int32_t throughput_kbps,
      int32_t timestamp_ms,
      net::NetworkQualityObservationSource source) override {}

  void OnStopNetLogCompleted() override {}

  ~NoOpCronetContextCallback() override = default;
};

std::unique_ptr<URLRequestContextConfig> CreateSimpleURLRequestContextConfig() {
  return URLRequestContextConfig::CreateURLRequestContextConfig(
      // Enable QUIC.
      true,
      // Enable SPDY.
      true,
      // Enable Brotli.
      false,
      // Type of http cache.
      URLRequestContextConfig::HttpCacheType::DISK,
      // Max size of http cache in bytes.
      1024000,
      // Disable caching for HTTP responses. Other information may be stored
      // in the cache.
      false,
      // Storage path for http cache and cookie storage.
      "/data/data/org.chromium.net/app_cronet_test/test_storage",
      // Accept-Language request header field.
      "foreign-language",
      // User-Agent request header field.
      "fake agent",
      // JSON encoded experimental options.
      "",
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier>(),
      // Enable network quality estimator.
      false,
      // Enable Public Key Pinning bypass for local trust anchors.
      true,
      // Optional network thread priority.
      std::nullopt);
}

class NetworkTasksTest : public testing::Test {
 protected:
  NetworkTasksTest()
      : ncn_(net::NetworkChangeNotifier::CreateMockIfNeeded()),
        scoped_ncn_(
            std::make_unique<net::test::ScopedMockNetworkChangeNotifier>()),
        network_thread_(std::make_unique<base::Thread>("network")),
        file_thread_(std::make_unique<base::Thread>("Network File Thread")),
        network_tasks_(new CronetContext::NetworkTasks(
            CreateSimpleURLRequestContextConfig(),
            std::make_unique<NoOpCronetContextCallback>())) {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    network_thread_->StartWithOptions(std::move(options));
    network_task_runner_ = network_thread_->task_runner();

    file_thread_->Start();
    file_task_runner_ = file_thread_->task_runner();

    scoped_ncn_->mock_network_change_notifier()->ForceNetworkHandlesSupported();
    Initialize();
  }

  ~NetworkTasksTest() override {
    PostToNetworkThreadSync(base::BindOnce(
        // Deletion ocurrs as a result of the argument going out of scope.
        [](std::unique_ptr<CronetContext::NetworkTasks> tasks_to_be_deleted) {},
        std::move(network_tasks_)));
  }

  void Initialize() {
    PostToNetworkThreadSync(
        base::BindOnce(&CronetContext::NetworkTasks::Initialize,
                       base::Unretained(network_tasks_.get()),
                       network_task_runner_, file_task_runner_,
                       std::make_unique<net::ProxyConfigServiceFixed>(
                           net::ProxyConfigWithAnnotation::CreateDirect())));
  }

  void SpawnNetworkBoundURLRequestContext(net::handles::NetworkHandle network) {
    PostToNetworkThreadSync(base::BindLambdaForTesting([=, this]() {
      network_tasks_->SpawnNetworkBoundURLRequestContextForTesting(network);
    }));
  }

  void CheckURLRequestContextExistence(net::handles::NetworkHandle network,
                                       bool expected) {
    std::atomic_bool context_exists = false;
    PostToNetworkThreadSync(base::BindLambdaForTesting([&]() {
      context_exists.store(
          network_tasks_->URLRequestContextExistsForTesting(network));
    }));
    EXPECT_EQ(expected, context_exists.load());
  }

  void CreateURLRequest(net::handles::NetworkHandle network) {
    std::atomic_bool url_request_created = false;
    PostToNetworkThreadSync(base::BindLambdaForTesting([&]() {
      auto* context = network_tasks_->GetURLRequestContext(network);
      url_request_ = context->CreateRequest(GURL("http://www.foo.com"),
                                            net::DEFAULT_PRIORITY, nullptr,
                                            TRAFFIC_ANNOTATION_FOR_TESTS);
      url_request_created = !!url_request_;
    }));
    EXPECT_TRUE(url_request_created);
  }

  void ReleaseURLRequest() {
    PostToNetworkThreadSync(
        base::BindLambdaForTesting([&]() { url_request_.reset(); }));
  }

  void MaybeDestroyURLRequestContext(net::handles::NetworkHandle network) {
    PostToNetworkThreadSync(base::BindLambdaForTesting(
        [&]() { network_tasks_->MaybeDestroyURLRequestContext(network); }));
  }

  void PostToNetworkThreadSync(base::OnceCallback<void()> callback) {
    std::latch callback_executed{1};
    auto wait_for_callback = base::BindLambdaForTesting(
        [&callback_executed]() { callback_executed.count_down(); });
    network_task_runner_->PostTask(
        FROM_HERE, std::move(callback).Then(std::move(wait_for_callback)));
    callback_executed.wait();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::NetworkChangeNotifier> ncn_;
  std::unique_ptr<net::test::ScopedMockNetworkChangeNotifier> scoped_ncn_;
  std::unique_ptr<base::Thread> network_thread_;
  std::unique_ptr<base::Thread> file_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  std::unique_ptr<CronetContext::NetworkTasks> network_tasks_;
  std::unique_ptr<net::URLRequest> url_request_;
};

TEST_F(NetworkTasksTest, NetworkBoundContextLifetime) {
#if BUILDFLAG(IS_ANDROID)
  constexpr net::handles::NetworkHandle kNetwork = 1;

  CheckURLRequestContextExistence(kNetwork, false);
  SpawnNetworkBoundURLRequestContext(kNetwork);
  CheckURLRequestContextExistence(kNetwork, true);

  // Once the network disconnects the context should be destroyed.
  scoped_ncn_->mock_network_change_notifier()->NotifyNetworkDisconnected(
      kNetwork);
  CheckURLRequestContextExistence(kNetwork, false);
#else
  GTEST_SKIP() << "Network binding is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(NetworkTasksTest, NetworkBoundContextWithPendingRequest) {
#if BUILDFLAG(IS_ANDROID)
  constexpr net::handles::NetworkHandle kNetwork = 1;

  CheckURLRequestContextExistence(kNetwork, false);
  SpawnNetworkBoundURLRequestContext(kNetwork);
  CheckURLRequestContextExistence(kNetwork, true);

  // If after a network disconnection there are still pending requests, the
  // context should not be destroyed to avoid UAFs (URLRequests can reference
  // their associated URLRequestContext).
  CreateURLRequest(kNetwork);
  CheckURLRequestContextExistence(kNetwork, true);
  scoped_ncn_->mock_network_change_notifier()->QueueNetworkDisconnected(
      kNetwork);
  CheckURLRequestContextExistence(kNetwork, true);

  // Once the URLRequest is destroyed, MaybeDestroyURLRequestContext should be
  // able to destroy the context.
  ReleaseURLRequest();
  MaybeDestroyURLRequestContext(kNetwork);
  CheckURLRequestContextExistence(kNetwork, false);
#else
  GTEST_SKIP() << "Network binding is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

}  // namespace cronet
