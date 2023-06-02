// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_web_contents_factory.h"

class FedCmModalDialogView;

namespace {

class FedCmModalDialogViewTest : public ChromeViewsTestBase {
 public:
  FedCmModalDialogViewTest() {
    web_contents_ = web_contents_factory_.CreateWebContents(&testing_profile_);
  }

 protected:
  content::WebContents* web_contents() { return web_contents_; }

 private:
  TestingProfile testing_profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

class TestDelegate : public content::WebContentsDelegate {
 public:
  explicit TestDelegate(content::WebContents* contents) {
    contents->SetDelegate(this);
  }
  ~TestDelegate() override = default;

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    opened_++;
    return source;
  }

  int opened() const { return opened_; }

 private:
  int opened_ = 0;
};

}  // namespace

TEST_F(FedCmModalDialogViewTest, ShowPopupWindow) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  std::unique_ptr<FedCmModalDialogView> popup_window =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);
  content::WebContents* web_contents =
      popup_window->ShowPopupWindow(GURL(u"https://example.com"));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate.opened());
  ASSERT_TRUE(web_contents);
}
