// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_network_context.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

network::mojom::NetworkContextParamsPtr CreateNetworkContextParams(
    int* call_count) {
  ++(*call_count);
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  params->cert_verifier_params =
      network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  return params;
}

void InvokeCreateLoaderAndStartAndClone(
    network::SharedURLLoaderFactory* url_loader_factory) {
  mojo::Remote<network::mojom::URLLoader> loader_remote;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver =
      client_remote.InitWithNewPipeAndPassReceiver();
  network::ResourceRequest request;
  url_loader_factory->CreateLoaderAndStart(
      loader_remote.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      /*options=*/0, request, std::move(client_remote),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  mojo::Remote<network::mojom::URLLoaderFactory> cloned_factory;
  url_loader_factory->Clone(cloned_factory.BindNewPipeAndPassReceiver());
}

}  // namespace

class SafeBrowsingNetworkContextTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SafeBrowsingNetworkContextTest,
       ActiveURLLoaderFactoryCreatesNetworkContext) {
  int params_factory_call_count = 0;
  SafeBrowsingNetworkContext sb_network_context(
      temp_dir_.GetPath(), /*trigger_migration=*/true,
      base::BindRepeating(&CreateNetworkContextParams,
                          base::Unretained(&params_factory_call_count)));

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      sb_network_context.GetURLLoaderFactory();
  ASSERT_NE(nullptr, url_loader_factory);

  InvokeCreateLoaderAndStartAndClone(url_loader_factory.get());
  EXPECT_EQ(1, params_factory_call_count);
  sb_network_context.ServiceShuttingDown();
}

TEST_F(SafeBrowsingNetworkContextTest,
       ServiceShuttingDownPreventsNetworkContextRecreation) {
  int params_factory_call_count = 0;
  SafeBrowsingNetworkContext sb_network_context(
      temp_dir_.GetPath(), /*trigger_migration=*/true,
      base::BindRepeating(&CreateNetworkContextParams,
                          base::Unretained(&params_factory_call_count)));

  ASSERT_NE(nullptr, sb_network_context.GetNetworkContext());
  EXPECT_EQ(1, params_factory_call_count);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      sb_network_context.GetURLLoaderFactory();
  ASSERT_NE(nullptr, url_loader_factory);

  sb_network_context.ServiceShuttingDown();

  EXPECT_EQ(nullptr, sb_network_context.GetNetworkContext());
  InvokeCreateLoaderAndStartAndClone(url_loader_factory.get());
  EXPECT_EQ(1, params_factory_call_count);
}

TEST_F(SafeBrowsingNetworkContextTest,
       DestructorPreventsParamsFactoryCallbackAfterDestruction) {
  int params_factory_call_count = 0;
  auto sb_network_context = std::make_unique<SafeBrowsingNetworkContext>(
      temp_dir_.GetPath(), /*trigger_migration=*/true,
      base::BindRepeating(&CreateNetworkContextParams,
                          base::Unretained(&params_factory_call_count)));

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      sb_network_context->GetURLLoaderFactory();
  ASSERT_NE(nullptr, url_loader_factory);

  // Destroy without calling `ServiceShuttingDown()` first.
  sb_network_context.reset();

  InvokeCreateLoaderAndStartAndClone(url_loader_factory.get());
  EXPECT_EQ(0, params_factory_call_count);
}

}  // namespace safe_browsing
