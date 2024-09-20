// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/related_app_row_view.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCookieAccessedEvent);
const char kFirstPartyAllowedRow[] = "FirstPartyAllowedRow";
const char kThirdPartyBlockedRow[] = "ThirdPartyBlockedRow";
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAppSettingsWebContentsElementId);
const char kRelatedAppRow[] = "RelatedAppRow";
const char kRelatedAppLabel[] = "RelatedAppLabel";
const char kOnlyPartitionedRow[] = "OnlyPartitionedRow";
const char kMixedPartitionedRow[] = "MixedPartitionedRow";
const char kCookiesDialogOpenedActionName[] = "CookiesInUseDialog.Opened";
const char kCookiesDialogRemoveButtonClickedActionName[] =
    "CookiesInUseDialog.RemoveButtonClicked";

class CookieChangeObserver : public content::WebContentsObserver {
 public:
  CookieChangeObserver(content::WebContents* web_contents,
                       int num_expected_calls)
      : content::WebContentsObserver(web_contents),
        num_expected_calls_(num_expected_calls) {}
  ~CookieChangeObserver() override = default;

 private:
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override {
    OnCookieAccessed();
  }
  void OnCookiesAccessed(content::NavigationHandle* navigation,
                         const content::CookieAccessDetails& details) override {
    OnCookieAccessed();
  }

  void OnCookieAccessed() {
    if (++num_seen_ == num_expected_calls_) {
      auto* const el =
          ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
              kBrowserViewElementId);
      ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
          el, kCookieAccessedEvent);
    }
  }

  int num_seen_ = 0;
  const int num_expected_calls_;
};

}  // namespace

class PageSpecificSiteDataDialogInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  PageSpecificSiteDataDialogInteractiveUiTest() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~PageSpecificSiteDataDialogInteractiveUiTest() override = default;
  PageSpecificSiteDataDialogInteractiveUiTest(
      const PageSpecificSiteDataDialogInteractiveUiTest&) = delete;
  void operator=(const PageSpecificSiteDataDialogInteractiveUiTest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(https_server()->InitializeAndListen());
    SetUpFeatureList();
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
    user_actions_ = std::make_unique<base::UserActionTester>();
    EXPECT_EQ(0, user_actions_->GetActionCount(kCookiesDialogOpenedActionName));
    EXPECT_EQ(0, user_actions_->GetActionCount(
                     kCookiesDialogRemoveButtonClickedActionName));
    SetUpCookieControlMode();
    SetUpPrivacySandboxState();
  }

  void TearDownOnMainThread() override {
    user_actions_.reset();
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  // Returns a callback that queries an expected user action count.
  auto ExpectActionCount(std::string action, int count) {
    return base::BindLambdaForTesting([this, action, count]() {
      EXPECT_EQ(count, user_actions().GetActionCount(action));
    });
  }

  // Returns a common sequence of setup steps for all tests.
  MultiStep NavigateAndOpenDialog(
      ui::ElementIdentifier section_id,
      CookieChangeObserver* cookie_observer = nullptr) {
    const GURL third_party_cookie_page_url =
        https_server()->GetURL("a.test", GetTestPageRelativeURL());
    return Steps(
        InstrumentTab(kWebContentsElementId),
        NavigateWebContents(kWebContentsElementId, third_party_cookie_page_url),
        cookie_observer
            ? Steps(WaitForEvent(kBrowserViewElementId, kCookieAccessedEvent))
            : MultiStep(),
        PressButton(kLocationIconElementId),
        PressButton(PageInfoMainView::kCookieButtonElementId),
        PressButton(PageInfoCookiesContentView::kCookieDialogButton),
        InAnyContext(AfterShow(
            section_id, ExpectActionCount(kCookiesDialogOpenedActionName, 1))));
  }

  // Returns a test step that verifies that the label for `row` matches
  // `string_id`.
  auto CheckRowLabel(ElementSpecifier row, int string_id) {
    return CheckView(row, base::BindOnce([](SiteDataRowView* row) {
                       return row->state_label_for_testing()->GetText();
                     }),
                     l10n_util::GetStringUTF16(string_id));
  }

  // Returns a step that opens the menu for a SiteDataRow.
  auto OpenRowMenu(ElementSpecifier row) {
    return WithElement(
        row, base::BindOnce([](ui::TrackedElement* el) {
          views::test::InteractionTestUtilSimulatorViews::PressButton(
              AsView<SiteDataRowView>(el)->menu_button_for_testing());
        }));
  }

  // Returns a step that clicks the delete button on a SiteDataRow.
  auto DeleteRow(ElementSpecifier row) {
    return WithElement(
        row, base::BindOnce([](ui::TrackedElement* el) {
          views::test::InteractionTestUtilSimulatorViews::PressButton(
              AsView<SiteDataRowView>(el)->delete_button_for_testing());
        }));
  }

  const base::UserActionTester& user_actions() const { return *user_actions_; }
  ui::ElementContext context() const {
    return browser()->window()->GetElementContext();
  }

 protected:
  virtual void SetUpFeatureList() { feature_list_.InitWithFeatures({}, {}); }

  virtual void SetUpCookieControlMode() {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  virtual void SetUpPrivacySandboxState() {}

  virtual std::string GetTestPageRelativeURL() {
    return "/third_party_partitioned_cookies.html";
  }

  std::unique_ptr<base::UserActionTester> user_actions_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Flaky on ChromeOS: crbug.com/1429381
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_FirstPartyAllowed DISABLED_FirstPartyAllowed
#else
#define MAYBE_FirstPartyAllowed FirstPartyAllowed
#endif
IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       MAYBE_FirstPartyAllowed) {
  CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 6);
  RunTestSequenceInContext(
      context(),
      NavigateAndOpenDialog(kPageSpecificSiteDataDialogFirstPartySection,
                            &observer),
      // Name the first row in the first-party section.
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogFirstPartySection,
                                 kFirstPartyAllowedRow, 0u)),
      // Verify no empty state label is present.
      InAnyContext(
          EnsureNotPresent(kPageSpecificSiteDataDialogEmptyStateLabel)),
      // Verify the row label and open the row menu.
      CheckRowLabel(kFirstPartyAllowedRow,
                    IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE),
      OpenRowMenu(kFirstPartyAllowedRow),
      // Verify that the menu has "Block" and "Clear on exit" menu items.
      InAnyContext(WaitForShow(SiteDataRowView::kBlockMenuItem)),
      InAnyContext(WaitForShow(SiteDataRowView::kClearOnExitMenuItem)),
      // Verify that "Allow" is not present as it is already allowed.
      InAnyContext(EnsureNotPresent(SiteDataRowView::kAllowMenuItem)),
      // Verify that the site can be deleted.
      DeleteRow(kFirstPartyAllowedRow),
      // Verify that UI has updated as a result of clicking on a menu item and
      // the correct histogram was logged.
      AfterHide(
          kFirstPartyAllowedRow,
          ExpectActionCount(kCookiesDialogRemoveButtonClickedActionName, 1)),
      // Verify that after deleting the last (and only) row in a section, a
      // label explaining the empty state is shown.
      InAnyContext(CheckViewProperty(
          kPageSpecificSiteDataDialogEmptyStateLabel, &views::Label::GetText,
          l10n_util::GetStringUTF16(
              IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_EMPTY_STATE_LABEL))));
}

