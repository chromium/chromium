// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "components/search/ntp_features.h"
#endif

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// Returns the remote New Tab Page URL of the third-party search engine created
// by SetThirdPartyDefaultSearchProvider() for `host`.
GURL ThirdPartyNtpUrl(const std::string& host) {
  return GURL("https://" + host + "/newtab");
}

// Tests BrowserInstantController, which reloads the NTP tabs of a browser
// window through the window's TabListInterface when the default search engine
// changes. The window and its tab list are mocked, so the test exercises the
// same code path on every platform.
class BrowserInstantControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  BrowserInstantControllerTest() {
#if BUILDFLAG(IS_ANDROID)
    // On Android the WebUI NTP, the third-party NTPs it routes to and the
    // Instant API are all behind flags.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chrome::android::kUseWebUiNtpAndroid,
                              chrome::android::kUseWebUiNtp3PDSE,
                              ntp_features::kNtpEnableInstantApiAndroid},
        /*disabled_features=*/{});
#endif
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::set_is_desktop_for_testing(true);
#endif

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service());

    ON_CALL(browser_window_, GetProfile()).WillByDefault(Return(profile()));

    ON_CALL(tab_list_, GetTabCount()).WillByDefault([this] {
      return static_cast<int>(tabs_.size());
    });
    ON_CALL(tab_list_, GetTab(_))
        .WillByDefault([this](int index) -> tabs::TabInterface* {
          if (index < 0 || static_cast<size_t>(index) >= tabs_.size()) {
            return nullptr;
          }
          return tabs_[index].get();
        });
  }

  void TearDown() override {
    controller_.reset();
    tab_list_registration_.reset();
    tabs_.clear();
    web_contents_.clear();
#if BUILDFLAG(IS_ANDROID)
    base::android::device_info::reset_is_desktop_for_testing();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  TemplateURLService* template_url_service() {
    return TemplateURLServiceFactory::GetForProfile(profile());
  }

  void CreateController() {
    controller_ = std::make_unique<BrowserInstantController>(browser_window_);
  }

  void DestroyController() { controller_.reset(); }

  BrowserInstantController* controller() { return controller_.get(); }

  BrowserWindowInterface* browser_window() { return &browser_window_; }

  // Exposes the tab list as the browser window's TabListInterface, like
  // TabListBridge (desktop) and TabModelJniBridge::AssociateWithBrowserWindow()
  // (Android) do in production.
  void AssociateTabListWithBrowserWindow() {
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            browser_window_.GetUnownedUserDataHost(), tab_list_);
  }

  // Appends a tab whose main frame lives in the SiteInstance for `site_url`,
  // without navigating it. BrowserInstantController only looks at the
  // SiteInstance's security principal and at the renderer process, and
  // committing a chrome:// URL in a unit test would instantiate the real WebUI.
  content::WebContents* AddTab(const GURL& site_url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(
            profile(),
            content::SiteInstance::CreateForURL(profile(), site_url));
    content::WebContents* raw_contents = contents.get();
    web_contents_.push_back(std::move(contents));

    auto tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab, GetContents()).WillByDefault(Return(raw_contents));
    tabs_.push_back(std::move(tab));
    return raw_contents;
  }

  // Appends a tab without WebContents, like an Android tab that has not been
  // restored yet.
  void AddTabWithoutWebContents() {
    auto tab = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*tab, GetContents()).WillByDefault(Return(nullptr));
    tabs_.push_back(std::move(tab));
  }

  // Makes a third-party search engine hosted on `host`, whose New Tab Page is
  // ThirdPartyNtpUrl(host), the default search provider.
  void SetThirdPartyDefaultSearchProvider(const std::string& host) {
    TemplateURLData data;
    data.SetShortName(base::UTF8ToUTF16(host));
    data.SetKeyword(base::UTF8ToUTF16(host));
    data.SetURL("https://" + host + "/search?q={searchTerms}");
    data.new_tab_url = ThirdPartyNtpUrl(host).spec();
    TemplateURL* template_url =
        template_url_service()->Add(std::make_unique<TemplateURL>(data));
    template_url_service()->SetUserSelectedDefaultSearchProvider(template_url);
  }

  // Returns the URL `contents` is being reloaded to by
  // BrowserInstantController, or an empty GURL if no such reload is pending.
  // The controller navigates to chrome://newtab, which the platform's
  // BrowserURLHandler rewrites to the NTP of the current default search engine,
  // so chrome://newtab stays the virtual URL of the pending entry.
  static GURL PendingNtpReloadUrl(content::WebContents* contents) {
    content::NavigationEntry* entry =
        contents->GetController().GetPendingEntry();
    if (!entry) {
      return GURL();
    }
    EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(), entry->GetVirtualURL());
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                             ui::PAGE_TRANSITION_RELOAD));
    return entry->GetURL();
  }

 private:
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list_;
#endif
  NiceMock<MockBrowserWindowInterface> browser_window_;
  NiceMock<MockTabListInterface> tab_list_;
  std::vector<std::unique_ptr<content::WebContents>> web_contents_;
  std::vector<std::unique_ptr<tabs::MockTabInterface>> tabs_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  std::unique_ptr<BrowserInstantController> controller_;
};

