// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_review_handler.h"

#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kCallbackId[] = "test-callback-id";

class TestingChildProfile : public TestingProfile {
 public:
  bool IsChild() const override { return true; }
};

}  // namespace

namespace settings {

class PrivacyReviewHandlerTest : public testing::Test {
 public:
  PrivacyReviewHandlerTest() = default;

  void SetUp() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(test_web_contents());
    handler_ = std::make_unique<PrivacyReviewHandler>();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();
  }

  void ValidateIsPrivacyReviewAvailable(bool privacyReviewAvailableExpected) {
    base::ListValue args;
    args.Append(kCallbackId);
    handler()->HandleIsPrivacyReviewAvailable(args.GetList());

    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    EXPECT_EQ(kCallbackId, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());

    ASSERT_TRUE(data.arg3()->is_bool());
    EXPECT_EQ(privacyReviewAvailableExpected, data.arg3()->GetBool());
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  content::WebContents* test_web_contents() { return web_contents_.get(); }
  PrivacyReviewHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment browser_task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_ =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PrivacyReviewHandler> handler_;
};

class PrivacyReviewHandlerDisabledTest : public PrivacyReviewHandlerTest {
 public:
  PrivacyReviewHandlerDisabledTest() {
    feature_list_.InitAndDisableFeature(features::kPrivacyReview);
  }
};

class PrivacyReviewHandlerEnabledTest : public PrivacyReviewHandlerTest {
 public:
  PrivacyReviewHandlerEnabledTest() {
    feature_list_.InitAndEnableFeature(features::kPrivacyReview);
  }
};

// TODO(crbug.com/1277422): Flaky on Linux TSan Tests as one particular instance
// of crbug/crbug.com/1275502.
#if defined(THREAD_SANITIZER)
#define MAYBE_IsDisabledPrivacyReviewAvailable \
  DISABLED_IsDisabledPrivacyReviewAvailable
#else
#define MAYBE_IsDisabledPrivacyReviewAvailable IsDisabledPrivacyReviewAvailable
#endif
TEST_F(PrivacyReviewHandlerDisabledTest,
       MAYBE_IsDisabledPrivacyReviewAvailable) {
  // Privacy review is not available when the experimental flag is disabled.
  ValidateIsPrivacyReviewAvailable(false);
}

// TODO(crbug.com/1277422): Flaky on Linux TSan Tests as one particular instance
// of crbug/crbug.com/1275502.
#if defined(THREAD_SANITIZER)
#define MAYBE_IsEnabledPrivacyReviewAvailable \
  DISABLED_IsEnabledPrivacyReviewAvailable
#else
#define MAYBE_IsEnabledPrivacyReviewAvailable IsEnabledPrivacyReviewAvailable
#endif
TEST_F(PrivacyReviewHandlerEnabledTest, MAYBE_IsEnabledPrivacyReviewAvailable) {
  // Privacy review is available when the experimental flag is enabled.
  ValidateIsPrivacyReviewAvailable(true);

  // If the browser is managed, then privacy review is not available.
  {
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(true);
    std::unique_ptr<TestingProfile> profile = builder.Build();

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile.get()));

    web_ui()->set_web_contents(web_contents.get());
    ValidateIsPrivacyReviewAvailable(false);

    web_ui()->set_web_contents(test_web_contents());
    ValidateIsPrivacyReviewAvailable(true);
  }

  // If the users is a child, then privacy review is not available.
  {
    TestingChildProfile profile;

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(&profile));

    web_ui()->set_web_contents(web_contents.get());
    ValidateIsPrivacyReviewAvailable(false);
  }
}

}  // namespace settings