// Flaky on ChromeOS: crbug.com/1429381
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ThirdPartyBlocked DISABLED_ThirdPartyBlocked
#else
#define MAYBE_ThirdPartyBlocked ThirdPartyBlocked
#endif
IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       MAYBE_ThirdPartyBlocked) {
  CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 6);
  RunTestSequenceInContext(
      context(),
      NavigateAndOpenDialog(kPageSpecificSiteDataDialogThirdPartySection,
                            &observer),
      // Name the third-party cookies row.
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogThirdPartySection,
                                 kThirdPartyBlockedRow, 2u)),
      CheckRowLabel(kThirdPartyBlockedRow,
                    IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE),
      OpenRowMenu(kThirdPartyBlockedRow),
      // Verify that the menu has "Clear on exit" and "Allow" menu items.
      InAnyContext(WaitForShow(SiteDataRowView::kClearOnExitMenuItem)),
      InAnyContext(WaitForShow(SiteDataRowView::kAllowMenuItem)),
      // Verify that the menu doesn't have the "Block" or "Delete" menu items
      // because it is already blocked.
      InAnyContext(EnsureNotPresent(SiteDataRowView::kBlockMenuItem)),
      InAnyContext(SelectMenuItem(SiteDataRowView::kAllowMenuItem)),
      // Wait until custom event happens (triggered when any menu item
      // callback is called). Menu item is accepted on Mac async, after
      // closure animation finished.
      WaitForEvent(kThirdPartyBlockedRow, kSiteRowMenuItemClicked),
      CheckRowLabel(kThirdPartyBlockedRow,
                    IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE),
      // Verify that after allowing a site, it can be deleted.
      DeleteRow(kThirdPartyBlockedRow),
      // Verify that UI has updated as a result of clicking on the delete
      // button and the correct histogram was logged.
      AfterHide(
          kThirdPartyBlockedRow,
          ExpectActionCount(kCookiesDialogRemoveButtonClickedActionName, 1)));
}

// Flaky on ChromeOS: crbug.com/1429381
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OnlyPartitionedBlockedThirdPartyCookies \
  DISABLED_OnlyPartitionedBlockedThirdPartyCookies
#else
#define MAYBE_OnlyPartitionedBlockedThirdPartyCookies \
  OnlyPartitionedBlockedThirdPartyCookies
#endif
IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       MAYBE_OnlyPartitionedBlockedThirdPartyCookies) {
  CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 6);
  RunTestSequenceInContext(
      context(),
      NavigateAndOpenDialog(kPageSpecificSiteDataDialogThirdPartySection,
                            &observer),
      // Find the third party section and name the row with partitioned only
      // access (b.test).
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogThirdPartySection,
                                 kOnlyPartitionedRow, 0u)),
      CheckRowLabel(
          kOnlyPartitionedRow,
          IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE),
      OpenRowMenu(kOnlyPartitionedRow),
      // Verify that the menu has "Clear on exit", "Allow" and "Block" menu
      // items. Even though the website didn't try to access third party
      // cookies, the allow option is still offered for consistency.
      InAnyContext(WaitForShow(SiteDataRowView::kClearOnExitMenuItem)),
      InAnyContext(WaitForShow(SiteDataRowView::kAllowMenuItem)),
      // Block the site.
      InAnyContext(SelectMenuItem(SiteDataRowView::kBlockMenuItem)),
      // Wait until custom event happens (triggered when any menu item callback
      // is called). Menu item is accepted on Mac async, after closure
      // animation finished. Also check the blocked histogram.
      WaitForEvent(kOnlyPartitionedRow, kSiteRowMenuItemClicked),

      CheckRowLabel(kOnlyPartitionedRow,
                    IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
}
// Flaky on ChromeOS: crbug.com/1429381
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MixedPartitionedBlockedThirdPartyCookies \
  DISABLED_MixedPartitionedBlockedThirdPartyCookies
#else
#define MAYBE_MixedPartitionedBlockedThirdPartyCookies \
  MixedPartitionedBlockedThirdPartyCookies
