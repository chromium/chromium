// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kFirstPartyDomain[] = "{google:baseURL}";
const char kNonNtpUrl[] = "https://www.google.com";

void InstallTemplateURLWithNewTabPage(Profile* profile) {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile,
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

  TemplateURLData data;
  data.SetURL(std::string(kFirstPartyDomain) + "search?q={searchTerms}");
  data.new_tab_url = std::string(kFirstPartyDomain) + "_/chrome/newtab";
  data.SetShortName(u"first party");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}
}  // namespace

class FakeNewTabFooterWebView : public new_tab_footer::NewTabFooterWebView {
 public:
  explicit FakeNewTabFooterWebView(BrowserWindowInterface* browser_window)
      : NewTabFooterWebView(browser_window) {}
  ~FakeNewTabFooterWebView() override {}

  bool did_show_ui() { return did_show_ui_; }
  bool did_close_ui() { return did_close_ui_; }

  void Clear() {
    did_show_ui_ = false;
    did_close_ui_ = false;
  }

 protected:
  // WebUIContentsWrapper::Host:
  void ShowUI() override { did_show_ui_ = true; }
  void CloseUI() override { did_close_ui_ = true; }

  bool did_show_ui_ = false;
  bool did_close_ui_ = false;
};

class FakeBrowserWindowInterface : public MockBrowserWindowInterface {
 public:
  explicit FakeBrowserWindowInterface(Profile* profile) { profile_ = profile; }
  ~FakeBrowserWindowInterface() override = default;

  // MockBrowserWindowInterface:
  FakeNewTabFooterWebView* NewTabFooterWebView() override {
    return footer_web_view_;
  }
  Profile* GetProfile() override { return profile_.get(); }

  void SetFooterWebView(FakeNewTabFooterWebView* footer_web_view) {
    footer_web_view_ = footer_web_view;
  }

 protected:
  raw_ptr<Profile> profile_;
  raw_ptr<FakeNewTabFooterWebView> footer_web_view_;
};

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  FakeTabInterface(FakeBrowserWindowInterface* browser_window,
                   content::BrowserContext* browser_context) {
    browser_window_ = browser_window;
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context, nullptr);
    footer_web_view_ =
        std::make_unique<FakeNewTabFooterWebView>(browser_window);
    browser_window_->SetFooterWebView(footer_web_view_.get());
  }
  ~FakeTabInterface() override {
    browser_window_->SetFooterWebView(nullptr);
    footer_web_view_.reset();
  }

  // tabs::MockTabInterface:
  FakeBrowserWindowInterface* GetBrowserWindowInterface() override {
    return browser_window_;
  }
  content::WebContents* GetContents() const override {
    return web_contents_.get();
  }
  bool IsActivated() const override { return true; }

  void NavigateTo(const GURL url) {
    EXPECT_TRUE(web_contents_.get());
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents_.get());
    web_contents_tester->NavigateAndCommit(url);
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<FakeNewTabFooterWebView> footer_web_view_;
  raw_ptr<FakeBrowserWindowInterface> browser_window_;
};

class FooterControllerExtensionTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter,
                              features::kEnterpriseBadgingForNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    browser_window_ = std::make_unique<FakeBrowserWindowInterface>(profile());
    tab_ = std::make_unique<FakeTabInterface>(browser_window_.get(),
                                              browser_context());
    controller_ =
        std::make_unique<new_tab_footer::NewTabFooterController>(tab_.get());
  }

  void TearDown() override {
    controller_.reset();
    tab_.reset();
    browser_window_.reset();
    ExtensionServiceTestBase::TearDown();
  }

  scoped_refptr<const extensions::Extension> LoadNtpExtension() {
    extensions::TestExtensionDir extension_dir;
    const std::string kManifest = R"(
      {
        "chrome_url_overrides": {
            "newtab": "ext.html"
        },
        "name": "Extension-overridden NTP",
          "manifest_version": 3,
          "version": "0.1"
      })";
    extension_dir.WriteManifest(kManifest);
    extension_dir.WriteFile(FILE_PATH_LITERAL("ext.html"),
                            "<body>Extension-overridden NTP</body>");
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    scoped_refptr<const extensions::Extension> extension =
        extension_loader.LoadExtension(extension_dir.Pack());
    return extension;
  }

  FakeBrowserWindowInterface* browser_window() { return browser_window_.get(); }

  FakeTabInterface* tab_interface() { return tab_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<FakeBrowserWindowInterface> browser_window_;
  std::unique_ptr<FakeTabInterface> tab_;
  std::unique_ptr<new_tab_footer::NewTabFooterController> controller_;
};

TEST_F(FooterControllerExtensionTest, FooterShown_ExtensionNTP) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(extension->url());

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

