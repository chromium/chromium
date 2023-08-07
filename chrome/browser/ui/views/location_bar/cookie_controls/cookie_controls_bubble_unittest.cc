// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"

#include <memory>
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/vector_icons.h"

class MockCookieControlsBubbleView : public CookieControlsBubbleView {
 public:
  ~MockCookieControlsBubbleView() override = default;

  MOCK_METHOD(void,
              InitContentView,
              (std::unique_ptr<CookieControlsContentView>),
              (override));
  MOCK_METHOD(void,
              InitReloadingView,
              (std::unique_ptr<views::View>),
              (override));

  MOCK_METHOD(void, UpdateTitle, (const std::u16string&), (override));
  MOCK_METHOD(void, UpdateSubtitle, (const std::u16string&), (override));
  MOCK_METHOD(void, UpdateFaviconImage, (const gfx::Image&, int), (override));

  MOCK_METHOD(void, SwitchToReloadingView, (), (override));

  MOCK_METHOD(CookieControlsContentView*, GetContentView, (), (override));
  MOCK_METHOD(views::View*, GetReloadingView, (), (override));

  MOCK_METHOD(void, CloseWidget, (), (override));

  MOCK_METHOD(base::CallbackListSubscription,
              RegisterOnUserClosedContentViewCallback,
              (base::RepeatingClosureList::CallbackType),
              (override));
};

class MockCookieControlsContentView : public CookieControlsContentView {
 public:
  MOCK_METHOD(void,
              UpdateContentLabels,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void, SetToggleIsOn, (bool), (override));
  MOCK_METHOD(void, SetToggleIcon, (const gfx::VectorIcon&), (override));
  MOCK_METHOD(void, SetToggleLabel, (const std::u16string&), (override));
  MOCK_METHOD(void, SetFeedbackSectionVisibility, (bool), (override));
};

class CookieControlsBubbleCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()));

    coordinator_ = std::make_unique<CookieControlsBubbleCoordinator>();

    AddTab(browser(), GURL("http://a.com"));
  }

  void TearDown() override {
    // Clean up the coordinator before the browser is destroyed to avoid
    // dangling pointers.
    coordinator_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  content_settings::CookieControlsController* controller() {
    return controller_.get();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<CookieControlsBubbleCoordinator> coordinator_;

 private:
  std::unique_ptr<content_settings::CookieControlsController> controller_;
};

TEST_F(CookieControlsBubbleCoordinatorTest, ShowBubbleTest) {
  EXPECT_EQ(coordinator_->GetBubble(), nullptr);
  coordinator_->ShowBubble(web_contents(), controller());
  EXPECT_NE(coordinator_->GetBubble(), nullptr);

  views::test::WidgetDestroyedWaiter waiter(
      coordinator_->GetBubble()->GetWidget());
  coordinator_->GetBubble()->GetWidget()->Close();
  waiter.Wait();
  EXPECT_EQ(coordinator_->GetBubble(), nullptr);
}

class CookieControlsBubbleViewControllerTest
    : public TestWithBrowserView,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    std::string expiration = GetParam() ? "30d" : "0d";
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kUserBypassUI,
          {{"expiration", expiration}}}},
        {});
    TestWithBrowserView::SetUp();

    const GURL url = GURL("http://a.com");
    AddTab(browser(), url);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    mock_bubble_view_ =
        std::make_unique<testing::NiceMock<MockCookieControlsBubbleView>>();
    mock_content_view_ =
        std::make_unique<testing::NiceMock<MockCookieControlsContentView>>();

    empty_reloading_view_ = std::make_unique<views::View>();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()));

    ON_CALL(*mock_bubble_view(), GetContentView())
        .WillByDefault(testing::Return(mock_content_view()));
    ON_CALL(*mock_bubble_view(), GetReloadingView())
        .WillByDefault(testing::Return(empty_reloading_view()));

    EXPECT_CALL(*mock_bubble_view(),
                UpdateSubtitle(base::ASCIIToUTF16(url.host())));
    view_controller_ = std::make_unique<CookieControlsBubbleViewController>(
        mock_bubble_view(), controller_.get(), web_contents);
  }

  void TearDown() override {
    // Clean up the pointers in the correct order before the browser is
    // destroyed to avoid dangling pointers.
    view_controller_ = nullptr;
    mock_bubble_view_ = nullptr;
    mock_content_view_ = nullptr;
    controller_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  CookieControlsBubbleViewController* view_controller() {
    return view_controller_.get();
  }

  MockCookieControlsBubbleView* mock_bubble_view() {
    return mock_bubble_view_.get();
  }

  MockCookieControlsContentView* mock_content_view() {
    return mock_content_view_.get();
  }

  views::View* empty_reloading_view() { return empty_reloading_view_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
  std::unique_ptr<MockCookieControlsContentView> mock_content_view_;
  std::unique_ptr<MockCookieControlsBubbleView> mock_bubble_view_;
  std::unique_ptr<views::View> empty_reloading_view_;
  std::unique_ptr<CookieControlsBubbleViewController> view_controller_;
};

TEST_P(CookieControlsBubbleViewControllerTest, ThirdPartyCookiesBlocked) {
  const int kAllowedSitesCount = 2;
  const int kBlockedSitesCount = 3;
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE)));
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE),
          l10n_util::GetStringUTF16(
              GetParam()
                  ? IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY
                  : IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_PERMANENT)));
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(false));
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(false));
  EXPECT_CALL(
      *mock_content_view(),
      SetToggleLabel(l10n_util::GetPluralStringFUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_BLOCKED_SITES_COUNT, kBlockedSitesCount)));
  EXPECT_CALL(*mock_content_view(), SetToggleIcon(testing::Field(
                                        &gfx::VectorIcon::name,
                                        features::IsChromeRefresh2023()
                                            ? views::kEyeCrossedRefreshIcon.name
                                            : views::kEyeCrossedIcon.name)));

  view_controller()->OnStatusChanged(CookieControlsStatus::kEnabled,
                                     CookieControlsEnforcement::kNoEnforcement,
                                     base::Time());
  view_controller()->OnSitesCountChanged(kAllowedSitesCount,
                                         kBlockedSitesCount);
}

