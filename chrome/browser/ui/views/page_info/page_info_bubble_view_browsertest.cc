// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/page_info.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/content/password_protection/metrics_util.h"
#include "components/safe_browsing/core/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/types/event_type.h"
#include "ui/views/test/widget_test.h"

namespace {

using password_manager::metrics_util::PasswordType;

constexpr char kExpiredCertificateFile[] = "expired_cert.pem";

class ClickEvent : public ui::Event {
 public:
  ClickEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ClickEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the UI view identified by
// |view_id|.
views::View* GetView(Browser* browser, int view_id) {
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  views::View* view = page_info_bubble->GetRootView()->GetViewByID(view_id);
  EXPECT_TRUE(view);
  return view;
}

// Clicks the "Site settings" button from Page Info and waits for a "Settings"
// tab to open.
void ClickAndWaitForSettingsPageToOpen(views::View* site_settings_button) {
  content::WebContentsAddedObserver new_tab_observer;
  PerformMouseClickOnView(site_settings_button);

  base::string16 expected_title(base::ASCIIToUTF16("Settings"));
  content::TitleWatcher title_watcher(new_tab_observer.GetWebContents(),
                                      expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Returns the URL of the new tab that's opened on clicking the "Site settings"
// button from Page Info.
const GURL OpenSiteSettingsForUrl(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURL(browser, url);
  OpenPageInfoBubble(browser);
  // Get site settings button.
  views::View* site_settings_button = GetView(
      browser,
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS);
  ClickAndWaitForSettingsPageToOpen(site_settings_button);

  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL();
}

}  // namespace

class PageInfoBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewBrowserTest() = default;
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    // All the possible test names.
    constexpr char kInsecure[] = "Insecure";
    constexpr char kInternal[] = "Internal";
    constexpr char kInternalExtension[] = "InternalExtension";
    constexpr char kInternalViewSource[] = "InternalViewSource";
    constexpr char kFile[] = "File";
    constexpr char kSecure[] = "Secure";
    constexpr char kEvSecure[] = "EvSecure";
    constexpr char kMalware[] = "Malware";
    constexpr char kDeceptive[] = "Deceptive";
    constexpr char kUnwantedSoftware[] = "UnwantedSoftware";
    constexpr char kSignInSyncPasswordReuse[] = "SignInSyncPasswordReuse";
    constexpr char kSignInNonSyncPasswordReuse[] = "SignInNonSyncPasswordReuse";
    constexpr char kEnterprisePasswordReuse[] = "EnterprisePasswordReuse";
    constexpr char kSavedPasswordReuse[] = "SavedPasswordReuse";
    constexpr char kMalwareAndBadCert[] = "MalwareAndBadCert";
    constexpr char kMixedContentForm[] = "MixedContentForm";
    constexpr char kMixedContent[] = "MixedContent";
    constexpr char kAllowAllPermissions[] = "AllowAllPermissions";
    constexpr char kBlockAllPermissions[] = "BlockAllPermissions";

    const GURL internal_url("chrome://settings");
    const GURL internal_extension_url("chrome-extension://example");
    const GURL file_url("file:///Users/homedirname/folder/file.pdf");
    // Note the following two URLs are not really necessary to get the different
    // versions of Page Info to appear, but are here to indicate the type of
    // URL each IdentityInfo type would normally be associated with.
    const GURL https_url("https://example.com");
    const GURL http_url("http://example.com");

    GURL url = http_url;
    if (name == kSecure || name == kEvSecure || name == kMixedContentForm ||
        name == kMixedContent || name == kAllowAllPermissions ||
        name == kBlockAllPermissions || name == kMalwareAndBadCert) {
      url = https_url;
    }
    if (name == kInternal) {
      url = internal_url;
    } else if (name == kInternalExtension) {
      url = internal_extension_url;
    } else if (name == kInternalViewSource) {
      constexpr char kTestHtml[] = "/viewsource/test.html";
      ASSERT_TRUE(embedded_test_server()->Start());
      url = GURL(content::kViewSourceScheme +
                 std::string(url::kStandardSchemeSeparator) +
                 embedded_test_server()->GetURL(kTestHtml).spec());
    } else if (name == kFile) {
      url = file_url;
    }

    ui_test_utils::NavigateToURL(browser(), url);
    OpenPageInfoBubble(browser());

    PageInfoUI::IdentityInfo identity;
    if (name == kInsecure) {
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_NO_CERT;
    } else if (name == kSecure || name == kAllowAllPermissions ||
               name == kBlockAllPermissions) {
      // Generate a valid mock HTTPS identity, with a certificate.
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
      constexpr char kGoodCertificateFile[] = "ok_cert.pem";
      identity.certificate = net::ImportCertFromFile(
          net::GetTestCertsDirectory(), kGoodCertificateFile);
    } else if (name == kEvSecure) {
      // Generate a valid mock EV HTTPS identity, with an EV certificate. Must
      // match conditions in PageInfoBubbleView::SetIdentityInfo() for setting
      // the certificate button subtitle.
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_EV_CERT;
      identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
      scoped_refptr<net::X509Certificate> ev_cert =
          net::X509Certificate::CreateFromBytes(
              reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));
      ASSERT_TRUE(ev_cert);
      identity.certificate = ev_cert;
    } else if (name == kMalware) {
      identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
    } else if (name == kDeceptive) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SOCIAL_ENGINEERING;
    } else if (name == kUnwantedSoftware) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_UNWANTED_SOFTWARE;
    } else if (name == kSignInSyncPasswordReuse) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
    } else if (name == kSignInNonSyncPasswordReuse) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
    } else if (name == kEnterprisePasswordReuse) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE;
    } else if (name == kSavedPasswordReuse) {
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE;
    } else if (name == kMalwareAndBadCert) {
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_ERROR;
      identity.certificate = net::ImportCertFromFile(
          net::GetTestCertsDirectory(), kExpiredCertificateFile);
      identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
    } else if (name == kMixedContentForm) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      identity.connection_status =
          PageInfo::SITE_CONNECTION_STATUS_INSECURE_FORM_ACTION;
    } else if (name == kMixedContent) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT;
      identity.connection_status =
          PageInfo::SITE_CONNECTION_STATUS_INSECURE_PASSIVE_SUBRESOURCE;
    }

    if (name == kAllowAllPermissions || name == kBlockAllPermissions) {
      // Generate a |PermissionInfoList| with every permission allowed/blocked.
      PermissionInfoList permissions_list;
      for (ContentSettingsType content_type :
           PageInfo::GetAllPermissionsForTesting()) {
        PageInfo::PermissionInfo info;
        info.type = content_type;
        info.setting = (name == kAllowAllPermissions) ? CONTENT_SETTING_ALLOW
                                                      : CONTENT_SETTING_BLOCK;
        info.default_setting =
            content_settings::ContentSettingsRegistry::GetInstance()
                ->Get(info.type)
                ->GetInitialDefaultSetting();
        info.source = content_settings::SettingSource::SETTING_SOURCE_USER;
        info.is_incognito = false;
        permissions_list.push_back(info);
      }

      ChosenObjectInfoList chosen_object_list;

      PageInfoBubbleView* page_info_bubble_view =
          static_cast<PageInfoBubbleView*>(
              PageInfoBubbleView::GetPageInfoBubbleForTesting());
      // Normally |PageInfoBubbleView| doesn't update the permissions already
      // shown if they change while it's still open. For this test, manually
      // force an update by clearing the existing permission views here.
      page_info_bubble_view->GetFocusManager()->SetFocusedView(nullptr);
      page_info_bubble_view->selector_rows_.clear();
      page_info_bubble_view->permissions_view_->RemoveAllChildViews(true);

      page_info_bubble_view->SetPermissionInfo(permissions_list,
                                               std::move(chosen_object_list));
    }

    if (name != kInsecure && name.find(kInternal) == std::string::npos &&
        name != kFile) {
      // The bubble may be PageInfoBubbleView or InternalPageInfoBubbleView. The
      // latter is only used for |kInternal|, so it is safe to static_cast here.
      static_cast<PageInfoBubbleView*>(
          PageInfoBubbleView::GetPageInfoBubbleForTesting())
          ->SetIdentityInfo(identity);
    }
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;
    // Check that each expected View is present in the Page Info bubble.
    views::View* page_info_bubble_view =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetContentsView();
    for (auto id : expected_identifiers_) {
      views::View* view = GetView(browser(), id);
      if (!page_info_bubble_view->Contains(view))
        return false;
    }
    return true;
  }

 protected:
  GURL GetSimplePageUrl() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("simple.html")));
  }

  GURL GetIframePageUrl() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("iframe_blank.html")));
  }

  void ExecuteJavaScriptForTests(const std::string& js) {
    base::RunLoop run_loop;
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->ExecuteJavaScriptForTests(
            base::ASCIIToUTF16(js),
            base::BindOnce([](const base::Closure& quit_callback,
                              base::Value result) { quit_callback.Run(); },
                           run_loop.QuitClosure()));
    run_loop.Run();
  }

  void TriggerReloadPromptOnClose() const {
    PageInfoBubbleView* const page_info_bubble_view =
        static_cast<PageInfoBubbleView*>(
            PageInfoBubbleView::GetPageInfoBubbleForTesting());
    ASSERT_NE(nullptr, page_info_bubble_view);

    // Set some dummy non-default permissions. This will trigger a reload prompt
    // when the bubble is closed.
    PageInfo::PermissionInfo permission;
    permission.type = ContentSettingsType::NOTIFICATIONS;
    permission.setting = ContentSetting::CONTENT_SETTING_BLOCK;
    permission.default_setting = ContentSetting::CONTENT_SETTING_ASK;
    permission.source = content_settings::SettingSource::SETTING_SOURCE_USER;
    permission.is_incognito = false;
    page_info_bubble_view->OnPermissionChanged(permission);
  }

  void SetPageInfoBubbleIdentityInfo(
      const PageInfoUI::IdentityInfo& identity_info) {
    static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting())
        ->SetIdentityInfo(identity_info);
  }

  base::string16 GetCertificateButtonTitle() const {
    // Only PageInfoBubbleViewBrowserTest can access certificate_button_ in
    // PageInfoBubbleView, or title() in HoverButton.
    PageInfoBubbleView* page_info_bubble_view =
        static_cast<PageInfoBubbleView*>(
            PageInfoBubbleView::GetPageInfoBubbleForTesting());
    return page_info_bubble_view->certificate_button_->title()->GetText();
  }

  base::string16 GetCertificateButtonSubtitle() const {
    PageInfoBubbleView* page_info_bubble_view =
        static_cast<PageInfoBubbleView*>(
            PageInfoBubbleView::GetPageInfoBubbleForTesting());
    return page_info_bubble_view->certificate_button_->subtitle()->GetText();
  }

  const base::string16 GetPageInfoBubbleViewDetailText() {
    PageInfoBubbleView* page_info_bubble_view =
        static_cast<PageInfoBubbleView*>(
            PageInfoBubbleView::GetPageInfoBubbleForTesting());
    return page_info_bubble_view->details_text();
  }

 private:
  std::vector<PageInfoBubbleView::PageInfoBubbleViewID> expected_identifiers_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ShowBubble) {
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeURL) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeExtensionURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-extension://extension-id/options.html"));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeDevtoolsURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("devtools://devtools/bundled/inspector.html"));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ViewSourceURL) {
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->ViewSource();
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