#endif
IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       MAYBE_MixedPartitionedBlockedThirdPartyCookies) {
  CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 6);
  RunTestSequenceInContext(
      context(),
      NavigateAndOpenDialog(kPageSpecificSiteDataDialogThirdPartySection,
                            &observer),
      // Find the third party section and name the row with mixed storage
      // access (c.test).
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogThirdPartySection,
                                 kMixedPartitionedRow, 1u)),
      CheckRowLabel(
          kMixedPartitionedRow,
          IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE),
      OpenRowMenu(kMixedPartitionedRow),
      // Verify that the menu has "Clear on exit", "Allow" and "Block" menu
      // items.
      InAnyContext(WaitForShow(SiteDataRowView::kClearOnExitMenuItem)),
      InAnyContext(WaitForShow(SiteDataRowView::kBlockMenuItem)),
      // "Allow" menu item is shown because the site has access 3PC and they
      // were blocked. "Allow" menu item is here to allow 3PC access. It also
      // has special string that specifies allowing 3PC.
      InAnyContext(CheckViewProperty(
          SiteDataRowView::kAllowMenuItem, &views::MenuItemView::title,
          l10n_util::GetStringUTF16(
              IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOW_THIRD_PARTY_MENU_ITEM))),
      InAnyContext(SelectMenuItem(SiteDataRowView::kAllowMenuItem)),
      // Wait until custom event happens (triggered when any menu item callback
      // is called). Menu item is accepted on Mac async, after closure animation
      // finished.
      WaitForEvent(kMixedPartitionedRow, kSiteRowMenuItemClicked),
      // Verify that UI has updated as a result of clicking on a menu
      // item and the correct histogram was logged.
      CheckRowLabel(kMixedPartitionedRow,
                    IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));
}

class PageSpecificSiteDataDialogWithRelatedWebAppsInteractiveUiTest
    : public PageSpecificSiteDataDialogInteractiveUiTest {
 public:
  PageSpecificSiteDataDialogWithRelatedWebAppsInteractiveUiTest() = default;
  ~PageSpecificSiteDataDialogWithRelatedWebAppsInteractiveUiTest() override =
      default;

  MultiStep LaunchBrowserForWebAppInTab(const webapps::AppId& app_id,
                                        ui::ElementIdentifier section_id) {
    const auto desc =
        base::StringPrintf("LaunchBrowserForWebAppInTab( %s )", app_id.c_str());

    auto* provider = web_app::WebAppProvider::GetForTest(browser()->profile());
    const GURL target_app_url(
        provider->registrar_unsafe().GetAppLaunchUrl(app_id));

    return Steps(
        std::move(
            StepBuilder()
                .SetDescription(
                    base::StrCat({desc, ": LaunchBrowserForWebAppInTab"}))
                .SetElementID(kWebContentsElementId)
                .SetContext(ui::InteractionSequence::ContextMode::kAny)
                .SetStartCallback(base::BindOnce(
                    [](Profile* profile, webapps::AppId app_id,
                       ui::InteractionSequence* seq, ui::TrackedElement* el) {
                      web_app::LaunchBrowserForWebAppInTab(
                          profile, app_id, WindowOpenDisposition::CURRENT_TAB);
                    },
                    browser()->profile(), app_id))),
        std::move(WaitForWebContentsNavigation(section_id, target_app_url)
                      .FormatDescription(base::StrCat({desc, ": %s"}))));
  }

  MultiStep LaunchBrowserForWebAppInTabAndOpenDialog(
      const webapps::AppId& app_id,
      ui::ElementIdentifier section_id) {
    return Steps(
        InstrumentTab(section_id),
        LaunchBrowserForWebAppInTab(app_id, section_id),
        WaitForEvent(kBrowserViewElementId, kCookieAccessedEvent),
        PressButton(kLocationIconElementId),
        PressButton(PageInfoMainView::kCookieButtonElementId),
        PressButton(PageInfoCookiesContentView::kCookieDialogButton),
        InAnyContext(AfterShow(
            section_id, ExpectActionCount(kCookiesDialogOpenedActionName, 1))));
  }

  const std::string GetDummyAppName() { return "DummyApp"; }

  const GURL GetDummyAppUrl() {
    return https_server()->GetURL("a.test", GetTestPageRelativeURL());
  }

  const GURL GetAppSettingsUrlForApp(std::string app_id) {
#if BUILDFLAG(IS_CHROMEOS)
    return GURL("chrome://os-settings/app-management/detail?id=" + app_id);
#else
    return GURL("chrome://app-settings/" + app_id);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

 protected:
  void SetUpFeatureList() override {
    feature_list_.InitWithFeatures(
        {features::kPageSpecificDataDialogRelatedInstalledAppsSection}, {});
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(
    PageSpecificSiteDataDialogWithRelatedWebAppsInteractiveUiTest,
    RelatedApplicationsSectionInBrowserTab) {
  // Unrelated to the RelatedApplications tests, but needed to avoid crashing.
  CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 6);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Make sure the system web apps are installed since the app management page
  // opens in the OS Settings app, and not a normal browser tab.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Install an app so that the related application section will have something
  // to show. We don't actually care about the app in this test though.
  auto app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), GetDummyAppName(), GetDummyAppUrl());

  RunTestSequenceInContext(
      context(),
      LaunchBrowserForWebAppInTabAndOpenDialog(app_id, kWebContentsElementId),
      // Name the first row in the Related Apps section.
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogRelatedAppsSection,
                                 kRelatedAppRow, 0u)),
      // The label is the 2nd child of the row view.
      InAnyContext(NameChildView(kRelatedAppRow, kRelatedAppLabel, 1u)),
      // Verify the row label.
      CheckViewProperty(kRelatedAppLabel, &views::Label::GetText,
                        base::UTF8ToUTF16(GetDummyAppName())),
      // TODO(crbug.com/362922563): Update this test once the uninstall button
      // is implemented.
      // Verify that row has "Link to app settings".
      InAnyContext(WaitForShow(RelatedAppRowView::kLinkToAppSettings)),
      // Prepare to click the link.
      InstrumentNextTab(kAppSettingsWebContentsElementId, AnyBrowser()),
      // Click the link, and verify it goes to the app's site settings page.
      PressButton(RelatedAppRowView::kLinkToAppSettings),
      WaitForWebContentsNavigation(kAppSettingsWebContentsElementId,
                                   GetAppSettingsUrlForApp(app_id)));
}

