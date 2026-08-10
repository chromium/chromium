// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/test_private_ai_service.h"

#include "components/private_ai/private_ai_network_driver.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace private_ai {

TestPrivateAiService::TestPrivateAiService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    network::mojom::NetworkContext* network_context,
    const std::string& url,
    const std::string& api_key,
    const std::string& proxy_url,
    bool use_token_attestation,
    std::unique_ptr<PrivateAiNetworkDriver> network_driver,
    std::unique_ptr<PrivateAiOakSessionDriver> oak_session_driver,
    std::unique_ptr<TestBlindSignAuthFactory> test_bsa_factory,
    version_info::Channel channel)
    : PrivateAiService(identity_manager,
                       test_bsa_factory.get(),
                       std::move(url_loader_factory),
                       std::move(network_driver),
                       std::move(oak_session_driver),
                       network_context,
                       url,
                       api_key,
                       proxy_url,
                       use_token_attestation,
                       channel),
      test_bsa_factory_(std::move(test_bsa_factory)) {}

TestPrivateAiService::~TestPrivateAiService() = default;

void TestPrivateAiService::Shutdown() {
  test_bsa_factory_->ResetBsa();
  PrivateAiService::Shutdown();
}

phosphor::MockBlindSignAuth* TestPrivateAiService::mock_bsa() {
  return test_bsa_factory_->mock_bsa();
}

}  // namespace private_ai
