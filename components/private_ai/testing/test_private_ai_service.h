// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_TEST_PRIVATE_AI_SERVICE_H_
#define COMPONENTS_PRIVATE_AI_TESTING_TEST_PRIVATE_AI_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/private_ai/private_ai_service.h"
#include "components/private_ai/testing/test_blind_sign_auth_factory.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace network::mojom {
class NetworkContext;
}

namespace signin {
class IdentityManager;
}

namespace private_ai {

namespace phosphor {
class MockBlindSignAuth;
}  // namespace phosphor

class PrivateAiNetworkDriver;
class PrivateAiOakSessionDriver;

// Test helper class for PrivateAiService. Manages the mock BlindSignAuth client
// and handles cleanup during service shutdown.
class TestPrivateAiService : public PrivateAiService {
 public:
  TestPrivateAiService(
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
      version_info::Channel channel);

  ~TestPrivateAiService() override;

  // PrivateAiService override:
  void Shutdown() override;

  phosphor::MockBlindSignAuth* mock_bsa();

 private:
  std::unique_ptr<TestBlindSignAuthFactory> test_bsa_factory_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_TEST_PRIVATE_AI_SERVICE_H_
