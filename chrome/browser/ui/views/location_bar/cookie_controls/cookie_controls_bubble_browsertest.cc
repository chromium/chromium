// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_impl.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/vector_icons.h"

const int kDaysToExpiration = 30;

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

class CookieControlsBubbleCoordinatorBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(GetProfile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(GetProfile()),
        TrackingProtectionSettingsFactory::GetForProfile(GetProfile()),
        /*is_incognito_profile=*/false);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("http://a.com")));
  }

  void TearDownOnMainThread() override {
    // Clean up before the browser is destroyed to avoid
    // dangling pointers.
    controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  CookieControlsBubbleCoordinator* coordinator() {
    return CookieControlsBubbleCoordinator::From(browser());
  }

  content_settings::CookieControlsController* controller() {
    return controller_.get();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  std::unique_ptr<content_settings::CookieControlsController> controller_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleCoordinatorBrowserTest,
                       ShowBubbleTest) {
  EXPECT_EQ(coordinator()->GetBubble(), nullptr);
  coordinator()->ShowBubble(BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar_button_provider(),
                            web_contents(), controller());
  EXPECT_NE(coordinator()->GetBubble(), nullptr);

  views::test::WidgetDestroyedWaiter waiter(
      coordinator()->GetBubble()->GetWidget());
  coordinator()->GetBubble()->GetWidget()->Close();
  waiter.Wait();
  EXPECT_EQ(coordinator()->GetBubble(), nullptr);
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleCoordinatorBrowserTest,
                       ActionItemUpdatedWithBubbleVisibility) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(kActionShowCookieControls);
  ASSERT_NE(action, nullptr);

  EXPECT_EQ(coordinator()->GetBubble(), nullptr);
  coordinator()->ShowBubble(BrowserView::GetBrowserViewForBrowser(browser())
                                ->toolbar_button_provider(),
                            web_contents(), controller());
  EXPECT_NE(coordinator()->GetBubble(), nullptr);
  EXPECT_TRUE(action->GetIsShowingBubble());

  views::test::WidgetDestroyedWaiter waiter(
      coordinator()->GetBubble()->GetWidget());
  coordinator()->GetBubble()->GetWidget()->Close();
  waiter.Wait();
  EXPECT_EQ(coordinator()->GetBubble(), nullptr);
  EXPECT_FALSE(action->GetIsShowingBubble());
}

class CookieControlsBubbleViewControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    const GURL url = GURL("http://a.com");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    mock_bubble_view_ =
        std::make_unique<testing::NiceMock<MockCookieControlsBubbleView>>();
    mock_content_view_ =
        std::make_unique<testing::NiceMock<MockCookieControlsContentView>>();

    empty_reloading_view_ = std::make_unique<views::View>();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(GetProfile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(GetProfile()),
        TrackingProtectionSettingsFactory::GetForProfile(GetProfile()),
        /*is_incognito_profile=*/false);

    ON_CALL(*mock_bubble_view(), GetContentView())
        .WillByDefault(testing::Return(mock_content_view()));
    ON_CALL(*mock_bubble_view(), GetReloadingView())
        .WillByDefault(testing::Return(empty_reloading_view()));

    EXPECT_CALL(*mock_bubble_view(),
                UpdateSubtitle(base::ASCIIToUTF16(url.GetHost())));
    view_controller_ = std::make_unique<CookieControlsBubbleViewController>(
        mock_bubble_view(), controller_.get(), web_contents);
  }

  void TearDownOnMainThread() override {
    // Clean up the pointers in the correct order before the browser is
    // destroyed to avoid dangling pointers.
    view_controller_ = nullptr;
    mock_bubble_view_ = nullptr;
    mock_content_view_ = nullptr;
    empty_reloading_view_.reset();
    controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
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

  void OnStatusChanged(int days_to_expiration = 0) {
    auto expiration = days_to_expiration
                          ? base::Time::Now() + base::Days(days_to_expiration)
                          : base::Time();
    view_controller()->OnStatusChanged(controls_state_, enforcement_,
                                       blocking_status_, expiration);
  }

 protected:
  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsBubbleViewControllerBrowserTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content_settings::CookieControlsController> controller_;
  std::unique_ptr<MockCookieControlsContentView> mock_content_view_;
  std::unique_ptr<MockCookieControlsBubbleView> mock_bubble_view_;
  std::unique_ptr<views::View> empty_reloading_view_;
  std::unique_ptr<CookieControlsBubbleViewController> view_controller_;
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  CookieControlsState controls_state_ = CookieControlsState::kBlocked3pc;
  CookieBlocking3pcdStatus blocking_status_ =
      CookieBlocking3pcdStatus::kNotIn3pcd;
};

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerBrowserTest,
                       WidgetClosesWhenControlsAreNotVisible) {
  EXPECT_CALL(*mock_bubble_view(), CloseWidget());
  controls_state_ = CookieControlsState::kHidden;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerBrowserTest,
                       WidgetClosesOnTpcdEnforcement) {
  EXPECT_CALL(*mock_bubble_view(), CloseWidget());
  enforcement_ = CookieControlsEnforcement::kEnforcedByTpcdGrant;
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerBrowserTest,
                       DisplaysThirdPartyCookiesBlockedTitle) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_BLOCKED_TITLE)));
  blocking_status_ = CookieBlocking3pcdStatus::kAll;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerBrowserTest,
                       DisplaysThirdPartyCookiesLimitedTitle) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_LIMITED_TITLE)));
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerBrowserTest,
                       DisplaysThirdPartyCookiesAllowedTitle) {
  EXPECT_CALL(*mock_bubble_view(),
              UpdateTitle(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_COOKIES_ALLOWED_TITLE)));
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged(kDaysToExpiration);
}

