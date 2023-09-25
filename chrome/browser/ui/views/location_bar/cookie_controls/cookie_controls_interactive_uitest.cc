// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "base/feature_list_buildflags.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/vector_icons.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondWebContentsElementId);

const char kUMABubbleAllowThirdPartyCookies[] =
    "CookieControls.Bubble.AllowThirdPartyCookies";
const char kUMABubbleBlockThirdPartyCookies[] =
    "CookieControls.Bubble.BlockThirdPartyCookies";
const char kUMABubbleSendFeedback[] = "CookieControls.Bubble.SendFeedback";
}  // namespace

class CookieControlsInteractiveUiTest : public InteractiveBrowserTest {
 public:
  CookieControlsInteractiveUiTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~CookieControlsInteractiveUiTest() override = default;

  void SetUp() override {
    iph_feature_list_.InitAndEnableFeatures(EnabledFeatures(),
                                            DisabledFeatures());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  virtual std::vector<base::test::FeatureRef> EnabledFeatures() {
    return {content_settings::features::kUserBypassUI};
  }

  virtual std::vector<base::test::FeatureRef> DisabledFeatures() { return {}; }

  auto CheckIcon(ElementSpecifier view,
                 const gfx::VectorIcon& icon_pre_2023_refresh,
                 const gfx::VectorIcon& icon_post_2023_refresh) {
    std::string expected_name = features::IsChromeRefresh2023()
                                    ? icon_post_2023_refresh.name
                                    : icon_pre_2023_refresh.name;
    StepBuilder builder;
    builder.SetDescription("CheckIcon()");
    ui::test::internal::SpecifyElement(builder, view);
    builder.SetStartCallback(base::BindOnce(
        [](std::string expected_name, ui::InteractionSequence* sequence,
           ui::TrackedElement* element) {
          auto* vector_icon = AsView<views::ImageView>(element)
                                  ->GetImageModel()
                                  .GetVectorIcon()
                                  .vector_icon();
          if (vector_icon->name != expected_name) {
            sequence->FailForTesting();
          }
        },
        expected_name));
    return builder;
  }

  auto CheckStateForTemporaryException() {
    return Steps(
        CheckViewProperty(CookieControlsContentView::kTitle,
                          &views::Label::GetText,
                          l10n_util::GetPluralStringFUTF16(
                              IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_TITLE,
                              ExceptionDurationInDays())),
        CheckViewProperty(
            CookieControlsContentView::kDescription, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_BLOCKING_RESTART_DESCRIPTION_TODAY)),
        CheckViewProperty(CookieControlsContentView::kToggleButton,
                          &views::ToggleButton::GetIsOn, true),
        CheckIcon(RichControlsContainerView::kIcon, views::kEyeIcon,
                  views::kEyeRefreshIcon));
  }

  auto CheckStateForNoException() {
    return Steps(
        CheckViewProperty(CookieControlsContentView::kToggleButton,
                          &views::ToggleButton::GetIsOn, false),
        CheckViewProperty(
            CookieControlsContentView::kTitle, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE)),
        CheckViewProperty(
            CookieControlsContentView::kDescription, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                ExceptionDurationInDays() == 0
                    ? IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_PERMANENT
                    : IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY)),
        CheckIcon(RichControlsContainerView::kIcon, views::kEyeCrossedIcon,
                  views::kEyeCrossedRefreshIcon));
  }

  auto CheckFeedbackButtonVisible(bool visible) {
    if (visible) {
      return Steps(EnsurePresent(CookieControlsContentView::kFeedbackButton));
    } else {
      return Steps(
          EnsureNotPresent(CookieControlsContentView::kFeedbackButton));
    }
  }

  int ExceptionDurationInDays() {
    return content_settings::features::kUserBypassUIExceptionExpiration.Get()
        .InDays();
  }

  void BlockThirdPartyCookies() {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  void BlockCookiesAndSetHighConfidenceForSite() {
    BlockThirdPartyCookies();
    // Force high site engagement.
    auto* site_engagement =
        site_engagement::SiteEngagementService::Get(browser()->profile());
    site_engagement->ResetBaseScoreForURL(third_party_cookie_page_url(),
                                          /*score=*/100);
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  }
  GURL third_party_cookie_page_url() {
    return https_server()->GetURL("a.test",
                                  "/third_party_partitioned_cookies.html");
  }

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2023 11:00:00 UTC", &time));
    return time;
  }

  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsInteractiveUiTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};

  base::UserActionTester user_actions_;
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

