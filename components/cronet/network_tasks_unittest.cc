// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_context.h"

#include <latch>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_verifier.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
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
      // QUIC User Agent ID.
      "Default QUIC User Agent ID",
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
      absl::nullopt);
}

class NetworkTasksTest : public testing::Test {
 protected:
  NetworkTasksTest()
      : ncn_(net::NetworkChangeNotifier::CreateIfNeeded()),
        network_task_runner_(
            base::ThreadPool::CreateSingleThreadTaskRunner({})),
        file_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner({})),
        network_tasks_(new CronetContext::NetworkTasks(
            CreateSimpleURLRequestContextConfig(),
            std::make_unique<NoOpCronetContextCallback>())) {
    scoped_ncn_.mock_network_change_notifier()->ForceNetworkHandlesSupported();
    Initialize();
  }

  void Initialize() {
    PostToNetworkThreadSync(
        base::BindOnce(&CronetContext::NetworkTasks::Initialize,
                       base::Unretained(network_tasks_), network_task_runner_,
                       file_task_runner_,
                       std::make_unique<net::ProxyConfigServiceFixed>(
                           net::ProxyConfigWithAnnotation::CreateDirect())));
  }

  void SpawnNetworkBoundURLRequestContext(
      net::NetworkChangeNotifier::NetworkHandle network) {
    PostToNetworkThreadSync(base::BindLambdaForTesting([=]() {
      network_tasks_->SpawnNetworkBoundURLRequestContextForTesting(network);
    }));
  }

  void CheckURLRequestContextExistence(
      net::NetworkChangeNotifier::NetworkHandle network,
      bool expected) {
    PostToNetworkThreadSync(base::BindLambdaForTesting([=]() {
      EXPECT_EQ(expected,
                network_tasks_->DoesURLRequestContextExistForTesting(network));
    }));
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
  net::test::ScopedMockNetworkChangeNotifier scoped_ncn_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  raw_ptr<CronetContext::NetworkTasks> network_tasks_;
};

TEST_F(NetworkTasksTest, NetworkBoundContextLifetime) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_MARSHMALLOW) {
    GTEST_SKIP() << "Network binding on Android requires an API level >= 23";
  }
#endif  // BUILDFLAG(IS_ANDROID)
  constexpr net::NetworkChangeNotifier::NetworkHandle kNetwork = 1;

  CheckURLRequestContextExistence(kNetwork, false);
  SpawnNetworkBoundURLRequestContext(kNetwork);
  CheckURLRequestContextExistence(kNetwork, true);

  // Once the network disconnects the context should be destroyed.
  scoped_ncn_.mock_network_change_notifier()->NotifyNetworkDisconnected(
      kNetwork);
  CheckURLRequestContextExistence(kNetwork, false);
}

}  // namespace

}  // namespace cronet
