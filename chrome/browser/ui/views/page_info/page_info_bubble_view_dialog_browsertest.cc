// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_info/about_this_site_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

namespace {

using Status = ::content_settings::TrackingProtectionBlockingStatus;
using FeatureType = ::content_settings::TrackingProtectionFeatureType;

constexpr int kTopicsAPITestTaxonomyVersion = 1;

constexpr char kExpiredCertificateFile[] = "expired_cert.pem";
constexpr char kAboutThisSiteUrl[] = "a.test";
constexpr char kHistoryUrl[] = "b.test";

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ui::test::TestEvent event;
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

}  // namespace

class PageInfoBubbleViewDialogBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {},
        {// TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having
         // to disable this feature.
         features::kHttpsUpgrades,
         content_settings::features::kTrackingProtection3pcd,
         privacy_sandbox::kTrackingProtection3pcdUx});
  }

  PageInfoBubbleViewDialogBrowserTest(
      const PageInfoBubbleViewDialogBrowserTest& test) = delete;
  PageInfoBubbleViewDialogBrowserTest& operator=(
      const PageInfoBubbleViewDialogBrowserTest& test) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));

    // All the possible test names.
    constexpr char kInsecure[] = "Insecure";
    constexpr char kInternal[] = "Internal";
    constexpr char kInternalExtension[] = "InternalExtension";
    constexpr char kInternalViewSource[] = "InternalViewSource";
    constexpr char kFile[] = "File";
    constexpr char kSecure[] = "Secure";
    constexpr char kSecureSubpage[] = "SecureSubpage";
    constexpr char kEvSecure[] = "EvSecure";
    constexpr char kEvSecureSubpage[] = "EvSecureSubpage";
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
    constexpr char kTrackingProtection3pcAllowedForSite[] =
        "TrackingProtection3pcAllowedForSite";
    constexpr char kTrackingProtection3pcBlocked[] =
        "TrackingProtection3pcBlocked";
    constexpr char kTrackingProtection3pcLimited[] =
        "TrackingProtection3pcLimited";

    const GURL internal_url("chrome://settings");
    const GURL internal_extension_url("chrome-extension://example");
    const GURL file_url("file:///Users/homedirname/folder/file.pdf");
    // Note the following two URLs are not really necessary to get the different
    // versions of Page Info to appear, but are here to indicate the type of
    // URL each IdentityInfo type would normally be associated with.
    const GURL https_url("https://example.com");
    const GURL http_url("http://example.com");
    const std::string kSiteOrigin = "example.com";

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
      url = GURL(content::kViewSourceScheme + std::string(":") +
                 embedded_test_server()->GetURL(kTestHtml).spec());
    } else if (name == kFile) {
      url = file_url;
    }

    if (name == kTrackingProtection3pcAllowedForSite ||
        name == kTrackingProtection3pcBlocked ||
        name == kTrackingProtection3pcLimited) {
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kTrackingProtection3pcdEnabled, true);
      if (name == kTrackingProtection3pcAllowedForSite) {
        HostContentSettingsMapFactory::GetForProfile(browser()->profile())
            ->SetContentSettingCustomScope(
                ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::FromString(
                    std::string("[*.]example.com")),
                ContentSettingsType::COOKIES,
                ContentSetting::CONTENT_SETTING_ALLOW);
      } else if (name == kTrackingProtection3pcBlocked) {
        browser()->profile()->GetPrefs()->SetBoolean(
            prefs::kBlockAll3pcToggleEnabled, true);
      }
    }

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    OpenPageInfoBubble(browser());

    safe_browsing::ReusedPasswordAccountType reused_password_account_type;
    PageInfoUI::IdentityInfo identity;
    if (name == kInsecure) {
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_NO_CERT;
    } else if (name == kSecure || name == kAllowAllPermissions ||
               name == kBlockAllPermissions || name == kSecureSubpage) {
      // Generate a valid mock HTTPS identity, with a certificate.
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_CERT;
      constexpr char kGoodCertificateFile[] = "ok_cert.pem";
      identity.certificate = net::ImportCertFromFile(
          net::GetTestCertsDirectory(), kGoodCertificateFile);
    } else if (name == kEvSecure || name == kEvSecureSubpage) {
      // Generate a valid mock EV HTTPS identity, with an EV certificate. Must
      // match conditions in PageInfoBubbleView::SetIdentityInfo() for setting
      // the certificate button subtitle.
      identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_EV_CERT;
      identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
      scoped_refptr<net::X509Certificate> ev_cert =
          net::X509Certificate::CreateFromBytes(thawte_der);
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
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::GSUITE);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kSignInNonSyncPasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::GMAIL);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kEnterprisePasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_ENTERPRISE_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
    } else if (name == kSavedPasswordReuse) {
      reused_password_account_type.set_account_type(
          safe_browsing::ReusedPasswordAccountType::SAVED_PASSWORD);
      identity.safe_browsing_status =
          PageInfo::SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE;
      identity.show_change_password_buttons = true;
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
        info.source = content_settings::SettingSource::kUser;
        permissions_list.push_back(info);
      }

      ChosenObjectInfoList chosen_object_list;
      PageInfo* presenter = GetPresenter();
      EXPECT_TRUE(presenter);
      EXPECT_TRUE(presenter->ui_for_testing());
      auto* current_ui = presenter->ui_for_testing();
      views::View* bubble_view =
          PageInfoBubbleView::GetPageInfoBubbleForTesting();
      // Normally |PageInfoBubbleView| doesn't update the permissions already
      // shown if they change while it's still open. For this test, manually
      // force an update by clearing the existing permission views here.
      bubble_view->GetFocusManager()->SetFocusedView(nullptr);

      auto* main_page = static_cast<PageInfoMainView*>(current_ui);
      main_page->toggle_rows_.clear();
      main_page->permissions_view_->RemoveAllChildViews();

      current_ui->SetPermissionInfo(permissions_list,
                                    std::move(chosen_object_list));
    }

    if (name == kSignInSyncPasswordReuse ||
        name == kSignInNonSyncPasswordReuse ||
        name == kEnterprisePasswordReuse || name == kSavedPasswordReuse) {
      safe_browsing::ChromePasswordProtectionService* service =
          safe_browsing::ChromePasswordProtectionService::
              GetPasswordProtectionService(browser()->profile());
      service->set_reused_password_account_type_for_last_shown_warning(
          reused_password_account_type);
      identity.safe_browsing_details = service->GetWarningDetailText(
          service->reused_password_account_type_for_last_shown_warning());
    }

    if (name == kSecureSubpage || name == kEvSecureSubpage) {
      PageInfoBubbleView* bubble_view = static_cast<PageInfoBubbleView*>(
          PageInfoBubbleView::GetPageInfoBubbleForTesting());
      bubble_view->OpenSecurityPage();
    }

    if (name != kInsecure && name.find(kInternal) == std::string::npos &&
        name != kFile) {
      identity.site_identity = kSiteOrigin;
      // The bubble may be PageInfoBubbleView or InternalPageInfoBubbleView. The
      // latter is only used for |kInternal|, so it is safe to static_cast here.
      PageInfo* presenter = GetPresenter();
      EXPECT_TRUE(presenter);
      EXPECT_TRUE(presenter->ui_for_testing());
      presenter->ui_for_testing()->SetIdentityInfo(identity);
    }
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi()) {
      return false;
    }
    // Check that each expected View is present in the Page Info bubble.
    views::View* page_info_bubble_view =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetContentsView();
    for (auto id : expected_identifiers_) {
      views::View* view = GetView(browser(), id);
      if (!page_info_bubble_view->Contains(view)) {
        return false;
      }
    }
    return true;
  }

  PageInfo* GetPresenter() {
    return static_cast<PageInfoBubbleView*>(
               PageInfoBubbleView::GetPageInfoBubbleForTesting())
        ->presenter_for_testing();
  }

 private:
  std::vector<PageInfoViewFactory::PageInfoViewID> expected_identifiers_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_TrackingProtection3pcAllowedForSite) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_TrackingProtection3pcBlocked) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_TrackingProtection3pcLimited) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTP page (specifically, about:blank).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Insecure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTPS page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Secure) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a HTTPS page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SecureSubpage) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_EvSecure) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_EvSecureSubpage) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an internal page, e.g. chrome://settings.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Internal) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an extensions page.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_InternalExtension) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a chrome page that displays the source HTML.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_InternalViewSource) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a file:// URL.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_File) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for malware by Safe Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest, InvokeUi_Malware) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for social engineering by Safe
// Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_Deceptive) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for distributing unwanted
// software by Safe Browsing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_UnwantedSoftware) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for a site flagged for malware that also has a bad
// certificate.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_MalwareAndBadCert) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an admin-provided cert when the page is
// secure, but has a form that submits to an insecure url.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_MixedContentForm) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble for an admin-provided cert when the page is
// secure, but it uses insecure resources (e.g. images).
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_MixedContent) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble with all the permissions displayed with 'Allow'
// set. All permissions will show regardless of its factory default value.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_AllowAllPermissions) {
  // Last updated in crrev.com/c/5784580.
  set_baseline("5784580");
  ShowAndVerifyUi();
}

// Shows the Page Info bubble with all the permissions displayed with 'Block'
// set. All permissions will show regardless of its factory default value.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_BlockAllPermissions) {
  // Last updated in crrev.com/c/5784580.
  set_baseline("5784580");
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the user has
// re-used an existing password on a site, e.g. due to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SavedPasswordReuse) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the
// signed-in syncing user has re-used an existing password on a site, e.g. due
// to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SignInSyncPasswordReuse) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the
// signed-in not syncing user has re-used an existing password on a site, e.g.
// due to phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_SignInNonSyncPasswordReuse) {
  ShowAndVerifyUi();
}

// Shows the Page Info bubble Safe Browsing warning after detecting the
// enterprise user has re-used an existing password on a site, e.g. due to
// phishing.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewDialogBrowserTest,
                       InvokeUi_EnterprisePasswordReuse) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewAboutThisSiteDialogBrowserTest
    : public DialogBrowserTest {
 public:
  PageInfoBubbleViewAboutThisSiteDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {page_info::kPageInfoAboutThisSiteMoreLangs},
        {content_settings::features::kTrackingProtection3pcd});
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");

    optimization_guide::OptimizationMetadata optimization_metadata;
    page_info::proto::AboutThisSiteMetadata metadata;
    auto* site_info = metadata.mutable_site_info();

    auto* description = site_info->mutable_description();
    description->set_description(
        "A domain used in illustrative examples in documents");
    description->set_lang("en_US");
    description->set_name("Example");
    description->mutable_source()->set_url("https://example.com");
    description->mutable_source()->set_label("Example source");

    auto* more_about = site_info->mutable_more_about();
    more_about->set_url("https://example.com/moreinfo");

    optimization_metadata.SetAnyMetadataForTesting(metadata);

    auto* optimization_guide_decider =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    optimization_guide_decider->AddHintForTesting(
        GetUrl(kAboutThisSiteUrl), optimization_guide::proto::ABOUT_THIS_SITE,
        optimization_metadata);
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetUrl(kAboutThisSiteUrl)));
    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string site_name = u"Example site";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              site_name);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));
    if (name == "AboutThisSite") {
      // No further action needed, default case.
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewAboutThisSiteDialogBrowserTest,
                       InvokeUi_AboutThisSite) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewPrivacySandboxDialogBrowserTest
    : public DialogBrowserTest {
 public:
  PageInfoBubbleViewPrivacySandboxDialogBrowserTest() {
    feature_list_.InitAndDisableFeature(
        content_settings::features::kTrackingProtection3pcd);
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl("a.test")));

    // TODO(crbug.com/40210776): It would be better to actually access the
    // topic through Javascript for an end-to-end test when the API is ready.
    auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());

    pscs->OnTopicAccessed(
        url::Origin::Create(GURL("https://a.test")), false,
        privacy_sandbox::CanonicalTopic(browsing_topics::Topic(1),
                                        kTopicsAPITestTaxonomyVersion));

    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string site_name = u"Example site";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              site_name);

    if (name == "PrivacySandboxMain") {
      // No further action needed, default case.
    } else {
      CHECK_EQ(name, "PrivacySandboxSubpage");
      bubble_view->OpenAdPersonalizationPage();
    }
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewPrivacySandboxDialogBrowserTest,
                       InvokeUi_PrivacySandboxMain) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewPrivacySandboxDialogBrowserTest,
                       InvokeUi_PrivacySandboxSubpage) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewHistoryDialogBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewHistoryDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {page_info::kPageInfoHistoryDesktop},
        {content_settings::features::kTrackingProtection3pcd});
  }

  void SetUpOnMainThread() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");

    base::Time yesterday = base::Time::Now() - base::Days(1);
    auto* history_service = HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    history_service->AddPage(GetUrl(kHistoryUrl), yesterday,
                             history::SOURCE_BROWSED);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetUrl(kHistoryUrl)));
    OpenPageInfoBubble(browser());

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    std::u16string site_name = u"Example site";
    bubble_view->presenter_for_testing()->SetSiteNameForTesting(site_name);
    ASSERT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              site_name);
  }

  GURL GetUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewHistoryDialogBrowserTest,
                       InvokeUi_History) {
  ShowAndVerifyUi();
}

class PageInfoBubbleViewCookiesSubpageBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 public:
  PageInfoBubbleViewCookiesSubpageBrowserTest() {
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kPrivacySandboxFirstPartySetsUI},
        {content_settings::features::kTrackingProtection3pcd,
         privacy_sandbox::kTrackingProtection3pcdUx});
  }

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Sat, 1 Sep 2023 11:00:00 UTC", &time));
    return time;
  }

  std::vector<content_settings::TrackingProtectionFeature>
  GetTrackingProtectionFeatures() {
    if (!protections_on_) {
      return {
          {FeatureType::kThirdPartyCookies, enforcement_, Status::kAllowed}};
    }
    if (blocking_status_ == CookieBlocking3pcdStatus::kLimited) {
      return {
          {FeatureType::kThirdPartyCookies, enforcement_, Status::kLimited}};
    }
    return {{FeatureType::kThirdPartyCookies, enforcement_, Status::kBlocked}};
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    PageInfoUI::CookiesNewInfo cookie_info;
    cookie_info.allowed_sites_count = 9;
    cookie_info.enforcement = enforcement_;
    cookie_info.protections_on = protections_on_;
    cookie_info.controls_visible = controls_visible_;
    cookie_info.blocking_status = blocking_status_;
    cookie_info.features = features_;
    // TODO(crbug.com/40854087): Add rws enforcement info when finished
    // implementing it.
    if (rws_enabled_) {
      const std::u16string kSiteOrigin = u"example.com";
      cookie_info.rws_info = {PageInfoUI::CookiesRwsInfo(kSiteOrigin)};
      cookie_info.rws_info->is_managed = rws_managed_;
    }
    if (is_temporary_exception_) {
      cookie_info.expiration = GetReferenceTime() + base::Days(30);
    }

    if (blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd) {
      browser()->profile()->GetPrefs()->SetBoolean(
          prefs::kTrackingProtection3pcdEnabled, true);
    }

    // Open Page Info and wait for it to be fully initialized.
    base::RunLoop run_loop;
    GetPageInfoDialogCreatedCallbackForTesting() = run_loop.QuitClosure();
    OpenPageInfoBubble(browser());
    run_loop.Run();

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    auto* presenter = bubble_view->presenter_for_testing();
    EXPECT_TRUE(presenter);
    EXPECT_TRUE(presenter->ui_for_testing());

    // Open Cookies Subpage and wait for it to be fully initialized.
    base::RunLoop run_loop2;
    bubble_view->OpenCookiesPage();
    auto* cookies_subpage_content = static_cast<PageInfoCookiesContentView*>(
        bubble_view
            ->GetViewByID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW)
            ->children()[1]);
    cookies_subpage_content->SetInitializedCallbackForTesting(
        run_loop2.QuitClosure());
    run_loop2.Run();

    presenter->ui_for_testing()->SetCookieInfo(cookie_info);

    // Removing the focus as with tests run in parallel it causes different
    // outputs.
    bubble_view->GetFocusManager()->SetFocusedView(nullptr);
  }

 protected:
  bool protections_on_ = true;
  bool controls_visible_ = true;
  CookieControlsEnforcement enforcement_ =
      CookieControlsEnforcement::kNoEnforcement;
  CookieBlocking3pcdStatus blocking_status_ =
      CookieBlocking3pcdStatus::kNotIn3pcd;
  std::vector<content_settings::TrackingProtectionFeature> features_ = {
      {FeatureType::kThirdPartyCookies,
       CookieControlsEnforcement::kNoEnforcement, Status::kAllowed}};

  bool rws_enabled_ = false;
  bool rws_managed_ = false;
  bool is_temporary_exception_ = false;

 private:
  // Overriding `base::Time::Now()` to obtain a consistent X days until
  // exception expiration calculation regardless of the time the test runs.
  base::subtle::ScopedTimeClockOverrides time_override_{
      &PageInfoBubbleViewCookiesSubpageBrowserTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesAllowedByTpcdGrant_3pcdLimited) {
  blocking_status_ = CookieBlocking3pcdStatus::kLimited;
  protections_on_ = false;
  controls_visible_ = false;
  enforcement_ = CookieControlsEnforcement::kEnforcedByTpcdGrant;
  features_ = GetTrackingProtectionFeatures();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_RwsOn) {
  rws_enabled_ = true;
  features_ = GetTrackingProtectionFeatures();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_ManagedRwsOn) {
  rws_enabled_ = true;
  rws_managed_ = true;
  features_ = GetTrackingProtectionFeatures();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesBlocked) {
  blocking_status_ = GetParam();
  features_ = GetTrackingProtectionFeatures();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_CookiesAllowedByCookieSetting) {
  blocking_status_ = GetParam();
  protections_on_ = false;
  enforcement_ = CookieControlsEnforcement::kEnforcedByCookieSetting;
  features_ = GetTrackingProtectionFeatures();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewCookiesSubpageBrowserTest,
                       InvokeUi_TemporaryException) {
  is_temporary_exception_ = true;
  blocking_status_ = GetParam();
  protections_on_ = false;
  features_ = GetTrackingProtectionFeatures();
  ShowAndVerifyUi();
}

