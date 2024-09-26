// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/feature_list_buildflags.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
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
const char kUMABubbleReloadingShown[] = "CookieControls.Bubble.ReloadingShown";
const char kUMABubbleReloadingTimeout[] =
    "CookieControls.Bubble.ReloadingTimeout";
}  // namespace

class CookieControlsInteractiveTestBase : public InteractiveFeaturePromoTest {
 public:
  explicit CookieControlsInteractiveTestBase(
      std::vector<base::test::FeatureRef> iph_features = {})
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos(iph_features)) {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~CookieControlsInteractiveTestBase() override = default;

  void SetUp() override {
    disabled_features_.InitWithFeatures(EnabledFeatures(), DisabledFeatures());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());

    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(https_server()->InitializeAndListen());
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveFeaturePromoTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    InteractiveFeaturePromoTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    InteractiveFeaturePromoTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    // This test uses a mock time, so use mock cert verifier to not have cert
    // verification depend on the current mocked time.
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveFeaturePromoTest::TearDownOnMainThread();
  }

 protected:
  virtual std::vector<base::test::FeatureRef> DisabledFeatures() {
    return {content_settings::features::kTrackingProtection3pcd};
  }

  virtual std::vector<base::test::FeatureRef> EnabledFeatures() { return {}; }

  auto CheckIcon(ElementSpecifier view, const gfx::VectorIcon& icon) {
    std::string expected_name = icon.name;
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
        CheckViewProperty(
            CookieControlsContentView::kTitle, &views::Label::GetText,
            l10n_util::GetPluralStringFUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_TITLE,
                ExceptionDurationInDays())),
        CheckViewProperty(
            CookieControlsContentView::kDescription, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION)),
        CheckViewProperty(CookieControlsContentView::kToggleButton,
                          &views::ToggleButton::GetIsOn, true),
        CheckIcon(RichControlsContainerView::kIcon, views::kEyeRefreshIcon));
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
                IDS_TRACKING_PROTECTION_BUBBLE_SITE_NOT_WORKING_DESCRIPTION)),
        CheckIcon(RichControlsContainerView::kIcon,
                  views::kEyeCrossedRefreshIcon));
  }

  auto CheckTrackingProtectionAllowedState() {
    return Steps(
        CheckViewProperty(CookieControlsContentView::kToggleButton,
                          &views::ToggleButton::GetIsOn, !has_act_features_),
        CheckViewProperty(
            CookieControlsContentView::kTitle, &views::Label::GetText,
            l10n_util::GetPluralStringFUTF16(
                browser()->profile()->GetPrefs()->GetBoolean(
                    prefs::kBlockAll3pcToggleEnabled)
                    ? IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_TITLE
                    : IDS_TRACKING_PROTECTION_BUBBLE_LIMITING_RESTART_TITLE,
                ExceptionDurationInDays())),
        CheckViewProperty(
            CookieControlsContentView::kDescription, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_BLOCKING_RESTART_DESCRIPTION)),
        CheckViewProperty(
            has_act_features_
                ? CookieControlsContentView::kThirdPartyCookiesLabel
                : CookieControlsContentView::kToggleLabel,
            &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE)),
        CheckIcon(RichControlsContainerView::kIcon, views::kEyeRefreshIcon));
  }

  auto CheckTrackingProtectionBlockedState() {
    return Steps(
        CheckViewProperty(CookieControlsContentView::kToggleButton,
                          &views::ToggleButton::GetIsOn, has_act_features_),
        CheckViewProperty(
            CookieControlsContentView::kTitle, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_SITE_NOT_WORKING_TITLE)),
        CheckViewProperty(
            CookieControlsContentView::kDescription, &views::Label::GetText,
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_SITE_NOT_WORKING_DESCRIPTION)),
        CheckViewProperty(
            has_act_features_
                ? CookieControlsContentView::kThirdPartyCookiesLabel
                : CookieControlsContentView::kToggleLabel,
            &views::Label::GetText,
            l10n_util::GetStringUTF16(
                browser()->profile()->GetPrefs()->GetBoolean(
                    prefs::kBlockAll3pcToggleEnabled)
                    ? IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE
                    : IDS_TRACKING_PROTECTION_BUBBLE_3PC_LIMITED_SUBTITLE)),
        CheckIcon(RichControlsContainerView::kIcon,
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

  void SetBlockAll3pcToggle(bool enabled) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kBlockAll3pcToggleEnabled, enabled);
  }

  void BlockThirdPartyCookies(bool use_3pcd = false) {
    if (use_3pcd) {
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kTrackingProtection3pcdEnabled, true);
    } else {
      browser()->profile()->GetPrefs()->SetInteger(
          prefs::kCookieControlsMode,
          static_cast<int>(
              content_settings::CookieControlsMode::kBlockThirdParty));
    }
  }

  void SetHighSiteEngagement() {
    // Force high site engagement.
    auto* site_engagement =
        site_engagement::SiteEngagementService::Get(browser()->profile());
    site_engagement->ResetBaseScoreForURL(third_party_cookie_page_url(),
                                          /*score=*/100);
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  }

  // If slow is set to true will return a URL for a page that never finishes
  // loading.
  GURL third_party_cookie_page_url(bool slow = false) {
    if (slow) {
      return https_server()->GetURL(
          "a.test", "/third_party_partitioned_cookies_slow.html");
    } else {
      return https_server()->GetURL("a.test",
                                    "/third_party_partitioned_cookies.html");
    }
  }

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2023 11:00:00 UTC", &time));
    return time;
  }

  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &CookieControlsInteractiveTestBase::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};

  bool has_act_features_ = false;
  base::UserActionTester user_actions_;
  base::test::ScopedFeatureList disabled_features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

