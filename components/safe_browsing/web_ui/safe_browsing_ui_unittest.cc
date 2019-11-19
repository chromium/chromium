// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingUITest : public testing::Test {
 public:
  SafeBrowsingUITest() {}

  void SetUp() override {}

  int SetMemberInt(int member_int) {
    member_int_ = member_int;
    return member_int_;
  }

  SafeBrowsingUIHandler* RegisterNewHandler() {
    auto handler_unique =
        std::make_unique<SafeBrowsingUIHandler>(&browser_context_);

    SafeBrowsingUIHandler* handler = handler_unique.get();
    handler->SetWebUIForTesting(&web_ui_);
    WebUIInfoSingleton::GetInstance()->RegisterWebUIInstance(handler);

    web_ui_.AddMessageHandler(std::move(handler_unique));
    return handler;
  }

  void UnregisterHandler(SafeBrowsingUIHandler* handler) {
    WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(handler);
  }

 protected:
  int member_int_;
  content::TestWebUI web_ui_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(SafeBrowsingUITest, CRSBLOGDoesNotEvaluateWhenNoListeners) {
  member_int_ = 0;

  // Start with no listeners, so SetMemberInt() should not be evaluated.
  CRSBLOG << SetMemberInt(1);
  EXPECT_EQ(member_int_, 0);

  // Register a listener, so SetMemberInt() will be evaluated.
  SafeBrowsingUIHandler* handler = RegisterNewHandler();

  CRSBLOG << SetMemberInt(1);
  EXPECT_EQ(member_int_, 1);

  UnregisterHandler(handler);
}

}  // namespace safe_browsing