std::string ParamToTestSuffix(
    const testing::TestParamInfo<
        PageInfoBubbleViewCookiesSubpageBrowserTest::ParamType>& info) {
  std::stringstream name;
  name << "3pcd";
  switch (info.param) {
    case CookieBlocking3pcdStatus::kNotIn3pcd:
      name << "Off";
      break;
    case CookieBlocking3pcdStatus::kLimited:
      name << "Limited";
      break;
    case CookieBlocking3pcdStatus::kAll:
      name << "BlockAll";
      break;
  }
  return name.str();
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    PageInfoBubbleViewCookiesSubpageBrowserTest,
    testing::ValuesIn({CookieBlocking3pcdStatus::kNotIn3pcd,
                       CookieBlocking3pcdStatus::kLimited,
                       CookieBlocking3pcdStatus::kAll}),
    &ParamToTestSuffix);

class PageInfoBubbleViewIsolatedWebAppBrowserTest : public DialogBrowserTest {
 public:
  PageInfoBubbleViewIsolatedWebAppBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode},
        {content_settings::features::kTrackingProtection3pcd});
  }

  void SetUpOnMainThread() override {
    auto dev_server = web_app::CreateAndStartDevServer(
        FILE_PATH_LITERAL("web_apps/simple_isolated_app"));

    auto url_info = web_app::InstallDevModeProxyIsolatedWebApp(
        browser()->profile(), dev_server->GetOrigin());

    start_url_ = url_info.origin().GetURL();
    app_id_ = url_info.app_id();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    Browser* iwa_browser =
        web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id_);

    ASSERT_TRUE(iwa_browser);
    OpenPageInfoBubble(iwa_browser);

    auto* bubble_view = static_cast<PageInfoBubbleView*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting());
    bubble_view->presenter_for_testing()->UpdateSecurityState();
    // For Isolated Web Apps, normal site name gets overridden by app name.
    EXPECT_EQ(bubble_view->presenter_for_testing()->GetSubjectNameForDisplay(),
              u"Simple Isolated App");

    EXPECT_EQ(bubble_view->presenter_for_testing()->site_identity_status(),
              PageInfo::SITE_IDENTITY_STATUS_ISOLATED_WEB_APP);
    EXPECT_EQ(bubble_view->presenter_for_testing()->site_connection_status(),
              PageInfo::SITE_CONNECTION_STATUS_ISOLATED_WEB_APP);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  GURL start_url_;
  webapps::AppId app_id_;

  // Stop test from installing OS hooks.
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