class CookieControlsBubbleViewController3pcdStatusesBrowserTest
    : public CookieControlsBubbleViewControllerBrowserTest,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {};

// Verify toggle states
IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
    DisplaysAllowedToggleForSiteException) {
  EXPECT_CALL(*mock_content_view(), SetToggleIsOn(true));
  EXPECT_CALL(*mock_content_view(),
              SetToggleIcon(testing::Field(&gfx::VectorIcon::name,
                                           views::kEyeRefreshIcon.name)));
  EXPECT_CALL(*mock_content_view(),
              SetCookiesLabel(l10n_util::GetStringUTF16(
                  IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE)));
  blocking_status_ = GetParam();
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
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
IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
    FeedbackSectionIsVisibleWhenSiteHasExceptionAndNoEnforcement) {
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(true));
  blocking_status_ = GetParam();
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
    FeedbackSectionIsNotVisibleWhenCookiesBlockedOnSite) {
  EXPECT_CALL(*mock_content_view(), SetFeedbackSectionVisibility(false));
  blocking_status_ = GetParam();
  OnStatusChanged();
}

// Verify title and description states
IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
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

IN_PROC_BROWSER_TEST_F(
    CookieControlsBubbleViewControllerBrowserTest,
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
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged(kDaysToExpiration);
}

IN_PROC_BROWSER_TEST_F(
    CookieControlsBubbleViewControllerBrowserTest,
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
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged(kDaysToExpiration);
}

IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
    DisplaysTitleAndDescriptionWhenSiteHasPermanentException) {
  EXPECT_CALL(
      *mock_content_view(),
      UpdateContentLabels(
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_DESCRIPTION)));
  blocking_status_ = GetParam();
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsBubbleViewController3pcdStatusesBrowserTest,
    testing::Values(CookieBlocking3pcdStatus::kLimited,
                    CookieBlocking3pcdStatus::kAll));

class CookieControlsBubbleViewController3pcdEnforcementBrowserTest
    : public CookieControlsBubbleViewControllerBrowserTest,
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

IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdEnforcementBrowserTest,
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
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged(testing::get<1>(GetParam()) ? kDaysToExpiration : 0);
}

// Verify enforcement states
IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdEnforcementBrowserTest,
    DisplaysPolicyEnforcement) {
  VerifyEnforcementValues(vector_icons::kBusinessChromeRefreshIcon.name,
                          IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY);
  blocking_status_ = testing::get<0>(GetParam());
  enforcement_ = CookieControlsEnforcement::kEnforcedByPolicy;
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged(testing::get<1>(GetParam()) ? kDaysToExpiration : 0);
}

IN_PROC_BROWSER_TEST_P(
    CookieControlsBubbleViewController3pcdEnforcementBrowserTest,
    DisplaysExtensionEnforcement) {
  VerifyEnforcementValues(vector_icons::kExtensionChromeRefreshIcon.name,
                          IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION);
  blocking_status_ = testing::get<0>(GetParam());
  enforcement_ = CookieControlsEnforcement::kEnforcedByExtension;
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged(testing::get<1>(GetParam()) ? kDaysToExpiration : 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsBubbleViewController3pcdEnforcementBrowserTest,
    testing::Combine(testing::Values(CookieBlocking3pcdStatus::kLimited,
                                     CookieBlocking3pcdStatus::kAll),
                     testing::Bool()));

class CookieControlsBubbleViewControllerPre3pcdBrowserTest
    : public CookieControlsBubbleViewControllerBrowserTest {
 public:
  CookieControlsBubbleViewControllerPre3pcdBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        content_settings::features::kUserBypassUI, {{"expiration", "30d"}});
  }
};

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerPre3pcdBrowserTest,
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

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerPre3pcdBrowserTest,
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
  controls_state_ = CookieControlsState::kAllowed3pc;
  OnStatusChanged();
}

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewControllerPre3pcdBrowserTest,
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
  controls_state_ = CookieControlsState::kAllowed3pc;

  OnStatusChanged(kDaysToExpiration);
}

class CookieControlsBubbleViewImplBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    const GURL url = GURL("http://a.com");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    controller_ = std::make_unique<content_settings::CookieControlsController>(
        CookieSettingsFactory::GetForProfile(GetProfile()), nullptr,
        HostContentSettingsMapFactory::GetForProfile(GetProfile()),
        TrackingProtectionSettingsFactory::GetForProfile(GetProfile()),
        /*is_incognito_profile=*/false);

    coordinator()->ShowBubble(browser_view()->toolbar_button_provider(),
                              web_contents, controller_.get());
  }

  CookieControlsBubbleCoordinator* coordinator() {
    return CookieControlsBubbleCoordinator::From(browser());
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  void TearDownOnMainThread() override {
    // Ensure things are destroyed in an appropriate order to ensure pointers
    // are not considered dangling.
    views::test::WidgetDestroyedWaiter waiter(bubble_view()->GetWidget());
    bubble_view()->GetWidget()->Close();
    waiter.Wait();
    EXPECT_EQ(coordinator()->GetBubble(), nullptr);

    controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  CookieControlsBubbleViewImpl* bubble_view() {
    return coordinator()->GetBubble();
  }

 private:
  std::unique_ptr<content_settings::CookieControlsController> controller_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsBubbleViewImplBrowserTest, BubbleWidth) {
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
