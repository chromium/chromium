// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/new_tab_footer/mock_new_tab_footer_document.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class NewTabFooterHandlerBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({ntp_features::kNtpFooter}, {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        document_.BindAndGetRemote(),
        base::WeakPtr<TopChromeWebUIController::Embedder>(), web_contents());
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  NewTabFooterHandler& handler() { return *handler_; }
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NewTabFooterHandler> handler_;
  testing::NiceMock<MockNewTabFooterDocument> document_;
};

IN_PROC_BROWSER_TEST_F(NewTabFooterHandlerBrowserTest,
                       OpenExtensionOptionsPage_ExistingExtensionId) {
  // Load extension that overrides the New Tab Page.
  extensions::TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"(
                            {
                              "chrome_url_overrides": {
                                  "newtab": "ext.html"
                              },
                              "name": "Extension-overridden NTP",
                              "manifest_version": 3,
                              "version": "0.1"
                            })";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                          "<body>Extension-overridden NTP</body>");
  scoped_refptr<const extensions::Extension> extension =
      LoadExtension(extension_dir.Pack());
  ASSERT_TRUE(extension);
  // Invoke UpdateNtpExtensionName, triggering the handler to set its New Tab
  // Page extension ID.
  handler().UpdateNtpExtensionName();

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  handler().OpenExtensionOptionsPageWithFallback();
  WaitForLoadStop(web_contents());

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  const GURL expected_url = net::AppendOrReplaceQueryParameter(
      GURL(chrome::kChromeUIExtensionsURL), "id", extension->id());
  EXPECT_EQ(expected_url, web_contents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(NewTabFooterHandlerBrowserTest,
                       OpenExtensionOptionsPage_UseFallback) {
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  handler().OpenExtensionOptionsPageWithFallback();

  WaitForLoadStop(web_contents());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  const GURL expected_url = GURL(chrome::kChromeUIExtensionsURL);
  EXPECT_EQ(expected_url, web_contents()->GetLastCommittedURL());
}
