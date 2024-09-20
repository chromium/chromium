// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

namespace {

const char* kTestURL = "chrome://test";

}  // namespace

class Profile;

class TestWebUIController : public TopChromeWebUIController {
  WEB_UI_CONTROLLER_TYPE_DECL();
};
WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIController)

template <>
class WebUIContentsWrapperT<TestWebUIController> : public WebUIContentsWrapper {
 public:
  WebUIContentsWrapperT(const GURL& webui_url,
                        Profile* profile,
                        int task_manager_string_id,
                        bool webui_resizes_host = true,
                        bool esc_closes_ui = true,
                        bool supports_draggable_regions = false)
      : WebUIContentsWrapper(webui_url,
                             profile,
                             task_manager_string_id,
                             webui_resizes_host,
                             esc_closes_ui,
                             supports_draggable_regions,
                             "Test") {}
  void ReloadWebContents() override {}
  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<WebUIContentsWrapper> weak_ptr_factory_{this};
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
    ASSERT_TRUE(profile_manager_.SetUp());
    ChromeViewsTestBase::SetUp();
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  TestingProfileManager profile_manager_;
};

TEST_F(WebUIBubbleManagerTest, CreateWebUIBubbleDialogWithAnchorProvided) {
  const char* kProfileName = "Person 1";
  auto* test_profile = profile_manager()->CreateTestingProfile(kProfileName);

  std::unique_ptr<views::Widget> anchor_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  auto bubble_manager = WebUIBubbleManager::Create<TestWebUIController>(
      anchor_widget->GetContentsView(), test_profile, GURL(kTestURL), 1);
  bubble_manager->DisableCloseBubbleHelperForTesting();

  gfx::Rect anchor(666, 666, 0, 0);
  bubble_manager->ShowBubble(anchor);
  auto bubble_view = bubble_manager->bubble_view_for_testing();

  EXPECT_EQ(bubble_view->GetAnchorRect(), anchor);
}
