// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_web_contents.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
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

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
  }

  void TearDown() override {
    chrome_keyboard_web_contents_.reset();
    chrome_keyboard_controller_client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CreateWebContents(const GURL& url,
                         ChromeKeyboardWebContents::LoadCallback callback) {
    chrome_keyboard_web_contents_ = std::make_unique<ChromeKeyboardWebContents>(
        profile(), url, std::move(callback), base::NullCallback());
  }

 protected:
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
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
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
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
