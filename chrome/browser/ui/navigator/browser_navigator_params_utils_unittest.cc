// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/navigator/browser_navigator_params_utils.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/android/chrome_jni_headers/TabAndroidTestHelper_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#endif

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

class BrowserNavigatorParamsUtilsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

#if BUILDFLAG(IS_ANDROID)
    // On Android, initialize the Java and native Tab objects.
    JNIEnv* env = base::android::AttachCurrentThread();
    java_tab_ = Java_TabAndroidTestHelper_createAndInitializeTabImpl(
        env, 1, profile()->GetJavaObject(),
        static_cast<int32_t>(TabModel::TabLaunchType::FROM_LINK));
    EXPECT_FALSE(java_tab_.is_null()) << "Java tab creation failed.";

    tab_android_ = TabAndroid::GetNativeTab(env, java_tab_);
    EXPECT_NE(tab_android_, nullptr)
        << "Failed to get native TabAndroid from Java TabImpl";
#endif

    mock_browser_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    mock_tab_list_ =
        std::make_unique<testing::NiceMock<MockTabListInterface>>();

    EXPECT_CALL(*mock_browser_, GetProfile()).WillRepeatedly(Return(profile()));
    const MockBrowserWindowInterface* const_mock_browser = mock_browser_.get();
    EXPECT_CALL(*const_mock_browser, GetUnownedUserDataHost())
        .WillRepeatedly(ReturnRef(unowned_user_data_host_));

    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            mock_browser_->GetUnownedUserDataHost(), *mock_tab_list_);

    // Setup 3 dummy tabs.
    for (int i = 0; i < 3; ++i) {
      auto web_contents =
          content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
      auto mock_tab = std::make_unique<tabs::MockTabInterface>();
      EXPECT_CALL(*mock_tab, GetContents())
          .WillRepeatedly(Return(web_contents.get()));
      web_contents_list_.push_back(std::move(web_contents));
      mock_tabs_.push_back(std::move(mock_tab));
    }

    content::WebContentsTester::For(web_contents_list_[0].get())
        ->NavigateAndCommit(GURL(kUrl1));
    content::WebContentsTester::For(web_contents_list_[1].get())
        ->NavigateAndCommit(GURL(kUrl2));
    content::WebContentsTester::For(web_contents_list_[2].get())
        ->NavigateAndCommit(GURL(kUrl3));

    EXPECT_CALL(*mock_tab_list_, GetTabCount())
        .WillRepeatedly(Return(mock_tabs_.size()));
    EXPECT_CALL(*mock_tab_list_, GetActiveIndex()).WillRepeatedly(Return(0));

    ON_CALL(*mock_tab_list_, GetTab(_))
        .WillByDefault([this](int index) -> tabs::TabInterface* {
          if (index >= 0 && index < static_cast<int>(mock_tabs_.size())) {
            return mock_tabs_[index].get();
          }
          return nullptr;
        });
  }

  void TearDown() override {
    tab_list_registration_.reset();
    mock_tab_list_.reset();
    mock_browser_.reset();
    mock_tabs_.clear();
    web_contents_list_.clear();

#if BUILDFLAG(IS_ANDROID)
    DeleteContents();
    tab_android_ = nullptr;
    if (!java_tab_.is_null()) {
      JNIEnv* env = base::android::AttachCurrentThread();
      // Call the destroy() method on the Java TabImpl object.
      // This will trigger TabAndroid::Destroy() via JNI.
      auto tab_impl_class =
          jni_zero::AdoptRef(env, env->GetObjectClass(java_tab_.obj()));
      EXPECT_FALSE(tab_impl_class.is_null());

      jmethodID destroy_method =
          env->GetMethodID(tab_impl_class.obj(), "destroy", "()I");
      EXPECT_NE(nullptr, destroy_method)
          << "Failed to find TabImpl.destroy() method";
      env->CallIntMethod(java_tab_.obj(), destroy_method);
    }
    java_tab_.Reset();
#endif

    ChromeRenderViewHostTestHarness::TearDown();
  }

  NavigateParams NavigateParamsForTest(
      GURL url,
      WindowOpenDisposition disposition =
          WindowOpenDisposition::SINGLETON_TAB) {
    NavigateParams params(profile(), url,
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    params.disposition = disposition;
    return params;
  }

  MockBrowserWindowInterface* browser() { return mock_browser_.get(); }

#if BUILDFLAG(IS_ANDROID)
  TabAndroid* tab_android() const { return tab_android_; }
#endif

  const char* kUrl1 = "http://1.chromium.org/1";
  const char* kUrl2 = "http://2.chromium.org/2";
  const char* kUrl3 = "https://3.chromium.org/3";

 private:
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>> mock_browser_;
  std::unique_ptr<testing::NiceMock<MockTabListInterface>> mock_tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  ui::UnownedUserDataHost unowned_user_data_host_;

  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
  std::vector<std::unique_ptr<tabs::MockTabInterface>> mock_tabs_;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_tab_;
  raw_ptr<TabAndroid> tab_android_ = nullptr;
#endif
};

