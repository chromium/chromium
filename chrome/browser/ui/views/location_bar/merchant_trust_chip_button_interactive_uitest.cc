// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/location_bar/merchant_trust_chip_button_controller.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/page_info/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
}  // namespace

class MerchantTrustChipButtonInteractiveUITest : public InteractiveBrowserTest {
 public:
  MerchantTrustChipButtonInteractiveUITest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {page_info::kMerchantTrust,
         {{page_info::kMerchantTrustForceShowUIForTestingName, "true"}}}};
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  ~MerchantTrustChipButtonInteractiveUITest() override = default;
  MerchantTrustChipButtonInteractiveUITest(
      const MerchantTrustChipButtonInteractiveUITest&) = delete;
  void operator=(const MerchantTrustChipButtonInteractiveUITest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  GURL GetURL() { return https_server()->GetURL("a.test", "/title1.html"); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       MerchantTrustChipClick) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      PressButton(MerchantTrustChipButtonController::kElementIdForTesting),
      WaitForShow(PageInfoMerchantTrustContentView::kElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustChipButtonInteractiveUITest,
                       LocationBarIconClick) {
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, GetURL()),
      WaitForShow(MerchantTrustChipButtonController::kElementIdForTesting),
      PressButton(kLocationIconElementId),
      WaitForShow(PageInfoMainView::kMerchantTrustElementId),
      EnsurePresent(MerchantTrustChipButtonController::kElementIdForTesting));
}
