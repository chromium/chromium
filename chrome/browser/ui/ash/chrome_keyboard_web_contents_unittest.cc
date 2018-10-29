// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_web_contents.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace {

class ChromeKeyboardWebContentsTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeKeyboardWebContentsTest() = default;
  ~ChromeKeyboardWebContentsTest() override = default;

  void TearDown() override {
    chrome_keyboard_web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateWebContents(const GURL& url,
                         ChromeKeyboardWebContents::LoadCallback callback) {
    chrome_keyboard_web_contents_ = std::make_unique<ChromeKeyboardWebContents>(
        profile(), url, std::move(callback));
  }

 protected:
  std::unique_ptr<ChromeKeyboardWebContents> chrome_keyboard_web_contents_;
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

// Calling SetKeyboardUrl with a different URL should open the new page.
TEST_F(ChromeKeyboardWebContentsTest, SetKeyboardUrl) {
  CreateWebContents(GURL("http://foo.com"), base::DoNothing());
  ASSERT_TRUE(chrome_keyboard_web_contents_->web_contents());

  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(chrome_keyboard_web_contents_->web_contents());

  chrome_keyboard_web_contents_->SetKeyboardUrl(GURL("http://bar.com"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate.opened());
}
