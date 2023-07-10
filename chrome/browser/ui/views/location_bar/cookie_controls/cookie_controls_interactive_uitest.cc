// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_icon_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
}

class CookieControlsInteractiveUiTest : public InteractiveBrowserTest {
 public:
  CookieControlsInteractiveUiTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~CookieControlsInteractiveUiTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kUserBypassUI);
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, BubbleOpens) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  const GURL third_party_cookie_page_url =
      https_server()->GetURL("a.test", "/third_party_partitioned_cookies.html");
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url),
      PressButton(CookieControlsIconView::kCookieControlsIcon),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)));
}