// Test opening "Site Details" via Page Info from an ASCII origin does the
// correct URL canonicalization.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, SiteSettingsLink) {
  GURL url = GURL("https://www.google.com/");
  std::string expected_origin = "https%3A%2F%2Fwww.google.com";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url));
}

// Test opening "Site Details" via Page Info from a non-ASCII URL converts it to
// an origin and does punycode conversion as well as URL canonicalization.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithNonAsciiUrl) {
  GURL url = GURL("http://🥄.ws/other/stuff.htm");
  std::string expected_origin = "http%3A%2F%2Fxn--9q9h.ws";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url));
}

// Test opening "Site Details" via Page Info from an origin with a non-default
// (scheme, port) pair will specify port # in the origin passed to query params.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithNonDefaultPort) {
  GURL url = GURL("https://www.example.com:8372");
  std::string expected_origin = "https%3A%2F%2Fwww.example.com%3A8372";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url));
}

// Test opening "Site Details" via Page Info from about:blank goes to "Content
// Settings" (the alternative is a blank origin being sent to "Site Details").
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithAboutBlankURL) {
  EXPECT_EQ(GURL(chrome::kChromeUIContentSettingsURL),
            OpenSiteSettingsForUrl(browser(), GURL(url::kAboutBlankURL)));
}