class CookieControlsInteractiveUiNoFeedbackTest
    : public CookieControlsInteractiveUiTest {
 public:
  CookieControlsInteractiveUiNoFeedbackTest() = default;
  ~CookieControlsInteractiveUiNoFeedbackTest() override = default;

 protected:
  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    return {content_settings::features::kUserBypassFeedback};
  }
};

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, BubbleOpens) {
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      CheckFeedbackButtonVisible(false));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, CreateException) {
  // Open the bubble while 3PC are blocked, re-enable them for the site, and
  // confirm the appropriate exception is created.
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckStateForNoException(),
      CheckViewProperty(CookieControlsContentView::kToggleButton,
                        &views::ToggleButton::GetIsOn, false),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckFeedbackButtonVisible(true), CheckStateForTemporaryException());
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiNoFeedbackTest,
                       CreateExceptionFeedbackDisabled) {
  // Open the bubble while 3PC are blocked, re-enable them for the site, and
  // confirm the appropriate exception is created.
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckStateForNoException(),
      CheckViewProperty(CookieControlsContentView::kToggleButton,
                        &views::ToggleButton::GetIsOn, false),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckFeedbackButtonVisible(false), CheckStateForTemporaryException());
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, RemoveException) {
  // Open the bubble while 3PC are blocked, but the page already has an
  // exception. Disable 3PC for the page, and confirm the exception is removed.
  BlockCookiesAndSetHighConfidenceForSite();
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckStateForTemporaryException(),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckFeedbackButtonVisible(false),
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, false),
      CheckStateForNoException());
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest,
                       NavigateHighConfidence) {
  BlockCookiesAndSetHighConfidenceForSite();
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, true));
}

