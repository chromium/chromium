// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/vector_icons.h"

const int kDaysToExpiration = 30;

using Status = ::content_settings::TrackingProtectionBlockingStatus;
using FeatureType = ::content_settings::TrackingProtectionFeatureType;

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
  explicit MockCookieControlsContentView(bool has_act_features)
      : CookieControlsContentView(has_act_features) {}
  ~MockCookieControlsContentView() override = default;

  MOCK_METHOD(void,
              UpdateContentLabels,
              (const std::u16string&, const std::u16string&),
              (override));
  MOCK_METHOD(void, SetToggleIsOn, (bool), (override));
  MOCK_METHOD(void, SetToggleIcon, (const gfx::VectorIcon&), (override));
  MOCK_METHOD(void, SetCookiesLabel, (const std::u16string&), (override));
  MOCK_METHOD(void, SetFeedbackSectionVisibility, (bool), (override));
  MOCK_METHOD(void, SetContentLabelsVisible, (bool), (override));
  MOCK_METHOD(void, SetToggleVisible, (bool), (override));
  MOCK_METHOD(void,
              SetEnforcedIcon,
              (const gfx::VectorIcon&, const std::u16string&),
              (override));
};

class CookieControlsBubbleCoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()),
        /*tracking_protection_settings*/ nullptr);

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

class CookieControlsBubbleViewControllerTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    const GURL url = GURL("http://a.com");
    AddTab(browser(), url);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    mock_bubble_view_ =
        std::make_unique<testing::NiceMock<MockCookieControlsBubbleView>>();
    mock_content_view_ =
        std::make_unique<testing::NiceMock<MockCookieControlsContentView>>(
            has_act_features_);

    empty_reloading_view_ = std::make_unique<views::View>();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()),
        /*tracking_protection_settings=*/nullptr);

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

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2023 11:00:00", &time));
    return time;
  }

  std::vector<content_settings::TrackingProtectionFeature>
  GetTrackingProtectionFeatures() {
    if (protections_on_) {
      if (blocking_status_ == CookieBlocking3pcdStatus::kLimited) {
        return {
            {FeatureType::kThirdPartyCookies, enforcement_, Status::kLimited}};
      } else {
        return {
            {FeatureType::kThirdPartyCookies, enforcement_, Status::kBlocked}};
      }
    }
    return {{FeatureType::kThirdPartyCookies, enforcement_, Status::kAllowed}};
  }

  void OnStatusChanged(int days_to_expiration = 0) {
    auto expiration = days_to_expiration
                          ? base::Time::Now() + base::Days(days_to_expiration)
                          : base::Time();
    view_controller()->OnStatusChanged(
        controls_visible_, protections_on_, enforcement_, blocking_status_,
        expiration, GetTrackingProtectionFeatures());
  }

 protected:
  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsBubbleViewControllerTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
  std::unique_ptr<MockCookieControlsContentView> mock_content_view_;
  std::unique_ptr<MockCookieControlsBubbleView> mock_bubble_view_;
  std::unique_ptr<views::View> empty_reloading_view_;
  std::unique_ptr<CookieControlsBubbleViewController> view_controller_;
  bool has_act_features_ = false;
  bool controls_visible_ = true;
  bool protections_on_ = true;
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  CookieBlocking3pcdStatus blocking_status_ =
      CookieBlocking3pcdStatus::kNotIn3pcd;
};

TEST_F(CookieControlsBubbleViewControllerTest,
       WidgetClosesWhenControlsAreNotVisible) {
  EXPECT_CALL(*mock_bubble_view(), CloseWidget());
  controls_visible_ = false;
  OnStatusChanged();
}

TEST_F(CookieControlsBubbleViewControllerTest, WidgetClosesOnTpcdEnforcement) {
  EXPECT_CALL(*mock_bubble_view(), CloseWidget());
  enforcement_ = CookieControlsEnforcement::kEnforcedByTpcdGrant;
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  OnStatusChanged();
}

class CookieControlsBubbleViewController3pcdBubbleTitleTest
    : public CookieControlsBubbleViewControllerTest {
 public:
  CookieControlsBubbleViewController3pcdBubbleTitleTest() {
    feature_list_.InitAndDisableFeature(
        privacy_sandbox::kTrackingProtection3pcdUx);
  }
};

