// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list_buildflags.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/vector_icons.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
}

class CookieControlsInteractiveUiTest : public InteractiveBrowserTest {
 public:
  CookieControlsInteractiveUiTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~CookieControlsInteractiveUiTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kUserBypassUI);
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
  // TODO(crbug.com/1446230): This was useful during development, but should be
  // moved to a browsertest, instead of a uitest, where it's a bit more
  // appropriate.
  auto CheckException(const GURL& first_party_url, bool should_exist) {
    return Steps(Do([=]() {
      content_settings::SettingInfo info;
      EXPECT_EQ(
          host_content_settings_map()->GetContentSetting(
              GURL(), first_party_url, ContentSettingsType::COOKIES, &info),
          CONTENT_SETTING_ALLOW);
      EXPECT_TRUE(info.primary_pattern.MatchesAllHosts());
      // If an exception exists, it will have a targeted secondary pattern.
      // If it does not exist, it will fall through the default wildcard.
      if (should_exist) {
        EXPECT_EQ(info.secondary_pattern,
                  ContentSettingsPattern::FromURL(first_party_url));

      } else {
        EXPECT_TRUE(info.secondary_pattern.MatchesAllHosts());
      }
    }));
  }

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
        CheckException(third_party_cookie_page_url(), /*should_exist=*/true),
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
        CheckException(third_party_cookie_page_url(), /*should_exist=*/false),
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

  int ExceptionDurationInDays() {
    return content_settings::features::kUserBypassUIExceptionExpiration.Get()
        .InDays();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }
  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }
  GURL third_party_cookie_page_url() {
    return https_server()->GetURL("a.test",
                                  "/third_party_partitioned_cookies.html");
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, BubbleOpens) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(CookieControlsIconView::kCookieControlsIcon),
      InAnyContext(
          WaitForShow(CookieControlsBubbleView::kCookieControlsBubble)));
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, CreateException) {
  // Open the bubble while 3PC are blocked, re-enable them for the site, and
  // confirm the appropriate exception is created.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(CookieControlsIconView::kCookieControlsIcon),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckStateForNoException(),
      CheckViewProperty(CookieControlsContentView::kToggleButton,
                        &views::ToggleButton::GetIsOn, false),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckStateForTemporaryException());
}

IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, RemoveException) {
  // Open the bubble while 3PC are blocked, but the page already has an
  // exception. Disable 3PC for the page, and confirm the exception is removed.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  cookie_settings()->SetCookieSettingForUserBypass(
      third_party_cookie_page_url());

  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      CheckException(third_party_cookie_page_url(), /*should_exist=*/true),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url()),
      PressButton(CookieControlsIconView::kCookieControlsIcon),
      InAnyContext(WaitForShow(CookieControlsContentView::kToggleButton)),
      CheckStateForTemporaryException(),
      PressButton(CookieControlsContentView::kToggleButton),
      CheckStateForNoException());
}

// Opening the feedback dialog on CrOS & LaCrOS open a system level dialog,
// which cannot be easily tested here. Instead, LaCrOS has a separate feedback
// browser test which gives some coverage.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CookieControlsInteractiveUiTest, FeedbackOpens) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  const GURL third_party_cookie_page_url =
      https_server()->GetURL("a.test", "/third_party_partitioned_cookies.html");
  cookie_settings()->SetCookieSettingForUserBypass(third_party_cookie_page_url);
  RunTestSequenceInContext(
      context(), InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url),
      PressButton(CookieControlsIconView::kCookieControlsIcon),
      PressButton(CookieControlsContentView::kFeedbackButton),
      InAnyContext(WaitForShow(FeedbackDialog::kFeedbackDialogForTesting)));
}
#endif