IN_PROC_BROWSER_TEST_F(
    PageSpecificSiteDataDialogWithRelatedWebAppsInteractiveUiTest,
    RelatedApplicationsSectionInAppWindow) {
  // Unrelated to the RelatedApplications tests, but needed to avoid crashing.
  CookieChangeObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 6);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Make sure the system web apps are installed since the app management page
  // opens in the OS Settings app, and not a normal browser tab.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Install and launch the web app.
  auto app_id = web_app::test::InstallDummyWebApp(
      browser()->profile(), GetDummyAppName(), GetDummyAppUrl());

  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  // Helper for the test sequence.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAppWindowId);

  RunTestSequenceInContext(
      app_browser->window()->GetElementContext(), InstrumentTab(kAppWindowId),
      // Open the ... menu, web app info, cookies & site data, etc.
      PressButton(kToolbarAppMenuButtonElementId),
      WithView(kToolbarAppMenuButtonElementId,
               base::BindOnce([](AppMenuButton* button) {
                 CHECK(button->IsMenuShowing());
                 button->app_menu()->ExecuteCommand(IDC_WEB_APP_MENU_APP_INFO,
                                                    0);
               })),
      PressButton(PageInfoMainView::kCookieButtonElementId),
      PressButton(PageInfoCookiesContentView::kCookieDialogButton),
      InAnyContext(
          AfterShow(kPageSpecificSiteDataDialogRelatedAppsSection,
                    ExpectActionCount(kCookiesDialogOpenedActionName, 1))),
      // Name the 1st row (ie. the 1st related app) in the related apps section.
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogRelatedAppsSection,
                                 kRelatedAppRow, 0u)),
      // The label is the 2nd child of the row view.
      InAnyContext(NameChildView(kRelatedAppRow, kRelatedAppLabel, 1u)),
      // Verify the row label.
      CheckViewProperty(kRelatedAppLabel, &views::Label::GetText,
                        base::UTF8ToUTF16(GetDummyAppName())),
      // TODO(crbug.com/362922563): Update this test once the uninstall button
      // is implemented.
      // Verify that row has "Link to app settings".
      InAnyContext(WaitForShow(RelatedAppRowView::kLinkToAppSettings)),
      // Prepare to click the link. Must specify `browser()` since the current
      // context is for `app_browser`.
      InstrumentNextTab(kAppSettingsWebContentsElementId, AnyBrowser()),
      // Click the link, and verify it goes to the app's site settings page.
      PressButton(RelatedAppRowView::kLinkToAppSettings),
      WaitForWebContentsNavigation(kAppSettingsWebContentsElementId,
                                   GetAppSettingsUrlForApp(app_id)));
}