TEST_F(CookieControlsBubbleViewController3pcdBubbleTitleTest,
       DisplaysThirdPartyCookiesBlockedTitle) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE)));
  blocking_status_ = CookieBlocking3pcdStatus::kAll;
  OnStatusChanged();
}

TEST_F(CookieControlsBubbleViewController3pcdBubbleTitleTest,
       DisplaysThirdPartyCookiesLimitedTitle) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_LIMITED_TITLE)));
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  OnStatusChanged();
}

TEST_F(CookieControlsBubbleViewController3pcdBubbleTitleTest,
       DisplaysThirdPartyCookiesAllowedTitle) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE)));
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  protections_on_ = false;
  OnStatusChanged(kDaysToExpiration);
}

class CookieControlsBubbleViewController3pcdStatusesTest
    : public CookieControlsBubbleViewControllerTest,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {};

// Verify toggle states
TEST_P(CookieControlsBubbleViewController3pcdStatusesTest,
       DisplaysAllowedToggleForSiteException) {
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(true));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(&gfx::VectorIcon::name,
                                           views::kEyeRefreshIcon.name)));
  EXPECT_CALL(*mock_content_view(),
              SetCookiesLabel(l10n_util::GetStringUTF16(
                  IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE)));
  blocking_status_ = GetParam();
  protections_on_ = false;
  OnStatusChanged();
}

TEST_P(CookieControlsBubbleViewController3pcdStatusesTest,
       DisplaysOffToggleWhenCookiesBlockedOnSite) {
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(false));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(
                  &gfx::VectorIcon::name, views::kEyeCrossedRefreshIcon.name)));
  EXPECT_CALL(*mock_content_view(),
              SetCookiesLabel(l10n_util::GetStringUTF16(
                  GetParam() == CookieBlocking3pcdStatus::kAll
                      ? IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE
                      : IDS_TRACKING_PROTECTION_BUBBLE_3PC_LIMITED_SUBTITLE)));
  blocking_status_ = GetParam();
  OnStatusChanged();
}

// Verify feedback states
TEST_P(CookieControlsBubbleViewController3pcdStatusesTest,
       FeedbackSectionIsVisibleWhenSiteHasExceptionAndNoEnforcement) {
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(true));
  blocking_status_ = GetParam();
  protections_on_ = false;
  OnStatusChanged();
}

TEST_P(CookieControlsBubbleViewController3pcdStatusesTest,
       FeedbackSectionIsNotVisibleWhenCookiesBlockedOnSite) {
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(false));
  blocking_status_ = GetParam();
  OnStatusChanged();
}

// Verify title and description states
TEST_P(CookieControlsBubbleViewController3pcdStatusesTest,
       DisplaysTitleAndDescriptionWhenCookiesBlockedOnSite) {
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_SITE_NOT_WORKING_DESCRIPTION)));
  blocking_status_ = GetParam();
  OnStatusChanged();
}

TEST_F(CookieControlsBubbleViewControllerTest,
       DisplaysTitleAndDescriptionForTemporaryException3pcLimited) {
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetPluralStringFUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_LIMITING_RESTART_TITLE,
              kDaysToExpiration),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION)));
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  protections_on_ = false;
  OnStatusChanged(kDaysToExpiration);
}

TEST_F(CookieControlsBubbleViewControllerTest,
       DisplaysTitleAndDescriptionForTemporaryExceptionAll3pcBlocked) {
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetPluralStringFUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_TITLE,
              kDaysToExpiration),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION)));
  blocking_status_ = CookieBlocking3pcdStatus::kAll;
  protections_on_ = false;
  OnStatusChanged(kDaysToExpiration);
}

TEST_P(CookieControlsBubbleViewController3pcdStatusesTest,
       DisplaysTitleAndDescriptionWhenSiteHasPermanentException) {
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION)));
  blocking_status_ = GetParam();
  protections_on_ = false;
  OnStatusChanged();
}

INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsBubbleViewController3pcdStatusesTest,
                         testing::Values(CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));

class CookieControlsBubbleViewController3pcdEnforcementTest
    : public CookieControlsBubbleViewControllerTest,
      public testing::WithParamInterface<
          testing::tuple<CookieBlocking3pcdStatus,
                         /*is_permanent_exception*/ bool>> {
 protected:
  void VerifyEnforcementValues(const char* icon_name,
                               int tooltip,
                               bool labels_visible = false) {
    EXPECT_CALL(*mock_content_view(), SetContentLabelsVisible(labels_visible));
    EXPECT_CALL(*mock_content_view(), SetToggleVisible(false));
    EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(false));
    EXPECT_CALL(
        *mock_content_view(),
        SetEnforcedIcon(testing::Field(&gfx::VectorIcon::name, icon_name),
                        l10n_util::GetStringUTF16(tooltip)));
  }
};

