// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kNonExtensionNtpUrl[] = "https://www.google.com";
}

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
        /*enabled_features=*/{ntp_features::kNtpFooter},
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

TEST_F(FooterControllerExtensionTest, FooterHidden_NonExtensionNTP) {
  profile()->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, true);
  // After a pref change, there's an attempt to show the footer.
  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());
  // Clear the value of `FakeNewTabFooterWebView::did_close_ui()` to
  // check that the navigation also results in a hidden footer.
  browser_window()->NewTabFooterWebView()->Clear();

  EXPECT_FALSE(browser_window()->NewTabFooterWebView()->did_close_ui());
  tab_interface()->NavigateTo(GURL(kNonExtensionNtpUrl));

  EXPECT_TRUE(browser_window()->NewTabFooterWebView()->did_close_ui());
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