// Test opening page info bubble that matches
// SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       VerifyEnterprisePasswordReusePageInfoBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  // Update security state of the current page to match
  // SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  safe_browsing::ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  service->set_reused_password_account_type_for_last_shown_warning(
      reused_password_account_type);

  service->ShowModalWarning(
      contents, safe_browsing::RequestOutcome::UNKNOWN,
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", reused_password_account_type);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* allowlist_password_reuse_button = GetView(
      browser(),
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            visible_security_state->malicious_content_status);
  ASSERT_EQ(l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE),
            GetPageInfoBubbleViewDetailText());

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->GetVisible());
  EXPECT_TRUE(allowlist_password_reuse_button->GetVisible());

  // Verify clicking on button will increment corresponding bucket of
  // PasswordProtection.PageInfoAction.NonGaiaEnterprisePasswordEntry histogram.
  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(
          safe_browsing::kEnterprisePasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(allowlist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(
          safe_browsing::kEnterprisePasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after allowlisting.
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            visible_security_state->malicious_content_status);
}

// Test opening page info bubble that matches
// SB_THREAT_TYPE_SAVED_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       VerifySavedPasswordReusePageInfoBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  // Update security state of the current page to match
  // SB_THREAT_TYPE_SAVED_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  safe_browsing::ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::SAVED_PASSWORD);
  service->set_reused_password_account_type_for_last_shown_warning(
      reused_password_account_type);

  service->ShowModalWarning(
      contents, safe_browsing::RequestOutcome::UNKNOWN,
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", reused_password_account_type);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* allowlist_password_reuse_button = GetView(
      browser(),
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE,
            visible_security_state->malicious_content_status);

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->GetVisible());
  EXPECT_TRUE(allowlist_password_reuse_button->GetVisible());

  // Verify clicking on button will increment corresponding bucket of
  // PasswordProtection.PageInfoAction.NonGaiaEnterprisePasswordEntry histogram.
  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSavedPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(allowlist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSavedPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after allowlisting.
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            visible_security_state->malicious_content_status);
}

