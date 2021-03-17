// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace {

const char* kTestURL = "chrome://test";

}  // namespace

class TestWebUIController : public ui::MojoBubbleWebUIController {
  WEB_UI_CONTROLLER_TYPE_DECL();
};
WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIController)

template <>
class BubbleContentsWrapperT<TestWebUIController>
    : public BubbleContentsWrapper {
 public:
  BubbleContentsWrapperT(const GURL& webui_url,
                         content::BrowserContext* browser_context,
                         int task_manager_string_id,
                         bool enable_extension_apis = false,
                         bool webui_resizes_host = true)
      : BubbleContentsWrapper(browser_context,
                              task_manager_string_id,
                              enable_extension_apis,
                              webui_resizes_host) {}
  void ReloadWebContents() override {}
};

class WebUIBubbleManagerTest : public ChromeViewsTestBase {
 public:
  WebUIBubbleManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  WebUIBubbleManagerTest(const WebUIBubbleManagerTest&) = delete;
  WebUIBubbleManagerTest& operator=(const WebUIBubbleManagerTest&) = delete;
  ~WebUIBubbleManagerTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kWebUIBubblePerProfilePersistence}, {});
    ASSERT_TRUE(profile_manager_.SetUp());
    ChromeViewsTestBase::SetUp();
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
};

TEST_F(WebUIBubbleManagerTest, UsesPersistentContentsWrapperPerProfile) {
  const char* kProfileName = "Person 1";
  auto* test_profile = profile_manager()->CreateTestingProfile(kProfileName);

  auto* service =
      BubbleContentsWrapperServiceFactory::GetForProfile(test_profile, true);
  ASSERT_NE(nullptr, service);

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
  auto bubble_manager =
      std::make_unique<WebUIBubbleManagerT<TestWebUIController>>(
          anchor_widget->GetContentsView(), test_profile, GURL(kTestURL), 1,
          false);
  bubble_manager->DisableCloseBubbleHelperForTesting();

  // If using per-profile peristence the `contents_wrapper` should have been
  // created before the bubble has been invoked.
  BubbleContentsWrapper* contents_wrapper =
      service->GetBubbleContentsWrapperFromURL(GURL(kTestURL));
  EXPECT_NE(nullptr, contents_wrapper);

  // Open the bubble, the `contents_wrapper` used should match the one returned
  // from the BubbleContentsWrapperService.
  EXPECT_EQ(nullptr, bubble_manager->GetBubbleWidget());
  bubble_manager->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager->GetBubbleWidget()->IsClosed());
  EXPECT_EQ(contents_wrapper, bubble_manager->bubble_view_for_testing()
                                  ->get_contents_wrapper_for_testing());

  // After closing the bubble the `contents_wrapper` should continue to persist.
  bubble_manager->CloseBubble();
  EXPECT_TRUE(bubble_manager->GetBubbleWidget()->IsClosed());
  EXPECT_EQ(contents_wrapper,
            service->GetBubbleContentsWrapperFromURL(GURL(kTestURL)));

  profile_manager()->DeleteTestingProfile(kProfileName);
}

TEST_F(WebUIBubbleManagerTest,
       PerProfileContentsWrapperNotUsedForOffTheRecordProfile) {
  const char* kProfileName = "Person 1";
  auto* test_profile = profile_manager()->CreateTestingProfile(kProfileName);
  auto* otr_profile = test_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID("Test::WebUIBubble"));

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
  auto bubble_manager =
      std::make_unique<WebUIBubbleManagerT<TestWebUIController>>(
          anchor_widget->GetContentsView(), otr_profile, GURL(kTestURL), 1,
          false);
  bubble_manager->DisableCloseBubbleHelperForTesting();

  // The service should not exist for off the record profiles.
  auto* service =
      BubbleContentsWrapperServiceFactory::GetForProfile(otr_profile, true);
  ASSERT_EQ(nullptr, service);

  // Open the bubble for the given bubble manager
  EXPECT_EQ(nullptr, bubble_manager->GetBubbleWidget());
  bubble_manager->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager->GetBubbleWidget());

  // The contents wrapper should exist despite `service` not existing for the
  // off the record profile.
  EXPECT_NE(nullptr, bubble_manager->bubble_view_for_testing()
                         ->get_contents_wrapper_for_testing());

  bubble_manager->CloseBubble();
  EXPECT_TRUE(bubble_manager->GetBubbleWidget()->IsClosed());

  bubble_manager->ResetContentsWrapperForTesting();
  profile_manager()->DeleteTestingProfile(kProfileName);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // No multi-profile on ChromeOS.

TEST_F(WebUIBubbleManagerTest,
       UsesPersistentContentsWrapperPerProfileMultiProfile) {
  const char* kProfileName1 = "Person 1";
  const char* kProfileName2 = "Person 2";
  auto* profile1 = profile_manager()->CreateTestingProfile(kProfileName1);
  auto* profile2 = profile_manager()->CreateTestingProfile(kProfileName2);

  auto* service1 =
      BubbleContentsWrapperServiceFactory::GetForProfile(profile1, true);
  auto* service2 =
      BubbleContentsWrapperServiceFactory::GetForProfile(profile2, true);

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
  auto create_manager = [&](Profile* profile) {
    auto manager = std::make_unique<WebUIBubbleManagerT<TestWebUIController>>(
        anchor_widget->GetContentsView(), profile, GURL(kTestURL), 1, false);
    manager->DisableCloseBubbleHelperForTesting();
    return manager;
  };
  auto manager1 = create_manager(profile1);
  auto manager2 = create_manager(profile1);
  auto manager3 = create_manager(profile2);

  BubbleContentsWrapper* contents_wrapper_profile1 =
      service1->GetBubbleContentsWrapperFromURL(GURL(kTestURL));
  BubbleContentsWrapper* contents_wrapper_profile2 =
      service2->GetBubbleContentsWrapperFromURL(GURL(kTestURL));

  // content wrappers for the same WebUI URL should be different per profile.
  ASSERT_NE(nullptr, contents_wrapper_profile1);
  ASSERT_NE(nullptr, contents_wrapper_profile2);
  ASSERT_NE(contents_wrapper_profile1, contents_wrapper_profile2);

  auto test_manager = [](WebUIBubbleManager* manager,
                         BubbleContentsWrapperService* service,
                         BubbleContentsWrapper* expected_wrapper) {
    // Open the bubble for the given bubble manager
    EXPECT_EQ(nullptr, manager->GetBubbleWidget());
    manager->ShowBubble();
    EXPECT_NE(nullptr, manager->GetBubbleWidget());

    // The `expected_wrapper` matches the one used by the bubble.
    EXPECT_EQ(
        expected_wrapper,
        manager->bubble_view_for_testing()->get_contents_wrapper_for_testing());

    // Close the bubble, ensure the service's contents wrapper still exists and
    // matches the `expected_wrapper`.
    EXPECT_FALSE(manager->GetBubbleWidget()->IsClosed());
    manager->CloseBubble();
    EXPECT_TRUE(manager->GetBubbleWidget()->IsClosed());

    EXPECT_EQ(expected_wrapper,
              service->GetBubbleContentsWrapperFromURL(GURL(kTestURL)));
  };

  test_manager(manager1.get(), service1, contents_wrapper_profile1);
  test_manager(manager2.get(), service1, contents_wrapper_profile1);
  test_manager(manager3.get(), service2, contents_wrapper_profile2);
  profile_manager()->DeleteTestingProfile(kProfileName1);
  profile_manager()->DeleteTestingProfile(kProfileName2);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
