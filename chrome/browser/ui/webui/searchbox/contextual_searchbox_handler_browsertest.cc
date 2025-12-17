// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"

#include "base/test/bind.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display_switches.h"

class TestSearchboxHandler : public ContextualSearchboxHandler {
 public:
  TestSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      GetSessionHandleCallback get_session_callback)
      : ContextualSearchboxHandler(std::move(pending_page_handler),
                                   profile,
                                   web_contents,
                                   std::make_unique<OmniboxController>(
                                       std::make_unique<TestOmniboxClient>()),
                                   std::move(get_session_callback)) {}

  ~TestSearchboxHandler() override = default;

  void OnThumbnailRemoved() override {}
};

class ContextualSearchboxHandlerBrowserTest : public InProcessBrowserTest {
 protected:
  testing::NiceMock<MockSearchboxPage> page_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  std::unique_ptr<TestSearchboxHandler> handler_;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto* service =
        ContextualSearchServiceFactory::GetForProfile(browser()->profile());
    session_handle_ = service->CreateSession(
        ntp_composebox::CreateQueryControllerConfigParams(),
        contextual_search::ContextualSearchSource::kUnknown);

    handler_ = std::make_unique<TestSearchboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        browser()->profile(),
        /*web_contents=*/browser()->tab_strip_model()->GetActiveWebContents(),
        base::BindLambdaForTesting([&]() { return session_handle_.get(); }));
    handler_->SetPage(page_.BindAndGetRemote());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  }

  void TearDownOnMainThread() override { handler_.reset(); }
};

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       CreateTabPreviewEncodingOptions_NotScaled) {
  // When no device scale factor is applied, physical pixels should translate to
  // CSS pixels at a 1:1 ratio.
  int expected_width = 125 * 1;
  int expected_height = 200 * 1;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto options = handler_->CreateTabPreviewEncodingOptions(web_contents);

  ASSERT_TRUE(options.has_value());
  EXPECT_EQ(options->max_width, expected_width);
  EXPECT_EQ(options->max_height, expected_height);
}

class ContextualSearchboxHandlerBrowserTestDSF2
    : public ContextualSearchboxHandlerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextualSearchboxHandlerBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTestDSF2,
                       CreateTabPreviewEncodingOptions_Scaled) {
  // 60 physical pixels translates to 30 CSS pixels when the device scale factor
  // = 2 (2 physical pixels : 1 CSS pixel);
  int expected_width = 125 * 2;
  int expected_height = 200 * 2;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto options = handler_->CreateTabPreviewEncodingOptions(web_contents);

  ASSERT_TRUE(options.has_value());
  EXPECT_EQ(options->max_width, expected_width);
  EXPECT_EQ(options->max_height, expected_height);
}
