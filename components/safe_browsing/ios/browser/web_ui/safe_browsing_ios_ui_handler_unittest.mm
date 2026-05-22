// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/web_ui/safe_browsing_ios_ui_handler.h"

#import "components/safe_browsing/ios/browser/web_ui/safe_browsing_ui.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace safe_browsing {

class MockSafeBrowsingLocalStateDelegate
    : public safe_browsing::SafeBrowsingLocalStateDelegate {
 public:
  MockSafeBrowsingLocalStateDelegate() = default;
  ~MockSafeBrowsingLocalStateDelegate() override = default;
  PrefService* GetLocalState() override { return nullptr; }
};

class SafeBrowsingIOSUIHandlerTest : public PlatformTest {
 public:
  int SetMemberInt(int val) {
    member_int_ = val;
    return val;
  }

 protected:
  SafeBrowsingIOSUIHandlerTest() = default;
  ~SafeBrowsingIOSUIHandlerTest() override = default;

  int member_int_ = 0;
  web::WebTaskEnvironment task_environment_;
  web::FakeBrowserState browser_state_;
};

TEST_F(SafeBrowsingIOSUIHandlerTest, CRSBLOGDoesNotEvaluateWhenNoListeners) {
  member_int_ = 0;

  // Start with no listeners, so SetMemberInt() should not be evaluated.
  CRSBLOG << SetMemberInt(1);
  EXPECT_EQ(member_int_, 0);
}

TEST_F(SafeBrowsingIOSUIHandlerTest, TestWebUIObserverRegistration) {
  // Initially, the WebUI singleton should have no active listeners.
  EXPECT_FALSE(WebUIIOSInfoSingleton::GetInstance()->HasListener());

  {
    auto delegate = std::make_unique<MockSafeBrowsingLocalStateDelegate>();
    // Instantiating the handler should automatically register the observer.
    SafeBrowsingIOSUIHandler handler(&browser_state_, std::move(delegate),
                                     nullptr);

    EXPECT_TRUE(WebUIIOSInfoSingleton::GetInstance()->HasListener());
  }

  // Once the handler is destroyed, it should unregister the observer.
  EXPECT_FALSE(WebUIIOSInfoSingleton::GetInstance()->HasListener());
}

}  // namespace safe_browsing