// Need a separate fixture to override the enabled feature list.
class CookieControlsInteractiveUiWithCookieControlsIphTest
    : public CookieControlsInteractiveUiTest {
 public:
  CookieControlsInteractiveUiWithCookieControlsIphTest() = default;
  ~CookieControlsInteractiveUiWithCookieControlsIphTest() override = default;

 protected:
  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {content_settings::features::kUserBypassUI,
            feature_engagement::kIPHCookieControlsFeature};
  }
};

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiWithCookieControlsIphTest,
                       NavigateHighConfidenceDismissIph) {
  BlockCookiesAndSetHighConfidenceForSite();
  RunTestSequenceInContext(
      context(), ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that label doesn't animate.
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, false),
      // Check that IPH shows, then dismiss it.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      PressButton(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      // IPH should hide and cookie controls bubble should not open.
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsureNotPresent(CookieControlsBubbleView::kCookieControlsBubble));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiWithCookieControlsIphTest,
                       NavigateHighConfidenceOpenCookieControlsViaIph) {
  BlockCookiesAndSetHighConfidenceForSite();
  RunTestSequenceInContext(
      context(), ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that IPH shows, then open cookie controls bubble via IPH button.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
      // Cookie controls bubble should show and IPH should close.
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiWithCookieControlsIphTest,
                       NavigateHighConfidenceOpenCookieControlsViaIcon) {
  BlockCookiesAndSetHighConfidenceForSite();
  RunTestSequenceInContext(
      context(), ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that IPH shows, then open cookie controls bubble via icon.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      PressButton(kCookieControlsIconElementId),
      // Cookie controls bubble should show and IPH should close.
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class CookieControlsInteractiveUiWith3pcdUserBypassIphTest
    : public CookieControlsInteractiveUiTest {
 public:
  CookieControlsInteractiveUiWith3pcdUserBypassIphTest() = default;
  ~CookieControlsInteractiveUiWith3pcdUserBypassIphTest() override = default;

 protected:
  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {content_settings::features::kUserBypassUI,
            feature_engagement::kIPH3pcdUserBypassFeature,
            content_settings::features::kTrackingProtection3pcd};
  }
};

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiWith3pcdUserBypassIphTest,
                       ShowAndHide3pcdUbIph) {
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiWith3pcdUserBypassIphTest,
                       Show3pcdUbIphAndOpenCookieControlsViaIcon) {
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), ObserveState(kFeatureEngagementInitializedState, browser()),
      WaitForState(kFeatureEngagementInitializedState, true),
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that IPH shows, then open cookie controls bubble via icon.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      PressButton(kCookieControlsIconElementId),
      // Cookie controls bubble should show and IPH should close.
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

// Opening the feedback dialog on CrOS & LaCrOS open a system level dialog,
// which cannot be easily tested here. Instead, LaCrOS has a separate feedback
// browser test which gives some coverage.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, FeedbackOpens) {
  BlockThirdPartyCookies();
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      PressButton(CookieControlsContentView::kFeedbackButton),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleSendFeedback), 1);
}
#endif

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, ReloadView) {
  // Test that opening the bubble, then closing it after making a change,
  // results in the reload view being displayed.
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),
      PressButton(CookieControlsContentView::kToggleButton),
      PressButton(kLocationIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kReloadingView)),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleAllowThirdPartyCookies), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleBlockThirdPartyCookies), 0);
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest,
                       ReloadView_TabChanged_NoReload) {
  // Test that opening the bubble making a change, then changing tabs while
  // the bubble is open, then re-opening the bubble on the new tab and closing
  // _doesn't_ reload the page. Regression test for crbug.com/1470275.
  BlockThirdPartyCookies();
  const GURL third_party_cookie_page_url_one = third_party_cookie_page_url();
  const GURL third_party_cookie_page_url_two =
      https_server()->GetURL("b.test", "/third_party_partitioned_cookies.html");

  RunTestSequenceInContext(
      // Setup 2 tabs, second tab becomes active.
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId,
                          third_party_cookie_page_url_one),
      AddInstrumentedTab(kSecondWebContentsElementId,
                         third_party_cookie_page_url_two),

      // Open the bubble on the second tab.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),

      // Allow cookies for second tab
      PressButton(CookieControlsContentView::kToggleButton),

      // Select the first tab. Bubble should be hidden by tab swap.
      SelectTab(kTabStripElementId, 0),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble),
      FlushEvents(),

      // Re-open then cookie bubble on the first tab.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),

      // Close the bubble without making a change, the reload view should not
      // be shown.
      PressButton(kLocationIconElementId),
      EnsureNotPresent(CookieControlsBubbleView::kReloadingView),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest,
                       ReloadView_TabChanged_Reload) {
  // Test that opening the bubble, _not_ making a change, then changing tabs
  // while the bubble is open, then re-opening the bubble on the new tab and
  // making a change _does_ reload the page, and that on page reload the
  // reload view should be closed.
  // Regression test for crbug.com/1470275.
  BlockThirdPartyCookies();
  const GURL third_party_cookie_page_url_one = third_party_cookie_page_url();
  const GURL third_party_cookie_page_url_two =
      https_server()->GetURL("b.test", "/third_party_partitioned_cookies.html");

  RunTestSequenceInContext(
      // Setup 2 tabs, focus moves to the second tab.
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId,
                          third_party_cookie_page_url_one),
      AddInstrumentedTab(kSecondWebContentsElementId,
                         third_party_cookie_page_url_two),

      // Open the bubble on the second tab. Don't make any changes to the
      // setting.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),

      // Select the first tab. Bubble should be hidden by tab swap.
      SelectTab(kTabStripElementId, 0),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble),
      FlushEvents(),

      // Re-open then cookie bubble on the first tab.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),

      // Change the setting and close the bubble. The reloading view should
      // be shown, and the view should close automatically.
      PressButton(CookieControlsContentView::kToggleButton),

      PressButton(kLocationIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kReloadingView)),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest,
                       ReloadView_TabChangedDifferentSetting_NoReload) {
  // Test that loading a page with cookies allowed, then swapping to a tab
  // where cookies are disabled, then opening and closing the bubble without
  // making a change _does not_ reload the page.
  // Regression test for crbug.com/1470275.
  BlockThirdPartyCookies();
  const GURL third_party_cookie_page_url_one = third_party_cookie_page_url();
  const GURL third_party_cookie_page_url_two =
      https_server()->GetURL("b.test", "/third_party_partitioned_cookies.html");
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url_two);

  RunTestSequenceInContext(
      // Setup 2 tabs, focus moves to the second tab.
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId,
                          third_party_cookie_page_url_one),
      AddInstrumentedTab(kSecondWebContentsElementId,
                         third_party_cookie_page_url_two),

      // Open the bubble on the second tab, where cookies are allowed.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),

      // Select the first tab. Bubble should be hidden by tab swap.
      SelectTab(kTabStripElementId, 0),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble),
      FlushEvents(),

      // Re-open then cookie bubble on the first tab, where cookies are
      // disallowed.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),

      // Close the bubble without making a change, the reload view should not
      // be shown.
      PressButton(kLocationIconElementId),
      EnsureNotPresent(CookieControlsBubbleView::kReloadingView),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, NoReloadView) {
  // Test that opening the bubble, then closing it without making an effective
  // change to cookie settings, does not show the reload view.
  BlockThirdPartyCookies();
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),
      PressButton(CookieControlsContentView::kToggleButton),
      PressButton(CookieControlsContentView::kToggleButton),
      PressButton(kLocationIconElementId),
      EnsureNotPresent(CookieControlsBubbleView::kReloadingView),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleAllowThirdPartyCookies), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleBlockThirdPartyCookies), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleSendFeedback), 0);
}