TEST_F(BrowserNavigatorParamsUtilsTest, FindsExactMatch) {
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(
                browser(), NavigateParamsForTest(GURL(kUrl1))),
            0);
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(
                browser(), NavigateParamsForTest(GURL(kUrl2))),
            1);
}

TEST_F(BrowserNavigatorParamsUtilsTest, FindsWithDifferentRef) {
  auto params = NavigateParamsForTest(GURL(kUrl1).Resolve("/1#ref"));
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), 0);
}

TEST_F(BrowserNavigatorParamsUtilsTest, DoesNotFindNonSingletonDisposition) {
  auto params = NavigateParamsForTest(
      GURL(kUrl1), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), -1);
}

TEST_F(BrowserNavigatorParamsUtilsTest, DoesNotFindViewSource) {
  auto params =
      NavigateParamsForTest(GURL("view-source:http://1.chromium.org/1"));
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), -1);
}

TEST_F(BrowserNavigatorParamsUtilsTest, DoesNotFindDifferentPath) {
  auto params = NavigateParamsForTest(GURL(kUrl1).Resolve("/a"));
  EXPECT_EQ(GetIndexOfExistingTabMatchingURL(browser(), params), -1);
}

TEST_F(BrowserNavigatorParamsUtilsTest,
       DoesNotOverrideUserAgentForNullContents) {
  auto params = NavigateParamsForTest(GURL(kUrl1));
  EXPECT_EQ(
      LoadURLParamsFromNavigateParams(nullptr, &params).override_user_agent,
      content::NavigationController::UA_OVERRIDE_INHERIT);
}

TEST_F(BrowserNavigatorParamsUtilsTest,
       DoesNotOverrideUserAgentWithoutTabAndroid) {
  auto params = NavigateParamsForTest(GURL(kUrl1));
  EXPECT_EQ(LoadURLParamsFromNavigateParams(web_contents(), &params)
                .override_user_agent,
            content::NavigationController::UA_OVERRIDE_INHERIT);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(BrowserNavigatorParamsUtilsTest, OverridesUserAgentOnAndroid) {
  ASSERT_NE(tab_android(), nullptr);
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       tab_android());
  ASSERT_EQ(TabAndroid::FromWebContents(web_contents()), tab_android());

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ASSERT_TRUE(content_settings);
  content_settings->SetContentSettingDefaultScope(
      GURL(kUrl1), GURL(kUrl1), ContentSettingsType::REQUEST_DESKTOP_SITE,
      CONTENT_SETTING_ALLOW);

  auto params = NavigateParamsForTest(GURL(kUrl1));
  EXPECT_EQ(LoadURLParamsFromNavigateParams(web_contents(), &params)
                .override_user_agent,
            content::NavigationController::UA_OVERRIDE_TRUE);
}

TEST_F(BrowserNavigatorParamsUtilsTest,
       DoesNotOverrideUserAgentForSubframeNavigationsOnAndroid) {
  ASSERT_NE(tab_android(), nullptr);
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       tab_android());
  ASSERT_EQ(TabAndroid::FromWebContents(web_contents()), tab_android());

  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings->SetContentSettingDefaultScope(
      GURL(kUrl1), GURL(kUrl1), ContentSettingsType::REQUEST_DESKTOP_SITE,
      CONTENT_SETTING_ALLOW);

  // Navigate the outer frame using LoadURLParamsFromNavigateParams to naturally
  // establish UA_OVERRIDE_TRUE via the Java TabImpl.
  auto main_params = NavigateParamsForTest(GURL(kUrl1));
  auto main_load_url_params =
      LoadURLParamsFromNavigateParams(web_contents(), &main_params);
  EXPECT_EQ(main_load_url_params.override_user_agent,
            content::NavigationController::UA_OVERRIDE_TRUE);
  web_contents()->GetController().LoadURLWithParams(main_load_url_params);
  content::WebContentsTester::For(web_contents())->CommitPendingNavigation();
  ASSERT_TRUE(web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetIsOverridingUserAgent());

  // Create a child frame (subframe).
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  ASSERT_TRUE(subframe);
  // Initiate navigation parameters targeting the subframe.
  auto subframe_params = NavigateParamsForTest(GURL(kUrl1));
  subframe_params.frame_tree_node_id = subframe->GetFrameTreeNodeId();

  auto subframe_load_url_params =
      LoadURLParamsFromNavigateParams(web_contents(), &subframe_params);

  // Subframe navigations must inherit the outer frame's user agent override.
  EXPECT_EQ(subframe_load_url_params.override_user_agent,
            content::NavigationController::UA_OVERRIDE_INHERIT);

  // Validate that the outer frame maintains its user agent override setting.
  EXPECT_TRUE(web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetIsOverridingUserAgent());
}

DEFINE_JNI(TabAndroidTestHelper)
#endif  // BUILDFLAG(IS_ANDROID)