// Test renamed, as currently Skia Gold doesn't support resetting test
// expectation for tests run on windows.
// crbug.com/1403038
IN_PROC_BROWSER_TEST_F(
    PageInfoBubbleViewIsolatedWebAppBrowserTest,
    InvokeUi_AppNameIsDisplayedInsteadOfOriginForIsolatedWebApps_REV2) {
  ShowAndVerifyUi();
}

namespace {
enum class WebAppWindowMode { kBrowserTab, kAppWindow };

std::string WebAppWindowModeToString(
    const testing::TestParamInfo<WebAppWindowMode>& info) {
  switch (info.param) {
    case WebAppWindowMode::kBrowserTab:
      return "BrowserTab";
    case WebAppWindowMode::kAppWindow:
      return "AppWindow";
  }
}
}  // namespace

class PageInfoBubbleViewWebAppBrowserTest
    : public PageInfoBubbleViewDialogBrowserTest,
      public testing::WithParamInterface<WebAppWindowMode> {
 public:
  PageInfoBubbleViewWebAppBrowserTest() {
    feature_list_.InitWithFeatures(
        {
#if BUILDFLAG(IS_MAC)
            features::kAppShimNotificationAttribution
#endif
        },
        {});
  }

  void SetUpOnMainThread() override {
    PageInfoBubbleViewDialogBrowserTest::SetUpOnMainThread();

    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();

    https_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(https_server_.Start());

    start_url_ = https_server_.GetURL("/web_apps/basic.html");
    app_id_ = web_app::InstallWebAppFromPage(browser(), start_url_);
  }

  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(browser()->profile());
    override_registration_.reset();

    PageInfoBubbleViewDialogBrowserTest::TearDownOnMainThread();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name_with_param_suffix) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    const std::string& name =
        name_with_param_suffix.substr(0, name_with_param_suffix.find("/"));

