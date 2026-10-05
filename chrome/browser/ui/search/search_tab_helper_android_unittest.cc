// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/common/search/mock_embedded_search_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

constexpr char kThirdPartyNtpUrl[] = "https://foo.com/newtab";

class MockEmbeddedSearchClientFactory
    : public SearchIPCRouter::EmbeddedSearchClientFactory {
 public:
  MOCK_METHOD(search::mojom::EmbeddedSearchClient*,
              GetEmbeddedSearchClient,
              (),
              (override));
  MOCK_METHOD(void,
              BindFactoryReceiver,
              (mojo::PendingAssociatedReceiver<
                   search::mojom::EmbeddedSearchConnector> receiver,
               content::RenderFrameHost* rfh),
              (override));
};

// Tests the Android-specific behavior of SearchTabHelper. Android has no
// Browser / InstantController, so the helper itself (1) derives the tab's
// active state from the WebContents visibility and (2) pushes the NTP theme
// and Most Visited info to a third-party remote Instant NTP when it commits.
class SearchTabHelperAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  SearchTabHelperAndroidTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpEnableInstantApiAndroid,
                              chrome::android::kUseWebUiNtp3PDSE},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Use a non-Google default search engine with a remote New Tab URL, so
    // that the NTP is a third-party remote Instant NTP.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(u"foo.com");
    data.SetURL("https://foo.com/url?bar={searchTerms}");
    data.new_tab_url = kThirdPartyNtpUrl;
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

    // Ensure the WebContents has a committed entry and a process.
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("about:blank"));
  }

  void TearDown() override {
    search_tab_helper_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Creates the SearchTabHelper under test and routes its EmbeddedSearchClient
  // to `mock_client()`. The real SearchIPCRouterPolicyImpl is kept so that the
  // policy checks (e.g. search::IsInstantNTP()) are exercised too.
  void CreateSearchTabHelper() {
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_);
    search_tab_helper_ =
        std::make_unique<SearchTabHelper>(mock_tab_, web_contents());

    auto factory =
        std::make_unique<NiceMock<MockEmbeddedSearchClientFactory>>();
    ON_CALL(*factory, GetEmbeddedSearchClient())
        .WillByDefault(Return(&mock_embedded_search_client_));
    ipc_router().set_embedded_search_client_factory_for_testing(
        std::move(factory));
  }

  // Navigates the tab to `url` and commits it in a process registered as an
  // Instant process. This mirrors what ChromeContentBrowserClient does for NTP
  // URLs in production and is required for search::IsInstantNTP() to be true
  // once the navigation commits.
  void CommitNavigationInInstantProcess(const GURL& url) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    simulator->Start();
    simulator->ReadyToCommit();

    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(instant_service);
    instant_service->AddInstantProcess(
        simulator->GetNavigationHandle()->GetRenderFrameHost()->GetProcess());

    simulator->Commit();
  }

  SearchIPCRouter& ipc_router() {
    return search_tab_helper_->ipc_router_for_testing();
  }

  bool is_active_tab() { return ipc_router().is_active_tab_for_testing(); }

  NiceMock<MockEmbeddedSearchClient>& mock_client() {
    return mock_embedded_search_client_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  tabs::MockTabInterface mock_tab_;
  NiceMock<MockEmbeddedSearchClient> mock_embedded_search_client_;
  std::unique_ptr<SearchTabHelper> search_tab_helper_;
};

// A tab that is already visible when the helper is created (e.g. a new tab
// opened in the foreground) must be treated as active right away.
TEST_F(SearchTabHelperAndroidTest, VisibleTabIsActiveOnCreation) {
  ASSERT_EQ(content::Visibility::VISIBLE, web_contents()->GetVisibility());

  CreateSearchTabHelper();

  EXPECT_TRUE(is_active_tab());
}

// A tab that is hidden when the helper is created (e.g. a background tab
// restored at startup) must not be treated as active.
TEST_F(SearchTabHelperAndroidTest, HiddenTabIsInactiveOnCreation) {
  web_contents()->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());

  CreateSearchTabHelper();

  EXPECT_FALSE(is_active_tab());
}

// Visibility changes act as tab (de)activation: hiding the tab deactivates it,
// showing it again activates it, and occlusion does not change anything.
TEST_F(SearchTabHelperAndroidTest, VisibilityChangesToggleActiveState) {
  CreateSearchTabHelper();
  ASSERT_TRUE(is_active_tab());

  web_contents()->WasHidden();
  EXPECT_FALSE(is_active_tab());

  web_contents()->WasShown();
  EXPECT_TRUE(is_active_tab());

  web_contents()->WasOccluded();
  EXPECT_TRUE(is_active_tab());

  web_contents()->WasShown();
  EXPECT_TRUE(is_active_tab());

  web_contents()->WasHidden();
  EXPECT_FALSE(is_active_tab());
}

// Committing a third-party remote Instant NTP immediately pushes the NTP theme
// and the Most Visited info to the page. On desktop this is done by
// InstantController, which does not exist on Android.
TEST_F(SearchTabHelperAndroidTest, PushesThemeAndMostVisitedInfoOnNtpCommit) {
  CreateSearchTabHelper();

  EXPECT_CALL(mock_client(), ThemeChanged(_)).Times(1);
  EXPECT_CALL(mock_client(), MostVisitedInfoChanged(_)).Times(AtLeast(1));

  CommitNavigationInInstantProcess(GURL(kThirdPartyNtpUrl));

  EXPECT_TRUE(search::IsInstantNTP(web_contents()));
  Mock::VerifyAndClearExpectations(&mock_client());
}

// Committing a page that is not the NTP, even inside an Instant process, must
// not push anything to the page.
TEST_F(SearchTabHelperAndroidTest, DoesNotPushOnNonNtpCommit) {
  CreateSearchTabHelper();

  EXPECT_CALL(mock_client(), ThemeChanged(_)).Times(0);
  EXPECT_CALL(mock_client(), MostVisitedInfoChanged(_)).Times(0);

  CommitNavigationInInstantProcess(GURL("https://foo.com/not-the-ntp"));

  EXPECT_FALSE(search::IsInstantNTP(web_contents()));
  Mock::VerifyAndClearExpectations(&mock_client());
}

}  // namespace
