// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"

#include <memory>

#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_net {

namespace {

class MockTabHelperDelegate : public EnterpriseProxyTabHelper::Delegate {
 public:
  ~MockTabHelperDelegate() override = default;

  MOCK_METHOD(void, SignIn, (content::WebContents*), (override));
};

}  // namespace
class EnterpriseProxyTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ON_CALL(mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    error_service_ =
        std::make_unique<EnterpriseProxyErrorService>(&mock_proxy_service_);
    tab_helper_ = std::make_unique<EnterpriseProxyTabHelper>(
        mock_tab_, web_contents(), error_service_.get());
  }

  void TearDown() override {
    tab_helper_.reset();
    content::RenderViewHostTestHarness::TearDown();
    error_service_.reset();
  }

 protected:
  tabs::MockTabInterface mock_tab_;
  MockEnterpriseProxyService mock_proxy_service_;
  std::unique_ptr<EnterpriseProxyErrorService> error_service_;
  std::unique_ptr<EnterpriseProxyTabHelper> tab_helper_;
};

TEST_F(EnterpriseProxyTabHelperTest, FromTabInterface) {
  EXPECT_EQ(EnterpriseProxyTabHelper::From(&mock_tab_), tab_helper_.get());
  EXPECT_EQ(EnterpriseProxyTabHelper::From(nullptr), nullptr);
}

TEST_F(EnterpriseProxyTabHelperTest,
       PrimaryMainFrameNavigation_TracksIdAndCleansUpErrorState) {
  ASSERT_TRUE(tab_helper_);
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://target.example.com"), web_contents());
  simulator->Start();

  int64_t nav_id = tab_helper_->active_navigation_id();
  EXPECT_NE(nav_id, 0);

  EnterpriseProxyErrorData error_data(GURL("https://target.example.com"),
                                      GURL("https://proxy.example.com"), 502);
  error_service_->RecordDisguisedError(nav_id, error_data);
  EXPECT_TRUE(error_service_->TakeDisguisedError(nav_id).has_value());

  // Record again so we can verify cleanup on DidFinishNavigation.
  error_service_->RecordDisguisedError(nav_id, error_data);

  simulator->Fail(net::ERR_TUNNEL_CONNECTION_FAILED);
  simulator->CommitErrorPage();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);
  EXPECT_FALSE(error_service_->TakeDisguisedError(nav_id).has_value());
}

TEST_F(EnterpriseProxyTabHelperTest, IgnoresSubframeNavigations) {
  ASSERT_TRUE(tab_helper_);
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");

  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://subframe.example.com"), child_rfh);
  subframe_simulator->Start();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  subframe_simulator->Commit();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);
}

TEST_F(EnterpriseProxyTabHelperTest, IgnoresSameDocumentNavigations) {
  ASSERT_TRUE(tab_helper_);
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  // Commit an initial cross-document navigation to set up the document.
  auto initial_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://target.example.com"), web_contents());
  initial_simulator->Commit();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);

  // Same-document navigation should not update active_navigation_id.
  auto same_doc_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://target.example.com#hash"), main_rfh());
  same_doc_simulator->CommitSameDocument();
  EXPECT_EQ(tab_helper_->active_navigation_id(), 0);
}

TEST_F(EnterpriseProxyTabHelperTest, SignIn_InvokesDelegate) {
  ASSERT_TRUE(tab_helper_);

  auto delegate = std::make_unique<MockTabHelperDelegate>();
  EXPECT_CALL(*delegate, SignIn(web_contents()));
  tab_helper_->SetDelegateForTesting(std::move(delegate));

  tab_helper_->SignIn();
}

}  // namespace enterprise_net