#if BUILDFLAG(IS_MAC)
    if (name == "NotificationSystemPermissionDenied") {
      AppShimRegistry::Get()->SaveNotificationPermissionStatusForApp(
          app_id_, mac_notifications::mojom::PermissionStatus::kDenied);

      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->SetContentSettingDefaultScope(
              start_url_, start_url_, ContentSettingsType::NOTIFICATIONS,
              ContentSetting::CONTENT_SETTING_ALLOW);
    }
#endif

    Browser* app_browser = browser();
    switch (GetParam()) {
      case WebAppWindowMode::kBrowserTab:
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url_));
        break;
      case WebAppWindowMode::kAppWindow:
        app_browser =
            web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id_);
        ASSERT_TRUE(app_browser);
        break;
    }
    OpenPageInfoBubble(app_browser);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  GURL start_url_;
  webapps::AppId app_id_;

  std::unique_ptr<
      ::web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewWebAppBrowserTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewWebAppBrowserTest,
                       InvokeUi_NotificationSystemPermissionDenied) {
  ShowAndVerifyUi();
}
#endif

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    PageInfoBubbleViewWebAppBrowserTest,
    testing::ValuesIn({WebAppWindowMode::kBrowserTab,
                       WebAppWindowMode::kAppWindow}),
    &WebAppWindowModeToString);
