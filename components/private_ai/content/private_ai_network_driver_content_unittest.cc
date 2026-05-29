// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/content/private_ai_network_driver_content.h"

#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

TEST(PrivateAiNetworkDriverContentTest, GetCertVerifierParams) {
  content::BrowserTaskEnvironment task_environment;
  PrivateAiNetworkDriverContent driver;
  auto params = driver.GetCertVerifierParams();
  ASSERT_TRUE(params);
  EXPECT_TRUE(params->cert_verifier_service.is_valid());
}

TEST(PrivateAiNetworkDriverContentTest, CreateNetworkContext) {
  content::BrowserTaskEnvironment task_environment;
  PrivateAiNetworkDriverContent driver;
  mojo::PendingRemote<network::mojom::NetworkContext> network_context;
  auto params = network::mojom::NetworkContextParams::New();
  params->cert_verifier_params = driver.GetCertVerifierParams();
  driver.CreateNetworkContext(network_context.InitWithNewPipeAndPassReceiver(),
                              std::move(params));
  EXPECT_TRUE(network_context.is_valid());
}

}  // namespace private_ai