class PageSpecificSiteDataDialogIsolatedWebAppInteractiveUiTest
    : public PageSpecificSiteDataDialogInteractiveUiTest {
 public:
  PageSpecificSiteDataDialogIsolatedWebAppInteractiveUiTest() = default;
  ~PageSpecificSiteDataDialogIsolatedWebAppInteractiveUiTest() override =
      default;

 protected:
  void SetUpFeatureList() override {
    feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  Browser* InstallAndLaunchIsolatedWebApp() {
    Profile* profile = browser()->profile();
    auto iwa_dev_server = web_app::CreateAndStartDevServer(
        FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    auto iwa_url_info = web_app::InstallDevModeProxyIsolatedWebApp(
        profile, iwa_dev_server->GetOrigin());
    app_id_ = iwa_url_info.app_id();
    content::RenderFrameHost* iwa_frame =
        web_app::OpenIsolatedWebApp(profile, app_id_);

    CHECK(content::ExecJs(iwa_frame, "localStorage.setItem('key', 'value')"));

    return chrome::FindBrowserWithTab(
        content::WebContents::FromRenderFrameHost(iwa_frame));
  }

  // Installs and launches an IWA, then opens the PageSpecificSiteData dialog.
  MultiStep NavigateAndOpenDialog(Browser* iwa_browser,
                                  ui::ElementIdentifier section_id) {
    return Steps(InstrumentTab(kWebContentsElementId,
                               /*tab_index=*/std::nullopt, iwa_browser),
                 PressButton(kToolbarAppMenuButtonElementId),
                 WithView(kToolbarAppMenuButtonElementId,
                          base::BindOnce([](AppMenuButton* button) {
                            CHECK(button->IsMenuShowing());
                            button->app_menu()->ExecuteCommand(
                                IDC_WEB_APP_MENU_APP_INFO, 0);
                          })),
                 PressButton(PageInfoMainView::kCookieButtonElementId),
                 PressButton(PageInfoCookiesContentView::kCookieDialogButton),
                 InAnyContext(AfterShow(
                     section_id,
                     ExpectActionCount(kCookiesDialogOpenedActionName, 1))));
  }

  // Returns a test step that verifies that the hostname for `row` is equal to
  // `string`.
  auto CheckHostnameLabel(ElementSpecifier row, const std::u16string& string) {
    return CheckView(row, base::BindOnce([](SiteDataRowView* row) {
                       return row->hostname_label_for_testing()->GetText();
                     }),
                     string);
  }

 private:
  webapps::AppId app_id_;
  web_app::OsIntegrationTestOverrideImpl::BlockingRegistration
      override_registration_;
};

// TODO(crbug.com/40776475): This test fails to pass on Mac with real app shims
// working.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AppNameIsDisplayedInsteadOfHostname \
  DISABLED_AppNameIsDisplayedInsteadOfHostname
#else
#define MAYBE_AppNameIsDisplayedInsteadOfHostname \
  AppNameIsDisplayedInsteadOfHostname
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(
    PageSpecificSiteDataDialogIsolatedWebAppInteractiveUiTest,
    MAYBE_AppNameIsDisplayedInsteadOfHostname) {
  Browser* iwa_browser = InstallAndLaunchIsolatedWebApp();
  RunTestSequenceInContext(
      iwa_browser->window()->GetElementContext(),
      NavigateAndOpenDialog(iwa_browser,
                            kPageSpecificSiteDataDialogFirstPartySection),
      // Name the first row in the first-party section.
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogFirstPartySection,
                                 kFirstPartyAllowedRow, 0u)),
      // Verify no empty state label is present.
      InAnyContext(
          EnsureNotPresent(kPageSpecificSiteDataDialogEmptyStateLabel)),
      // Verify the hostname label.
      CheckHostnameLabel(kFirstPartyAllowedRow, u"Simple Isolated App"));
}

