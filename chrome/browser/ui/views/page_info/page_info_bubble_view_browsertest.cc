// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/page_info/page_info.h"
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
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/events/event_constants.h"

namespace {

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
      PageInfoBubbleView::GetPageInfoBubble();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the UI view identified by
// |view_id|.
views::View* GetView(Browser* browser, int view_id) {
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubble()->GetWidget();
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
  PageInfoBubbleViewBrowserTest() {}

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
    constexpr char kMalware[] = "Malware";
    constexpr char kDeceptive[] = "Deceptive";
    constexpr char kUnwantedSoftware[] = "UnwantedSoftware";
    constexpr char kSignInPasswordReuse[] = "SignInPasswordReuse";
    constexpr char kEnterprisePasswordReuse[] = "EnterprisePasswordReuse";
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
    if (name == kSecure || name == kMixedContentForm || name == kMixedContent ||
        name == kAllowAllPermissions || name == kBlockAllPermissions) {
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
    } else if (name == kMalware) {
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_MALWARE;
    } else if (name == kDeceptive) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_SOCIAL_ENGINEERING;
    } else if (name == kUnwantedSoftware) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_UNWANTED_SOFTWARE;
    } else if (name == kSignInPasswordReuse) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_SIGN_IN_PASSWORD_REUSE;
    } else if (name == kEnterprisePasswordReuse) {
      identity.identity_status =
          PageInfo::SITE_IDENTITY_STATUS_ENTERPRISE_PASSWORD_REUSE;
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
        PageInfoUI::PermissionInfo info;
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
              PageInfoBubbleView::GetPageInfoBubble());
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
      static_cast<PageInfoBubbleView*>(PageInfoBubbleView::GetPageInfoBubble())
          ->SetIdentityInfo(identity);
    }
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

 private:

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

// Times out due to isolation, see crbug.com/733767
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       DISABLED_ChromeDevtoolsURL) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-devtools://devtools/bundled/inspector.html"));
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
// SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       VerifySignInPasswordReusePageInfoBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(safe_browsing::kSyncPasswordPageInfoHistogram, 0);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));

  // Update security state of the current page to match
  // SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(contents, "token",
                            safe_browsing::LoginReputationClientRequest::
                                PasswordReuseEvent::SIGN_IN_PASSWORD);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* whitelist_password_reuse_button = GetView(
      browser(),
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_WHITELIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->visible());
  EXPECT_TRUE(whitelist_password_reuse_button->visible());

  // Verify clicking on button will increment corresponding bucket of
  // PasswordProtection.PageInfoAction.SyncPasswordEntry histogram.
  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSyncPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(whitelist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSyncPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after whitelisting.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);
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
  service->ShowModalWarning(contents, "token",
                            safe_browsing::LoginReputationClientRequest::
                                PasswordReuseEvent::ENTERPRISE_PASSWORD);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* whitelist_password_reuse_button = GetView(
      browser(),
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_WHITELIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->visible());
  EXPECT_TRUE(whitelist_password_reuse_button->visible());

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

  PerformMouseClickOnView(whitelist_password_reuse_button);
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
  // Security state will change after whitelisting.
  helper->GetSecurityInfo(&security_info);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);
}

// Shows the Page Info bubble for a HTTP page (specifically, about:blank).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Insecure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTPS page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, InvokeUi_Secure) {
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