class CookieControlsUiTest : public CookieControlsInteractiveTestBase,
                             public testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(,
                         CookieControlsUiTest,
                         testing::Bool(),
                         [](testing::TestParamInfo<bool> param) {
                           return param.param ? "BlockThirdPartyCookies"
                                              : "AllowThirdPartyCookies";
                         });

class CookieControlsInteractiveUiNoFeedbackTest : public CookieControlsUiTest {
 public:
  CookieControlsInteractiveUiNoFeedbackTest() = default;
  ~CookieControlsInteractiveUiNoFeedbackTest() override = default;

 protected:
  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    return {content_settings::features::kUserBypassFeedback,
            content_settings::features::kTrackingProtection3pcd};
  }
};

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, BubbleOpensWhenIconPressed) {
  BlockThirdPartyCookies(GetParam());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      CheckFeedbackButtonVisible(false));
}

IN_PROC_BROWSER_TEST_F(CookieControlsUiTest, CreateExceptionPre3pcd) {
  // Open the bubble while 3PC are blocked, re-enable them for the site, and
  // confirm the appropriate exception is created.
  BlockThirdPartyCookies();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
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
                       CreateExceptionFeedbackDisabledPre3pcd) {
  // Open the bubble while 3PC are blocked, re-enable them for the site, and
  // confirm the appropriate exception is created.
  BlockThirdPartyCookies();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckStateForNoException(),
      CheckViewProperty(CookieControlsContentView::kToggleButton,
                        &views::ToggleButton::GetIsOn, false),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckFeedbackButtonVisible(false), CheckStateForTemporaryException());
}

IN_PROC_BROWSER_TEST_F(CookieControlsUiTest, RemoveExceptionPre3pcd) {
  // Open the bubble while 3PC are blocked, but the page already has an
  // exception. Disable 3PC for the page, and confirm the exception is removed.
  BlockThirdPartyCookies();
  SetHighSiteEngagement();
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
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

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, IconAnimatesOnHighSiteEngagement) {
  BlockThirdPartyCookies(GetParam());
  SetHighSiteEngagement();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, true));
}

// Need a separate fixture to override the enabled feature list.
class CookieControlsWithIphUiTest : public CookieControlsInteractiveTestBase {
 public:
  CookieControlsWithIphUiTest()
      : CookieControlsInteractiveTestBase(
            {feature_engagement::kIPHCookieControlsFeature}) {}
  ~CookieControlsWithIphUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(CookieControlsWithIphUiTest,
                       ShowAndDismissIphOnHighSiteEngagement) {
  BlockThirdPartyCookies();
  SetHighSiteEngagement();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that label doesn't animate.
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, false),
      // Check that IPH shows, then dismiss it.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      ActivateSurface(kCookieControlsIconElementId),
      PressButton(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      // IPH should hide and cookie controls bubble should not open.
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      EnsureNotPresent(CookieControlsBubbleView::kCookieControlsBubble));
}

