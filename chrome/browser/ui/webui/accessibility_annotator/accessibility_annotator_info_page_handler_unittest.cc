// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info_page_handler.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace accessibility_annotator::info {
namespace {

class AccessibilityAnnotatorInfoPageHandlerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    test_web_ui_.set_web_contents(web_contents());
    webui::SetBrowserWindowInterface(web_contents(), &mock_browser_interface_);

    handler_ = std::make_unique<AccessibilityAnnotatorInfoPageHandler>(
        page_handler_.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting(
            [this](InfoDialogResult result) { result_ = result; }),
        test_web_ui_.GetWebContents());
  }

  void TearDown() override {
    handler_.reset();
    test_web_ui_.set_web_contents(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_interface_;
  mojo::Remote<accessibility_annotator::info::mojom::PageHandler> page_handler_;
  std::unique_ptr<AccessibilityAnnotatorInfoPageHandler> handler_;
  InfoDialogResult result_ = InfoDialogResult::kDismissed;
  base::UserActionTester user_action_tester_;
  content::TestWebUI test_web_ui_;
};

TEST_F(AccessibilityAnnotatorInfoPageHandlerTest, OnManageSettingsClicked) {
  EXPECT_EQ(
      0, user_action_tester_.GetActionCount(
             "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));

  EXPECT_CALL(
      mock_browser_interface_,
      OpenURL(
          testing::Field(
              &content::OpenURLParams::url,
              GURL(
                  accessibility_annotator::kAccessibilityAnnotatorSettingsURL)),
          testing::_))
      .WillOnce(testing::Return(nullptr));

  handler_->OnManageSettingsClicked();

  EXPECT_EQ(
      1, user_action_tester_.GetActionCount(
             "AccessibilityAnnotator.RemoteAnnotatorInfo.SettingsLinkClick"));
}

}  // namespace
}  // namespace accessibility_annotator::info