class PageSpecificSiteDataDialogPrivacySandboxInteractiveUiTest
    : public PageSpecificSiteDataDialogInteractiveUiTest {
 public:
  PageSpecificSiteDataDialogPrivacySandboxInteractiveUiTest() = default;
  ~PageSpecificSiteDataDialogPrivacySandboxInteractiveUiTest() override =
      default;

 protected:
  void SetUpFeatureList() override {
    feature_list_.InitWithFeatures(
        {blink::features::kSharedStorageAPI, blink::features::kFencedFrames,
         features::kPrivacySandboxAdsAPIsOverride},
        {});
  }

  void SetUpCookieControlMode() override {}

  void SetUpPrivacySandboxState() override {
    PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
        ->SetAllPrivacySandboxAllowedForTesting();
  }

  std::string GetTestPageRelativeURL() override {
    return "/shared_storage_first_party_data.html";
  }
};

IN_PROC_BROWSER_TEST_F(
    PageSpecificSiteDataDialogPrivacySandboxInteractiveUiTest,
    FirstPartyAllowed) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

  RunTestSequenceInContext(
      context(),
      NavigateAndOpenDialog(kPageSpecificSiteDataDialogFirstPartySection),
      // Name the first row in the first-party section.
      InAnyContext(NameChildView(kPageSpecificSiteDataDialogFirstPartySection,
                                 kFirstPartyAllowedRow, 0u)),
      // Verify no empty state label is present.
      InAnyContext(
          EnsureNotPresent(kPageSpecificSiteDataDialogEmptyStateLabel)),
      // Verify the row label and open the row menu.
      CheckRowLabel(kFirstPartyAllowedRow,
                    IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE),
      OpenRowMenu(kFirstPartyAllowedRow),
      // Verify that the menu has "Block" and "Clear on exit" menu items.
      InAnyContext(WaitForShow(SiteDataRowView::kBlockMenuItem)),
      InAnyContext(WaitForShow(SiteDataRowView::kClearOnExitMenuItem)),
      // Verify that "Allow" is not present as it is already allowed.
      InAnyContext(EnsureNotPresent(SiteDataRowView::kAllowMenuItem)),
      // Verify that the site can be deleted.
      DeleteRow(kFirstPartyAllowedRow),
      // Verify that UI has updated as a result of clicking on a menu item and
      // the correct histogram was logged.
      AfterHide(
          kFirstPartyAllowedRow,
          ExpectActionCount(kCookiesDialogRemoveButtonClickedActionName, 1)),
      // Verify that after deleting the last (and only) row in a section, a
      // label explaining the empty state is shown.
      InAnyContext(CheckViewProperty(
          kPageSpecificSiteDataDialogEmptyStateLabel, &views::Label::GetText,
          l10n_util::GetStringUTF16(
              IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_EMPTY_STATE_LABEL))));
}