IN_PROC_BROWSER_TEST_F(CookieControlsWithIphUiTest, OpenUserBypassViaIph) {
  BlockThirdPartyCookies();
  SetHighSiteEngagement();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that IPH shows, then open cookie controls bubble via IPH button.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      ActivateSurface(kCookieControlsIconElementId),
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
      // Cookie controls bubble should show and IPH should close.
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(CookieControlsWithIphUiTest,
                       OpenUserBypassViaIconWhenIphVisible) {
  BlockThirdPartyCookies();
  SetHighSiteEngagement();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that IPH shows, then open cookie controls bubble via icon.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      ActivateSurface(kCookieControlsIconElementId),
      PressButton(kCookieControlsIconElementId),
      // Cookie controls bubble should show and IPH should close.
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(CookieControlsWithIphUiTest, NotShownWhen3pcdEnabled) {
  BlockThirdPartyCookies(/*use_3pcd*/ true);
  SetHighSiteEngagement();
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      // Check that the IPH does not show.
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}
// Opening the feedback dialog on CrOS & LaCrOS open a system level dialog,
// which cannot be easily tested here. Instead, LaCrOS has a separate feedback
// browser test which gives some coverage.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, FeedbackOpens) {
  BlockThirdPartyCookies(GetParam());
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      PressButton(CookieControlsContentView::kFeedbackButton),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleSendFeedback), 1);
}
#endif

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, ReloadView) {
  // Test that opening the bubble, then closing it after making a change,
  // results in the reload view being displayed.
  BlockThirdPartyCookies(GetParam());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),
      PressButton(CookieControlsContentView::kToggleButton),
      PressButton(kLocationIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kReloadingView)),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleAllowThirdPartyCookies), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleBlockThirdPartyCookies), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingTimeout), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingShown), 1);
}

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, ReloadViewTimeout) {
  // Test that opening the bubble, then closing it after making a change,
  // results in the reload view being displayed and then timing out.
  //
  // The page loaded in this test will never finish loading, so the timeout
  // must be configured shorter than the test timeout.
  BlockThirdPartyCookies(GetParam());
  RunTestSequence(
      /*context(),*/ InstrumentTab(kWebContentsElementId),
      EnterText(kOmniboxElementId,
                base::UTF8ToUTF16(
                    "https://" +
                    third_party_cookie_page_url(/*slow=*/true).GetContent())),
      Confirm(kOmniboxElementId),
      InAnyContext(WaitForShow(kCookieControlsIconElementId)),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),
      PressButton(CookieControlsContentView::kToggleButton),
      PressButton(kLocationIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kReloadingView)),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));

  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleAllowThirdPartyCookies), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleBlockThirdPartyCookies), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingTimeout), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingShown), 1);
}

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, ReloadView_TabChanged_NoReload) {
  // Test that opening the bubble making a change, then changing tabs while
  // the bubble is open, then re-opening the bubble on the new tab and closing
  // _doesn't_ reload the page. Regression test for crbug.com/1470275.
  BlockThirdPartyCookies(GetParam());
  const GURL third_party_cookie_page_url_one = third_party_cookie_page_url();
  const GURL third_party_cookie_page_url_two =
      https_server()->GetURL("b.test", "/third_party_partitioned_cookies.html");

  RunTestSequence(
      // Setup 2 tabs, second tab becomes active.
      InstrumentTab(kWebContentsElementId),
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

      // Re-open then cookie bubble on the first tab.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)),

      // Close the bubble without making a change, the reload view should not
      // be shown.
      PressButton(kLocationIconElementId),
      EnsureNotPresent(CookieControlsBubbleView::kReloadingView),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingTimeout), 0);
}

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, ReloadView_TabChanged_Reload) {
  // Test that opening the bubble, _not_ making a change, then changing tabs
  // while the bubble is open, then re-opening the bubble on the new tab and
  // making a change _does_ reload the page, and that on page reload the
  // reload view should be closed.
  // Regression test for crbug.com/1470275.
  BlockThirdPartyCookies(GetParam());
  const GURL third_party_cookie_page_url_one = third_party_cookie_page_url();
  const GURL third_party_cookie_page_url_two =
      https_server()->GetURL("b.test", "/third_party_partitioned_cookies.html");

  RunTestSequence(
      // Setup 2 tabs, focus moves to the second tab.
      InstrumentTab(kWebContentsElementId),
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

      // Re-open then cookie bubble on the first tab.
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),

      // Change the setting and close the bubble. The reloading view should
      // be shown, and the view should close automatically.
      PressButton(CookieControlsContentView::kToggleButton),

      PressButton(kLocationIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kReloadingView)),
      WaitForHide(CookieControlsBubbleView::kCookieControlsBubble));
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingTimeout), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingShown), 1);
}

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest,
                       ReloadView_TabChangedDifferentSetting_NoReload) {
  // Test that loading a page with cookies allowed, then swapping to a tab
  // where cookies are disabled, then opening and closing the bubble without
  // making a change _does not_ reload the page.
  // Regression test for crbug.com/1470275.
  BlockThirdPartyCookies(GetParam());
  const GURL third_party_cookie_page_url_one = third_party_cookie_page_url();
  const GURL third_party_cookie_page_url_two =
      https_server()->GetURL("b.test", "/third_party_partitioned_cookies.html");
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url_two);

  RunTestSequence(
      // Setup 2 tabs, focus moves to the second tab.
      InstrumentTab(kWebContentsElementId),
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
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingTimeout), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingShown), 0);
}