TEST_P(CookieControlsBubbleViewController3pcdEnforcementTest,
       DisplaysCookieEnforcement) {
  VerifyEnforcementValues(
      vector_icons::kSettingsChromeRefreshIcon.name,
      IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_MANAGED_BY_SETTINGS_TOOLTIP,
      /*labels_visible=*/true);
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION)));
  blocking_status_ = testing::get<0>(GetParam());
  enforcement_ = CookieControlsEnforcement::kEnforcedByCookieSetting;
  protections_on_ = false;
  OnStatusChanged(testing::get<1>(GetParam()) ? kDaysToExpiration : 0);
}

// Verify enforcement states
TEST_P(CookieControlsBubbleViewController3pcdEnforcementTest,
       DisplaysPolicyEnforcement) {
  VerifyEnforcementValues(vector_icons::kBusinessChromeRefreshIcon.name,
                          IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY);
  blocking_status_ = testing::get<0>(GetParam());
  enforcement_ = CookieControlsEnforcement::kEnforcedByPolicy;
  protections_on_ = false;
  OnStatusChanged(testing::get<1>(GetParam()) ? kDaysToExpiration : 0);
}

TEST_P(CookieControlsBubbleViewController3pcdEnforcementTest,
       DisplaysExtensionEnforcement) {
  VerifyEnforcementValues(vector_icons::kExtensionChromeRefreshIcon.name,
                          IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION);
  blocking_status_ = testing::get<0>(GetParam());
  enforcement_ = CookieControlsEnforcement::kEnforcedByExtension;
  protections_on_ = false;
  OnStatusChanged(testing::get<1>(GetParam()) ? kDaysToExpiration : 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsBubbleViewController3pcdEnforcementTest,
    testing::Combine(testing::Values(CookieBlocking3pcdStatus::kLimited,
                                     CookieBlocking3pcdStatus::kAll),
                     testing::Bool()));

class CookieControlsBubbleViewControllerPre3pcdTest
    : public CookieControlsBubbleViewControllerTest {
 public:
  CookieControlsBubbleViewControllerPre3pcdTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        content_settings::features::kUserBypassUI, {{"expiration", "30d"}});
  }
};

TEST_F(CookieControlsBubbleViewControllerPre3pcdTest,
       ThirdPartyCookiesBlocked) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE)));
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_SITE_NOT_WORKING_DESCRIPTION)));
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(false));
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(false));
  EXPECT_CALL(*mock_content_view(),
              SetCookiesLabel(l10n_util::GetStringUTF16(
                  IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE)));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(
                  &gfx::VectorIcon::name, views::kEyeCrossedRefreshIcon.name)));
  OnStatusChanged();
}

TEST_F(CookieControlsBubbleViewControllerPre3pcdTest,
       ThirdPartyCookiesAllowedPermanent) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE)));
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION)));
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(true));
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(true));
  EXPECT_CALL(*mock_content_view(),
              SetCookiesLabel(l10n_util::GetStringUTF16(
                  IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE)));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(&gfx::VectorIcon::name,
                                           views::kEyeRefreshIcon.name)));
  protections_on_ = false;
  OnStatusChanged();
}

TEST_F(CookieControlsBubbleViewControllerPre3pcdTest,
       ThirdPartyCookiesAllowedTemporary) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE)));
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetPluralStringFUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_TITLE,
              kDaysToExpiration),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION)));
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(true));
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(true));
  EXPECT_CALL(*mock_content_view(),
              SetCookiesLabel(l10n_util::GetStringUTF16(
                  IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE)));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(&gfx::VectorIcon::name,
                                           views::kEyeRefreshIcon.name)));
  protections_on_ = false;
  OnStatusChanged(kDaysToExpiration);
}

class CookieControlsBubbleViewImplTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();

    const GURL url = GURL("http://a.com");
    AddTab(browser(), url);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(browser()->profile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(browser()->profile()),
        /*tracking_protection_settings=*/nullptr);

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