// Shows the Page Info bubble for a HTTP page (specifically, about:blank).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Insecure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTPS page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Secure) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_EvSecure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an internal page, e.g. chrome://settings.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Internal) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an extensions page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_InternalExtension) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a chrome page that displays the source HTML.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_InternalViewSource) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a file:// URL.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_File) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for malware by Safe Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Malware) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for social engineering by Safe
// Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Deceptive) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for distributing unwanted
// software by Safe Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_UnwantedSoftware) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the user has
// re-used an existing password on a site, e.g. due to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_PasswordReuse) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for malware that also has a bad
// certificate.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_MalwareAndBadCert) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an admin-provided cert when the page is
// secure, but has a form that submits to an insecure url.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_MixedContentForm) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an admin-provided cert when the page is
// secure, but it uses insecure resources (e.g. images).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_MixedContent) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble with all the permissions displayed with 'Allow'
// set. All permissions will show regardless of its factory default value.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_AllowAllPermissions) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble with all the permissions displayed with 'Block'
// set. All permissions will show regardless of its factory default value.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InvokeUi_BlockAllPermissions) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       ClosesOnUserNavigateToSamePage) {
  ui_test_utils::NavigateToURL(browser(), GetSimplePageUrl());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
  ui_test_utils::NavigateToURL(browser(), GetSimplePageUrl());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       ClosesOnUserNavigateToDifferentPage) {
  ui_test_utils::NavigateToURL(browser(), GetSimplePageUrl());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
  ui_test_utils::NavigateToURL(browser(), GetIframePageUrl());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       DoesntCloseOnSubframeNavigate) {
  ui_test_utils::NavigateToURL(browser(), GetIframePageUrl());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
  content::NavigateIframeToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), "test",
      GetSimplePageUrl());
  // Expect that the bubble is still open even after a subframe navigation has
  // happened.
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