IN_PROC_BROWSER_TEST_P(CookieControlsUiTest, NoReloadView) {
  // Test that opening the bubble, then closing it without making an effective
  // change to cookie settings, does not show the reload view.
  BlockThirdPartyCookies(GetParam());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
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
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleReloadingTimeout), 0);
}

class CookieControlsInteractiveUi3pcdTest
    : public CookieControlsInteractiveTestBase,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 protected:
  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    if (!testing::get<1>(GetParam())) {
      return {content_settings::features::kUserBypassFeedback};
    }
    return {};
  }
};

IN_PROC_BROWSER_TEST_P(CookieControlsInteractiveUi3pcdTest, CreateException) {
  BlockThirdPartyCookies(/*use_3pcd=*/true);
  SetBlockAll3pcToggle(std::get<0>(GetParam()));
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),
      CheckTrackingProtectionBlockedState(),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckFeedbackButtonVisible(testing::get<1>(GetParam())),
      EnsureNotPresent(CookieControlsBubbleView::kReloadingView),
      CheckTrackingProtectionAllowedState());
}

IN_PROC_BROWSER_TEST_P(CookieControlsInteractiveUi3pcdTest, RemoveException) {
  // Open the bubble while 3PC are blocked, but the page already has an
  // exception. Disable 3PC for the page, and confirm the exception is removed.
  BlockThirdPartyCookies(/*use_3pcd=*/true);
  SetHighSiteEngagement();
  SetBlockAll3pcToggle(std::get<0>(GetParam()));
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckTrackingProtectionAllowedState(),
      CheckFeedbackButtonVisible(testing::get<1>(GetParam())),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckFeedbackButtonVisible(false),
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, false),
      CheckTrackingProtectionBlockedState());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsInteractiveUi3pcdTest,
    testing::Combine(/*block_all_third_party_cookies*/ testing::Bool(),
                     /*show_feedback_button*/ testing::Bool()));

class CookieControlsInteractiveUiTrackingProtectionTest
    : public CookieControlsInteractiveTestBase,
      public testing::WithParamInterface<bool> {
 public:
  CookieControlsInteractiveUiTrackingProtectionTest() = default;
  ~CookieControlsInteractiveUiTrackingProtectionTest() override = default;

  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return TrackingProtectionSettingsFactory::GetForProfile(
        browser()->profile());
  }

 protected:
  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {privacy_sandbox::kFingerprintingProtectionUserBypass};
  }

  std::vector<base::test::FeatureRef> DisabledFeatures() override { return {}; }
};

IN_PROC_BROWSER_TEST_P(CookieControlsInteractiveUiTrackingProtectionTest,
                       CreateException) {
  BlockThirdPartyCookies(/*use_3pcd=*/true);
  has_act_features_ = true;
  SetBlockAll3pcToggle(GetParam());
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsBubbleView::kContentView)),
      CheckTrackingProtectionBlockedState(),
      PressButton(CookieControlsContentView::kToggleButton),
      EnsureNotPresent(CookieControlsBubbleView::kReloadingView),
      CheckTrackingProtectionAllowedState());
}

IN_PROC_BROWSER_TEST_P(CookieControlsInteractiveUiTrackingProtectionTest,
                       RemoveException) {
  // Open the bubble while 3PC are blocked, but the page already has an
  // exception. Disable 3PC for the page, and confirm the exception is removed.
  BlockThirdPartyCookies(/*use_3pcd=*/true);
  has_act_features_ = true;
  SetBlockAll3pcToggle(GetParam());
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());
  tracking_protection_settings()->AddTrackingProtectionException(
      third_party_cookie_page_url(), /*is_user_bypass_exception=*/true);
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(kCookieControlsIconElementId),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckTrackingProtectionAllowedState(),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckViewProperty(kCookieControlsIconElementId,
                        &CookieControlsIconView::is_animating_label, false),
      CheckTrackingProtectionBlockedState());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsInteractiveUiTrackingProtectionTest,
                         (/*block_all_third_party_cookies*/ testing::Bool()));
