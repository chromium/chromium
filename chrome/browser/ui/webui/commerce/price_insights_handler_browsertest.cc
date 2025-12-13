// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/price_insights_handler.h"

#include "base/json/json_reader.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"

namespace commerce {

class PriceInsightsHandlerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    side_panel_ui_ =
        std::make_unique<ShoppingInsightsSidePanelUI>(web_ui_.get());
    profile_ = Profile::FromBrowserContext(web_contents()->GetBrowserContext());

    handler_ = std::make_unique<commerce::PriceInsightsHandler>(
        mojo::PendingReceiver<price_insights::mojom::PriceInsightsHandler>(),
        *side_panel_ui_.get(), profile_);
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    profile_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  raw_ptr<Profile> profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ShoppingInsightsSidePanelUI> side_panel_ui_;
  std::unique_ptr<PriceInsightsHandler> handler_;
};

// The feedback dialog on CrOS happens at the system level, which cannot be
// easily tested here.
#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(PriceInsightsHandlerBrowserTest, TestShowFeedback) {
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  handler_->ShowFeedback();

  // Feedback dialog should be non-null with correct meta data.
  CHECK(FeedbackDialog::GetInstanceForTest());
  EXPECT_EQ(chrome::kChromeUIFeedbackURL,
            FeedbackDialog::GetInstanceForTest()->GetDialogContentURL());
  std::optional<base::Value::Dict> meta_data = base::JSONReader::ReadDict(
      FeedbackDialog::GetInstanceForTest()->GetDialogArgs(),
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(meta_data.has_value());
  ASSERT_EQ(*meta_data->FindString("categoryTag"), "price_insights");
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace commerce
