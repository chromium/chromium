// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_status.h"

#include <gmock/gmock.h>

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/ip_protection/common/ip_protection_status_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace ip_protection {
namespace {
using ::content::WebContents;
using ::testing::_;

net::ProxyChain CreateProxyChain(bool is_for_ip_protection,
                                 bool is_direct = false) {
  if (is_for_ip_protection) {
    if (is_direct) {
      return net::ProxyChain::Direct();
    }
    return net::ProxyChain::ForIpProtection(
        {net::ProxyUriToProxyServer("foo:555", net::ProxyServer::SCHEME_HTTPS),
         net::ProxyUriToProxyServer("foo:666",
                                    net::ProxyServer::SCHEME_HTTPS)});
  }

  // Default to invalid proxy chain.
  return net::ProxyChain();
}

blink::mojom::ResourceLoadInfoPtr CreateResourceLoadInfo(
    net::ProxyChain proxy_chain) {
  blink::mojom::ResourceLoadInfoPtr resource_load_info =
      blink::mojom::ResourceLoadInfo::New();

  resource_load_info->proxy_chain = std::move(proxy_chain);
  return resource_load_info;
}

class MockIpProtectionStatusObserver : public IpProtectionStatusObserver {
 public:
  MOCK_METHOD(void,
              OnFirstSubresourceProxiedOnCurrentPrimaryPage,
              (),
              (const, override));
};

class IpProtectionStatusTest : public content::RenderViewHostTestHarness {
 public:
  IpProtectionStatusTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnableIpProtectionProxy);

    // Create the IpProtectionStatus object and attach it to the web contents.
    IpProtectionStatus::CreateForWebContents(
        RenderViewHostTestHarness::web_contents());
    ip_protection_status_ = IpProtectionStatus::FromWebContents(web_contents());
    ip_protection_status_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    ip_protection_status_->RemoveObserver(&mock_observer_);
    ip_protection_status_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockIpProtectionStatusObserver mock_observer_;
  raw_ptr<IpProtectionStatus> ip_protection_status_;
};

TEST_F(IpProtectionStatusTest,
       SubresourceProxied_NonIpProtectionProxyChain_ObserverNotNotified) {
  EXPECT_CALL(mock_observer_, OnFirstSubresourceProxiedOnCurrentPrimaryPage())
      .Times(0);
  // Verify that the state is initially false.
  EXPECT_FALSE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());

  // Simulate a subresource load with a direct proxy chain.
  ip_protection_status_->ResourceLoadComplete(
      content::RenderViewHostTestHarness::main_rfh(),
      content::GlobalRequestID(),
      *CreateResourceLoadInfo(
          CreateProxyChain(/*is_for_ip_protection=*/false)));

  // Verify that the state is not updated and remained false.
  EXPECT_FALSE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());
}

TEST_F(IpProtectionStatusTest,
       SubresourceProxied_DirectProxyChain_ObserverNotNotified) {
  EXPECT_CALL(mock_observer_, OnFirstSubresourceProxiedOnCurrentPrimaryPage())
      .Times(0);
  // Verify that the state is initially false.
  EXPECT_FALSE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());

  // Simulate a subresource load with a direct proxy chain.
  ip_protection_status_->ResourceLoadComplete(
      content::RenderViewHostTestHarness::main_rfh(),
      content::GlobalRequestID(),
      *CreateResourceLoadInfo(CreateProxyChain(/*is_for_ip_protection=*/true,
                                               /*is_direct=*/true)));

  // Verify that the state is not updated and remained false.
  EXPECT_FALSE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());
}

TEST_F(IpProtectionStatusTest,
       SubresourceProxied_IpProtectionProxyChain_ObserverNotified) {
  EXPECT_CALL(mock_observer_, OnFirstSubresourceProxiedOnCurrentPrimaryPage());
  // Verify that the state is initially false.
  EXPECT_FALSE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());

  // Simulate a subresource load with a valid IP Protection proxy chain.
  ip_protection_status_->ResourceLoadComplete(
      content::RenderViewHostTestHarness::main_rfh(),
      content::GlobalRequestID(),
      *CreateResourceLoadInfo(CreateProxyChain(/*is_for_ip_protection=*/true)));

  // Verify that the state shows that a subresource was proxied.
  EXPECT_TRUE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());
}

TEST_F(IpProtectionStatusTest, PrimaryPageChanged_StateReset) {
  // Simulate a subresource load with a valid IP Protection proxy chain.
  ip_protection_status_->ResourceLoadComplete(
      content::RenderViewHostTestHarness::main_rfh(),
      content::GlobalRequestID(),
      *CreateResourceLoadInfo(CreateProxyChain(/*is_for_ip_protection=*/true)));

  // Verify that the state shows that a subresource was proxied.
  EXPECT_TRUE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());

  // Simulate a primary page change.
  ip_protection_status_->PrimaryPageChanged(
      content::RenderViewHostTestHarness::main_rfh()->GetPage());

  // Verify that the state is reset.
  EXPECT_FALSE(
      ip_protection_status_->IsSubresourceProxiedOnCurrentPrimaryPage());
}
}  // namespace
}  // namespace ip_protection
