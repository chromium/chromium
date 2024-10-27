// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_embedding_context.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"

namespace webui {
namespace {

class MockBrowserWindowInterfaceWithCloseListener
    : public MockBrowserWindowInterface {
 public:
  ~MockBrowserWindowInterfaceWithCloseListener() override {
    browser_did_close_callback_list_.Notify(this);
  }

  // MockBrowserWindowInterface:
  base::CallbackListSubscription RegisterBrowserDidClose(
      BrowserDidCloseCallback callback) override {
    return browser_did_close_callback_list_.Add(std::move(callback));
  }

 private:
  using BrowserDidCloseCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  BrowserDidCloseCallbackList browser_did_close_callback_list_;
};

}  // namespace

class WebUIEmbeddingContextTest : public testing::Test {
 public:
  content::WebContents* CreateWebContents() {
    return web_contents_factory_.CreateWebContents(profile());
  }

  Profile* profile() { return &profile_; }
  BrowserWindowInterface* browser() { return &browser_window_interface_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebContentsFactory web_contents_factory_;
  MockBrowserWindowInterfaceWithCloseListener browser_window_interface_;
};

TEST_F(WebUIEmbeddingContextTest, SingleBrowserSingleHost) {
  // Create a new host, assert the browser starts null.
  content::WebContents* host_contents = CreateWebContents();
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents));

  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription subscription =
      RegisterBrowserWindowInterfaceChanged(host_contents,
                                            browser_changed_callback.Get());

  // Set the browser on the host.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetBrowserWindowInterface(host_contents, browser());
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Reset the host's browser.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetBrowserWindowInterface(host_contents, nullptr);
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents));
}

TEST_F(WebUIEmbeddingContextTest, SingleBrowserMultipleHosts) {
  // Create a two hosts, assert the browser starts null.
  content::WebContents* host_contents_1 = CreateWebContents();
  content::WebContents* host_contents_2 = CreateWebContents();

  EXPECT_FALSE(GetBrowserWindowInterface(host_contents_1));
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents_2));

  base::MockCallback<base::RepeatingClosure> browser_changed_callback_1;
  base::MockCallback<base::RepeatingClosure> browser_changed_callback_2;
  base::CallbackListSubscription subscription_1 =
      RegisterBrowserWindowInterfaceChanged(host_contents_1,
                                            browser_changed_callback_1.Get());
  base::CallbackListSubscription subscription_2 =
      RegisterBrowserWindowInterfaceChanged(host_contents_2,
                                            browser_changed_callback_2.Get());

  // Set the browser on the first host.
  EXPECT_CALL(browser_changed_callback_1, Run).Times(1);
  EXPECT_CALL(browser_changed_callback_2, Run).Times(0);
  SetBrowserWindowInterface(host_contents_1, browser());
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents_1));
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents_2));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback_1);
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback_2);

  // Set the browser on the second host.
  EXPECT_CALL(browser_changed_callback_1, Run).Times(0);
  EXPECT_CALL(browser_changed_callback_2, Run).Times(1);
  SetBrowserWindowInterface(host_contents_2, browser());
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents_1));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents_2));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback_1);
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback_2);

  // Reset the browser on the first host.
  EXPECT_CALL(browser_changed_callback_1, Run).Times(1);
  EXPECT_CALL(browser_changed_callback_2, Run).Times(0);
  SetBrowserWindowInterface(host_contents_1, nullptr);
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents_1));
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents_2));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback_1);
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback_2);

  // Reset the browser on the second host.
  EXPECT_CALL(browser_changed_callback_1, Run).Times(0);
  EXPECT_CALL(browser_changed_callback_2, Run).Times(1);
  SetBrowserWindowInterface(host_contents_2, nullptr);
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents_1));
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents_2));
}

TEST_F(WebUIEmbeddingContextTest, MultipleBrowsersSingleHost) {
  // Create a second browser.
  MockBrowserWindowInterfaceWithCloseListener browser_2;

  // Create a new host, assert the browser starts null.
  content::WebContents* host_contents = CreateWebContents();
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents));

  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription subscription =
      RegisterBrowserWindowInterfaceChanged(host_contents,
                                            browser_changed_callback.Get());

  // Set the first browser on the host.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetBrowserWindowInterface(host_contents, browser());
  EXPECT_EQ(browser(), GetBrowserWindowInterface(host_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Set the second browser on the host.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetBrowserWindowInterface(host_contents, &browser_2);
  EXPECT_EQ(&browser_2, GetBrowserWindowInterface(host_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Reset the host's browser.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetBrowserWindowInterface(host_contents, nullptr);
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents));
}

TEST_F(WebUIEmbeddingContextTest, SingleBrowserDestruction) {
  // Create a new browser.
  auto test_browser =
      std::make_unique<MockBrowserWindowInterfaceWithCloseListener>();

  // Create a new host, assert the browser starts null.
  content::WebContents* host_contents = CreateWebContents();
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents));

  base::MockCallback<base::RepeatingClosure> browser_changed_callback;
  base::CallbackListSubscription subscription =
      RegisterBrowserWindowInterfaceChanged(host_contents,
                                            browser_changed_callback.Get());

  // Set the test_browser on the host.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  SetBrowserWindowInterface(host_contents, test_browser.get());
  EXPECT_EQ(test_browser.get(), GetBrowserWindowInterface(host_contents));
  testing::Mock::VerifyAndClearExpectations(&browser_changed_callback);

  // Destroy the browser with the host still alive.
  EXPECT_CALL(browser_changed_callback, Run).Times(1);
  test_browser.reset();
  EXPECT_FALSE(GetBrowserWindowInterface(host_contents));
}

}  // namespace webui