class PageInfoBubbleViewBrowserTestWithAutoupgradesDisabled
    : public PageInfoBubbleViewBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageInfoBubbleViewBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Ensure changes to security state are reflected in an open PageInfo bubble.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTestWithAutoupgradesDisabled,
                       UpdatesOnSecurityStateChange) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/delayed_mixed_content.html"));
  OpenPageInfoBubble(browser());

  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();

  EXPECT_EQ(page_info->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY));

  ExecuteJavaScriptForTests("load_mixed();");
  EXPECT_EQ(page_info->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY));
}

// Ensure a page can both have an invalid certificate *and* be blocked by Safe
// Browsing.  Regression test for bug 869925.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, BlockedAndInvalidCert) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  ui_test_utils::NavigateToURL(browser(), https_server.GetURL("/simple.html"));

  // Setup the bogus identity with an expired cert and SB flagging.
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_ERROR;
  identity.certificate = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                                 kExpiredCertificateFile);
  identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
  OpenPageInfoBubble(browser());

  SetPageInfoBubbleIdentityInfo(identity);

  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();

  // Verify bubble complains of malware...
  EXPECT_EQ(page_info->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MALWARE_SUMMARY));

  // ...and has a "Certificate (Invalid)" button.
  const base::string16 invalid_parens = l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CERTIFICATE_INVALID_PARENTHESIZED);
  EXPECT_EQ(GetCertificateButtonTitle(),
            l10n_util::GetStringFUTF16(IDS_PAGE_INFO_CERTIFICATE_BUTTON_TEXT,
                                       invalid_parens));
}

// Ensure a page that has an EV certificate *and* is blocked by Safe Browsing
// shows the correct PageInfo UI. Regression test for crbug.com/1014240.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, MalwareAndEvCert) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  ui_test_utils::NavigateToURL(browser(), https_server.GetURL("/simple.html"));

  // Generate a valid mock EV HTTPS identity, with an EV certificate. Must
  // match conditions in PageInfoBubbleView::SetIdentityInfo() for setting
  // the certificate button subtitle.
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_EV_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(thawte_der), sizeof(thawte_der));
  ASSERT_TRUE(ev_cert);
  identity.certificate = ev_cert;

  // Have the page also trigger an SB malware warning.
  identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;

  OpenPageInfoBubble(browser());
  SetPageInfoBubbleIdentityInfo(identity);

  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();

  // Verify bubble complains of malware...
  EXPECT_EQ(page_info->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MALWARE_SUMMARY));

  // ...and has the correct organization details in the Certificate button.
  EXPECT_EQ(GetCertificateButtonSubtitle(),
            l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
                base::UTF8ToUTF16("Thawte Inc"), base::UTF8ToUTF16("US")));
}

namespace {

// Tracks focus of an arbitrary UI element.
class FocusTracker {
 public:
  bool focused() const { return focused_; }

  // Wait for focused() to be in state |target_state_is_focused|. If focused()
  // is already in the desired state, returns immediately, otherwise waits until
  // it is.
  void WaitForFocus(bool target_state_is_focused) {
    if (focused_ == target_state_is_focused)
      return;
    target_state_is_focused_ = target_state_is_focused;
    run_loop_.Run();
  }

 protected:
  explicit FocusTracker(bool initially_focused) : focused_(initially_focused) {}
  virtual ~FocusTracker() {}

  void OnFocused() {
    focused_ = true;
    if (run_loop_.running() && target_state_is_focused_ == focused_)
      run_loop_.Quit();
  }

