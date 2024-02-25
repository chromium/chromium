// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/sharesheet/constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

// Keep in sync with //chrome/browser/resources/nearby_share/shared/types.js
enum class CloseReason {
  kUnknown = 0,
  kTransferStarted = 1,
  kTransferSucceeded = 2,
  kCancelled = 3,
  kRejected = 4
};

class TestSharesheetController : public sharesheet::SharesheetController {
 public:
  // sharesheet::SharesheetController
  void SetBubbleSize(int width, int height) override {}
  void CloseBubble(::sharesheet::SharesheetResult result) override {
    last_result = result;
  }
  bool IsBubbleVisible() const override { return !last_result; }

  std::optional<::sharesheet::SharesheetResult> last_result;
};

class NearbyShareDialogUITest : public InProcessBrowserTest {
 public:
  NearbyShareDialogUITest() {
    scoped_feature_list_.InitWithFeatures({features::kNearbySharing}, {});
  }
  ~NearbyShareDialogUITest() override = default;

 protected:
  content::WebContents* GetWebContentsForNearbyShareHost() const {
    GURL kUrl(content::GetWebUIURL(chrome::kChromeUINearbyShareHost));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(web_contents);
    EXPECT_EQ(kUrl, web_contents->GetLastCommittedURL());
    EXPECT_FALSE(web_contents->IsCrashed());

    return web_contents;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestSharesheetController sharesheet_controller_;
};

std::string BuildCloseScript(CloseReason reason) {
  return base::StringPrintf("chrome.send('close',[%d]);",
                            static_cast<int>(reason));
}

}  // namespace

IN_PROC_BROWSER_TEST_F(NearbyShareDialogUITest, RendersComponent) {
  content::WebContents* web_contents = GetWebContentsForNearbyShareHost();

  // Assert that we render the nearby-share-app component.
  EXPECT_EQ(1, content::EvalJs(
                   web_contents,
                   "document.getElementsByTagName('nearby-share-app').length"));
}

IN_PROC_BROWSER_TEST_F(NearbyShareDialogUITest,
                       SharesheetControllerGetsCalledOnClose) {
  content::WebContents* web_contents = GetWebContentsForNearbyShareHost();

  auto* webui = web_contents->GetWebUI();
  ASSERT_TRUE(webui);

  // Add a test observer and verify it gets called when 'close' is sent.
  auto* nearby_ui =
      webui->GetController()->GetAs<nearby_share::NearbyShareDialogUI>();
  ASSERT_TRUE(nearby_ui);

  // Calling 'close' before a Sharesheet controller is registered via
  // |SetSharesheetController| does not result in a crash.
  std::string script = BuildCloseScript(CloseReason::kCancelled);
  EXPECT_TRUE(content::ExecJs(web_contents, script));
  EXPECT_FALSE(sharesheet_controller_.last_result);

  // The Sharesheet controller gets called on 'close' if it's been registered.
  nearby_ui->SetSharesheetController(&sharesheet_controller_);
  EXPECT_TRUE(content::ExecJs(web_contents, script));
  EXPECT_EQ(::sharesheet::SharesheetResult::kCancel,
            sharesheet_controller_.last_result);

  // Any subsequent calls to 'close' do not call the Sharesheet controller,
  // since that would result in a crash.
  sharesheet_controller_.last_result.reset();
  EXPECT_TRUE(content::ExecJs(web_contents, script));
  EXPECT_FALSE(sharesheet_controller_.last_result);
}

IN_PROC_BROWSER_TEST_F(NearbyShareDialogUITest, CloseBubbleResults) {
  for (CloseReason reason :
       {CloseReason::kUnknown, CloseReason::kTransferStarted,
        CloseReason::kTransferSucceeded, CloseReason::kCancelled,
        CloseReason::kRejected}) {
    content::WebContents* web_contents = GetWebContentsForNearbyShareHost();
    auto* nearby_ui = web_contents->GetWebUI()
                          ->GetController()
                          ->GetAs<nearby_share::NearbyShareDialogUI>();

    sharesheet_controller_.last_result.reset();
    nearby_ui->SetSharesheetController(&sharesheet_controller_);
    EXPECT_TRUE(content::ExecJs(web_contents, BuildCloseScript(reason)));

    // Verify that the page-closed reason is translated into the correct
    // SharesheetResult and passed into CloseBubble().
    switch (reason) {
      case CloseReason::kTransferStarted:
      case CloseReason::kTransferSucceeded:
        EXPECT_EQ(sharesheet::SharesheetResult::kSuccess,
                  sharesheet_controller_.last_result);
        break;
      case CloseReason::kUnknown:
      case CloseReason::kCancelled:
      case CloseReason::kRejected:
        EXPECT_EQ(sharesheet::SharesheetResult::kCancel,
                  sharesheet_controller_.last_result);
        break;
    }
  }
}
