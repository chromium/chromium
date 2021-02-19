// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class TestWebUIController;

namespace {

const char* kTestURL = "chrome://test";

// An observer that returns back to test code after a new profile is
// initialized.
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::BindRepeating(&UnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(new_path);
}

std::unique_ptr<WebUIBubbleManagerT<TestWebUIController>> CreateBubbleManager(
    Browser* browser) {
  return std::make_unique<WebUIBubbleManagerT<TestWebUIController>>(
      BrowserView::GetBrowserViewForBrowser(browser), browser->profile(),
      GURL(kTestURL), 1, false);
}

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
                         bool enable_extension_apis = false)
      : BubbleContentsWrapper(browser_context,
                              task_manager_string_id,
                              enable_extension_apis) {}
  void ReloadWebContents() override {}
};

class WebUIBubbleManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebUIBubbleManagerBrowserTest() = default;
  WebUIBubbleManagerBrowserTest(const WebUIBubbleManagerBrowserTest&) = delete;
  const WebUIBubbleManagerBrowserTest& operator=(
      const WebUIBubbleManagerBrowserTest&) = delete;
  ~WebUIBubbleManagerBrowserTest() override = default;

  // content::BrowserTestBase:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    bubble_manager_ = CreateBubbleManager(browser());
  }
  void TearDownOnMainThread() override {
    auto* widget = bubble_manager_->GetBubbleWidget();
    if (widget)
      widget->CloseNow();
    bubble_manager_->ResetContentsWrapperForTesting();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  WebUIBubbleManager* bubble_manager() { return bubble_manager_.get(); }

 private:
  std::unique_ptr<WebUIBubbleManager> bubble_manager_;
};

IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest, CreateAndCloseBubble) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());

  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest,
                       ShowUISetsBubbleWidgetVisible) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());

  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
}

class WebUIBubblePersistenceTest : public WebUIBubbleManagerBrowserTest {
 public:
  // WebUIBubbleManagerBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kWebUIBubblePerProfilePersistence}, {});
    WebUIBubbleManagerBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIBubblePersistenceTest,
                       PerProfileContentsWrapperNotUsedForOffTheRecordProfile) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  auto manager =
      CreateBubbleManager(CreateBrowser(incognito_browser->profile()));

  auto* service = BubbleContentsWrapperServiceFactory::GetForProfile(
      incognito_browser->profile(), true);
  ASSERT_EQ(nullptr, service);

  // Open the bubble for the given bubble manager
  EXPECT_EQ(nullptr, manager->GetBubbleWidget());
  manager->ShowBubble();
  EXPECT_NE(nullptr, manager->GetBubbleWidget());

  // The contents wrapper should exist despite `service` not existing for the
  // off the record profile.
  EXPECT_NE(
      nullptr,
      manager->bubble_view_for_testing()->get_contents_wrapper_for_testing());

  manager->CloseBubble();
  EXPECT_TRUE(manager->GetBubbleWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(WebUIBubblePersistenceTest,
                       UsesPersistentContentsWrapperPerProfile) {
  auto* service = BubbleContentsWrapperServiceFactory::GetForProfile(
      browser()->profile(), true);
  ASSERT_NE(nullptr, service);

  // If using per-profile peristence the `contents_wrapper` should have been
  // created before the bubble has been invoked.
  BubbleContentsWrapper* contents_wrapper =
      service->GetBubbleContentsWrapperFromURL(GURL("chrome://test"));
  EXPECT_NE(nullptr, contents_wrapper);

  // Open the bubble, the `contents_wrapper` used should match the one returned
  // from the BubbleContentsWrapperService.
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());
  EXPECT_EQ(contents_wrapper, bubble_manager()
                                  ->bubble_view_for_testing()
                                  ->get_contents_wrapper_for_testing());

  // After closing the bubble the `contents_wrapper` should continue to persist.
  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
  EXPECT_EQ(contents_wrapper,
            service->GetBubbleContentsWrapperFromURL(GURL("chrome://test")));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // No multi-profile on ChromeOS.

IN_PROC_BROWSER_TEST_F(WebUIBubblePersistenceTest,
                       UsesPersistentContentsWrapperPerProfileMultiProfile) {
  Profile* profile1 = CreateProfile();
  Profile* profile2 = CreateProfile();

  auto* service1 =
      BubbleContentsWrapperServiceFactory::GetForProfile(profile1, true);
  auto* service2 =
      BubbleContentsWrapperServiceFactory::GetForProfile(profile2, true);

  auto manager1 = CreateBubbleManager(CreateBrowser(profile1));
  auto manager2 = CreateBubbleManager(CreateBrowser(profile1));
  auto manager3 = CreateBubbleManager(CreateBrowser(profile2));

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
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