TEST_P(CookieControlsBubbleViewControllerTest,
       ThirdPartyCookiesAllowedPermanent) {
  const int kAllowedSitesCount = 2;
  const int kBlockedSitesCount = 3;
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE)));
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_PERMANENT_ALLOWED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION)));
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(true));
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(true));
  EXPECT_CALL(
      *mock_content_view(),
      SetToggleLabel(l10n_util::GetPluralStringFUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_ALLOWED_SITES_COUNT, kAllowedSitesCount)));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(&gfx::VectorIcon::name,
                                           features::IsChromeRefresh2023()
                                               ? views::kEyeRefreshIcon.name
                                               : views::kEyeIcon.name)));

  view_controller()->OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                                     CookieControlsEnforcement::kNoEnforcement,
                                     base::Time());
  view_controller()->OnSitesCountChanged(kAllowedSitesCount,
                                         kBlockedSitesCount);
}

TEST_P(CookieControlsBubbleViewControllerTest,
       ThirdPartyCookiesAllowedTemporary) {
  const int kDaysToExpiration = 30;
  const int kAllowedSitesCount = 2;
  const int kBlockedSitesCount = 3;
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE)));
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetPluralStringFUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_TITLE,
              kDaysToExpiration),
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_DESCRIPTION_TODAY)));
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(true));
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(true));
  EXPECT_CALL(
      *mock_content_view(),
      SetToggleLabel(l10n_util::GetPluralStringFUTF16(
          IDS_COOKIE_CONTROLS_BUBBLE_ALLOWED_SITES_COUNT, kAllowedSitesCount)));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(&gfx::VectorIcon::name,
                                           features::IsChromeRefresh2023()
                                               ? views::kEyeRefreshIcon.name
                                               : views::kEyeIcon.name)));

  view_controller()->OnStatusChanged(
      CookieControlsStatus::kDisabledForSite,
      CookieControlsEnforcement::kNoEnforcement,
      base::Time::Now() + base::Days(kDaysToExpiration));
  view_controller()->OnSitesCountChanged(kAllowedSitesCount,
                                         kBlockedSitesCount);
}

// TODO(crbug.com/1446230): Add tests for enforced cookie controls.

class CookieControlsBubbleViewImplTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    const GURL url = GURL("http://a.com");
    AddTab(browser(), url);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()));

    coordinator_ = std::make_unique<CookieControlsBubbleCoordinator>();
    coordinator_->ShowBubble(web_contents, controller_.get());
  }

  void TearDown() override {
    // Ensure things are destroyed in an appropriate order to ensure pointers
    // are not considered dangling.
    views::test::WidgetDestroyedWaiter waiter(bubble_view()->GetWidget());
    bubble_view()->GetWidget()->Close();
    waiter.Wait();
    EXPECT_EQ(coordinator_->GetBubble(), nullptr);

    coordinator_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  CookieControlsBubbleViewImpl* bubble_view() {
    return coordinator_->GetBubble();
  }

 private:
  std::unique_ptr<CookieControlsBubbleCoordinator> coordinator_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
};

TEST_F(CookieControlsBubbleViewImplTest, BubbleWidth) {
  // Confirm that with extreme label lengths, the width of the bubble remains
  // within an acceptable range.
  const int kMinWidth = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  const int kMaxWidth = 1000;

  EXPECT_GE(bubble_view()->GetPreferredSize().width(), kMinWidth);
  EXPECT_LE(bubble_view()->GetPreferredSize().width(), kMaxWidth);

  bubble_view()->GetContentView()->UpdateContentLabels(
      std::u16string(10000, u'a'), std::u16string(10000, u'b'));
  EXPECT_GE(bubble_view()->GetPreferredSize().width(), kMinWidth);
  EXPECT_LE(bubble_view()->GetPreferredSize().width(), kMaxWidth);

  bubble_view()->GetContentView()->UpdateContentLabels(u"a", u"b");
  EXPECT_GE(bubble_view()->GetPreferredSize().width(), kMinWidth);
  EXPECT_LE(bubble_view()->GetPreferredSize().width(), kMaxWidth);
}

// Runs all tests with two versions of user bypass - one that creates temporary
// exceptions and one that creates permanent exceptions.
INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsBubbleViewControllerTest,
                         testing::Bool());