// Changing the default search engine reloads the WebUI NTP tabs of the window
// (first-party and third-party), and only those.
TEST_F(BrowserInstantControllerTest,
       ReloadsWebUiNtpTabsOnDefaultSearchProviderChange) {
  // Arrange.
  AssociateTabListWithBrowserWindow();
  content::WebContents* first_party_ntp =
      AddTab(GURL(chrome::kChromeUINewTabPageURL));
  content::WebContents* third_party_ntp =
      AddTab(GURL(chrome::kChromeUINewTabPageThirdPartyURL));
  content::WebContents* web_page = AddTab(GURL("https://example.com/"));
  content::WebContents* other_webui = AddTab(GURL("chrome://version/"));
  CreateController();

  // Act.
  SetThirdPartyDefaultSearchProvider("foo.com");

  // Assert.
  EXPECT_EQ(ThirdPartyNtpUrl("foo.com"), PendingNtpReloadUrl(first_party_ntp));
  EXPECT_EQ(ThirdPartyNtpUrl("foo.com"), PendingNtpReloadUrl(third_party_ntp));
  EXPECT_EQ(GURL(), PendingNtpReloadUrl(web_page));
  EXPECT_EQ(GURL(), PendingNtpReloadUrl(other_webui));
}

// A remote third-party NTP is recognized through its Instant process and
// reloaded to the NTP of the new default search engine.
TEST_F(BrowserInstantControllerTest, ReloadsRemoteNtpInInstantProcess) {
  // Arrange.
  SetThirdPartyDefaultSearchProvider("foo.com");
  AssociateTabListWithBrowserWindow();
  content::WebContents* remote_ntp = AddTab(ThirdPartyNtpUrl("foo.com"));
  // ChromeContentBrowserClient registers the process of an NTP SiteInstance as
  // an Instant process when the process is assigned; do it explicitly so the
  // test doesn't depend on that timing.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(instant_service);
  instant_service->AddInstantProcess(
      remote_ntp->GetPrimaryMainFrame()->GetProcess());
  CreateController();

  // Act.
  SetThirdPartyDefaultSearchProvider("bar.com");

  // Assert.
  EXPECT_EQ(ThirdPartyNtpUrl("bar.com"), PendingNtpReloadUrl(remote_ntp));
}

// Changes to the TemplateURLService that don't change the default search
// provider don't reload anything.
TEST_F(BrowserInstantControllerTest,
       IgnoresUnrelatedTemplateURLServiceChanges) {
  // Arrange.
  AssociateTabListWithBrowserWindow();
  content::WebContents* ntp = AddTab(GURL(chrome::kChromeUINewTabPageURL));
  CreateController();

  // Act.
  TemplateURLData data;
  data.SetShortName(u"other");
  data.SetKeyword(u"other");
  data.SetURL("https://other.com/search?q={searchTerms}");
  template_url_service()->Add(std::make_unique<TemplateURL>(data));

  // Assert.
  EXPECT_EQ(GURL(), PendingNtpReloadUrl(ntp));
}

// Tabs without WebContents (e.g. Android tabs that haven't been restored yet)
// are skipped without affecting the other tabs.
TEST_F(BrowserInstantControllerTest, SkipsTabsWithoutWebContents) {
  // Arrange.
  AssociateTabListWithBrowserWindow();
  content::WebContents* ntp = AddTab(GURL(chrome::kChromeUINewTabPageURL));
  AddTabWithoutWebContents();
  content::WebContents* other_ntp =
      AddTab(GURL(chrome::kChromeUINewTabPageURL));
  CreateController();

  // Act.
  SetThirdPartyDefaultSearchProvider("foo.com");

  // Assert.
  EXPECT_EQ(ThirdPartyNtpUrl("foo.com"), PendingNtpReloadUrl(ntp));
  EXPECT_EQ(ThirdPartyNtpUrl("foo.com"), PendingNtpReloadUrl(other_ntp));
}

// The tab list may only be associated with the browser window after the
// controller exists (on Android the TabModel is associated after the window is
// created). Until then there is nothing to reload, and the association must be
// picked up afterwards.
TEST_F(BrowserInstantControllerTest, LooksUpTabListLazily) {
  // Arrange.
  content::WebContents* ntp = AddTab(GURL(chrome::kChromeUINewTabPageURL));
  CreateController();

  // Act & Assert.
  SetThirdPartyDefaultSearchProvider("foo.com");
  EXPECT_EQ(GURL(), PendingNtpReloadUrl(ntp));

  // Act & Assert.
  AssociateTabListWithBrowserWindow();
  SetThirdPartyDefaultSearchProvider("bar.com");
  EXPECT_EQ(ThirdPartyNtpUrl("bar.com"), PendingNtpReloadUrl(ntp));
}

// The controller registers itself as unowned user data of its browser window.
TEST_F(BrowserInstantControllerTest, IsRetrievableFromBrowserWindow) {
  EXPECT_EQ(nullptr, BrowserInstantController::From(nullptr));
  EXPECT_EQ(nullptr, BrowserInstantController::From(browser_window()));

  CreateController();
  EXPECT_EQ(controller(), BrowserInstantController::From(browser_window()));

  DestroyController();
  EXPECT_EQ(nullptr, BrowserInstantController::From(browser_window()));
}

}  // namespace