// Ensures that the footer is hidden on all non-extension NTPs.
TEST_F(FooterControllerExtensionTest, FooterHidden_NonExtensionNTP) {
  InstallTemplateURLWithNewTabPage(profile());
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  // After a pref change, there's an attempt to show the footer.
  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());
  // Clear the value of `FakeNewTabFooterWebView::did_close_ui()` to
  // check that the navigation also results in a hidden footer.
  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to non-NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  tab_interface()->NavigateTo(GURL(kNonNtpUrl));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to  1P NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to 3P NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageThirdPartyURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to default NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();
}

// Ensures footer is shown on extension NTPs when
// `prefs::kNtpFooterVisible` is set to true.
TEST_F(FooterControllerExtensionTest, FooterShown_UserPref) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  tab_interface()->NavigateTo(extension->url());
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());

  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

// Ensures footer is hidden on extension NTPs when
// `prefs::kNtpFooterVisible` is set to false.
TEST_F(FooterControllerExtensionTest, FooterHidden_UserPref) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  // After a pref change, there's an attempt to show the footer.
  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());
  // Clear the value of `FakeNewTabFooterWebView::did_close_ui()` to check
  // the effect of pref changes on an extension NTP.
  browser_window()->NewTabFooterWebView()->Clear();
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());

  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  tab_interface()->NavigateTo(extension->url());
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());

  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());
}

// Ensures the footer is hidden if there is an extension NTP, but attribution is
// disabled by policy.
TEST_F(FooterControllerExtensionTest,
       FooterHidden_ExtensionNTP_AttributionDisabledByPolicy) {
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(extension->url());

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

// Ensures the footer is shown on extension NTP even when the browser is
// managed.
TEST_F(FooterControllerExtensionTest, FooterShown_ExtensionNTP_Management) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, true);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(extension->url());

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

// Ensures the footer is hidden if the policy is set while the footer is shown.
TEST_F(FooterControllerExtensionTest,
       FooterHidden_ExtensionNTP_AttributionPolicyChange) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(extension->url());

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, true);

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Ensures footer is shown on non-extension NTP if the browser is also managed.
TEST_F(FooterControllerExtensionTest, FooterShown_NonExtensionNTP_Management) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  InstallTemplateURLWithNewTabPage(profile());
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

// Ensures that the footer is shown for the extension attribution even when the
// management notice is disabledny policy for a managed browser.
TEST_F(FooterControllerExtensionTest,
       FooterShown_ExtensionNTP_ManagementNoticeDisabledByPolicy) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  testing_local_state_.Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, false);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(extension->url());

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

class FooterControllerEnterpriseTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpFooter,
                              features::kEnterpriseBadgingForNtpFooter},
        /*disabled_features=*/{features::kSideBySide});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Test Profile");
    InstallTemplateURLWithNewTabPage(profile());
    browser_window_ = std::make_unique<FakeBrowserWindowInterface>(profile());
    tab_ = std::make_unique<FakeTabInterface>(browser_window_.get(), profile());
    controller_ =
        std::make_unique<new_tab_footer::NewTabFooterController>(tab_.get());
  }

  void TearDown() override {
    controller_.reset();
    tab_.reset();
    browser_window_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  Profile* profile() { return profile_; }

  FakeBrowserWindowInterface* browser_window() { return browser_window_.get(); }

  FakeTabInterface* tab_interface() { return tab_.get(); }
  content::BrowserContext* browser_context() { return profile(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  content::RenderViewHostTestEnabler rvh_test_enabler;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakeBrowserWindowInterface> browser_window_;
  std::unique_ptr<FakeTabInterface> tab_;
  std::unique_ptr<new_tab_footer::NewTabFooterController> controller_;
};

// Ensures that the footer is shown for any NTP in a managed browser.
TEST_F(FooterControllerEnterpriseTest, FooterShown_AnyNTP) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  // Navigate to non-NTP first and ensure it is hidden.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  tab_interface()->NavigateTo(GURL(kNonNtpUrl));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to  1P NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());

  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to 3P NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageThirdPartyURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());

  browser_window()->NewTabFooterWebView()->Clear();

  // Navigate to default NTP.
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());

  browser_window()->NewTabFooterWebView()->Clear();
}

// Ensures that the footer is hidden if the management notice is disabled by
// policy.
TEST_F(FooterControllerEnterpriseTest, FooterHidden_NoticePolicyDisabled) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
  profile_manager_->local_state()->Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, false);

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
}

// Ensures that the footer becomes hidden after the policy is set while the
// footer is showing.
TEST_F(FooterControllerEnterpriseTest, FooterHidden_NoticePolicyChange) {
  // Simulate browser management.
  policy::ScopedManagementServiceOverrideForTesting
      profile_supervised_management(
          policy::ManagementServiceFactory::GetForProfile(profile()),
          policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_show_ui());
  tab_interface()->NavigateTo(GURL(chrome::kChromeUINewTabPageURL));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  profile_manager_->local_state()->Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, false);

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());

  browser_window()->NewTabFooterWebView()->Clear();
  profile_manager_->local_state()->Get()->SetBoolean(
      prefs::kNTPFooterManagementNoticeEnabled, true);

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_show_ui());
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