  void OnBlurred() {
    focused_ = false;
    if (run_loop_.running() && target_state_is_focused_ == focused_)
      run_loop_.Quit();
  }

 private:
  // Whether the tracked visual element is currently focused.
  bool focused_ = false;

  // Desired state when waiting for focus to change.
  bool target_state_is_focused_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(FocusTracker);
};

// Watches a WebContents for focus changes.
class WebContentsFocusTracker : public FocusTracker,
                                public content::WebContentsObserver {
 public:
  explicit WebContentsFocusTracker(content::WebContents* web_contents)
      : FocusTracker(IsWebContentsFocused(web_contents)),
        WebContentsObserver(web_contents) {}

  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override {
    OnFocused();
  }

  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override {
    OnBlurred();
  }

 private:
  static bool IsWebContentsFocused(content::WebContents* web_contents) {
    Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
    if (!browser)
      return false;
    if (browser->tab_strip_model()->GetActiveWebContents() != web_contents)
      return false;
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->contents_web_view()
        ->HasFocus();
  }
};

// Watches a View for focus changes.
class ViewFocusTracker : public FocusTracker, public views::ViewObserver {
 public:
  explicit ViewFocusTracker(views::View* view)
      : FocusTracker(view->HasFocus()) {
    scoped_observer_.Add(view);
  }

  void OnViewFocused(views::View* observed_view) override { OnFocused(); }

  void OnViewBlurred(views::View* observed_view) override { OnBlurred(); }

 private:
  ScopedObserver<views::View, views::ViewObserver> scoped_observer_{this};
};

}  // namespace

#if defined(OS_MAC)
// https://crbug.com/1029882
#define MAYBE_FocusReturnsToContentOnClose DISABLED_FocusReturnsToContentOnClose
#else
#define MAYBE_FocusReturnsToContentOnClose FocusReturnsToContentOnClose
#endif

// Test that when the PageInfo bubble is closed, focus is returned to the web
// contents pane.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       MAYBE_FocusReturnsToContentOnClose) {
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsFocusTracker web_contents_focus_tracker(web_contents);
  web_contents->Focus();
  web_contents_focus_tracker.WaitForFocus(true);

  OpenPageInfoBubble(browser());
  PageInfoBubbleView* page_info_bubble_view = static_cast<PageInfoBubbleView*>(
      PageInfoBubbleView::GetPageInfoBubbleForTesting());
  EXPECT_FALSE(web_contents_focus_tracker.focused());

  page_info_bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  web_contents_focus_tracker.WaitForFocus(true);
  EXPECT_TRUE(web_contents_focus_tracker.focused());
}

#if defined(OS_MAC)
// https://crbug.com/1029882
#define MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt \
  DISABLED_FocusDoesNotReturnToContentsOnReloadPrompt
#else
#define MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt \
  FocusDoesNotReturnToContentsOnReloadPrompt
#endif

// Test that when the PageInfo bubble is closed and a reload prompt is
// displayed, focus is NOT returned to the web contents pane, but rather returns
// to the location bar so accessibility users must tab through the reload prompt
// before getting back to web contents (see https://crbug.com/910067).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       MAYBE_FocusDoesNotReturnToContentsOnReloadPrompt) {
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsFocusTracker web_contents_focus_tracker(web_contents);
  ViewFocusTracker location_bar_focus_tracker(
      BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView());
  web_contents->Focus();
  web_contents_focus_tracker.WaitForFocus(true);

  OpenPageInfoBubble(browser());
  PageInfoBubbleView* page_info_bubble_view = static_cast<PageInfoBubbleView*>(
      PageInfoBubbleView::GetPageInfoBubbleForTesting());
  EXPECT_FALSE(web_contents_focus_tracker.focused());

  TriggerReloadPromptOnClose();
  page_info_bubble_view->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  location_bar_focus_tracker.WaitForFocus(true);
  web_contents_focus_tracker.WaitForFocus(false);
  EXPECT_TRUE(location_bar_focus_tracker.focused());
  EXPECT_FALSE(web_contents_focus_tracker.focused());
}
