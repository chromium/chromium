// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_tab_helper.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/search/search_ipc_router.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/search/mock_embedded_search_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

class OmniboxView;

using testing::Eq;
using testing::Return;
using testing::_;

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD1(FocusOmnibox, void(bool focus));
  MOCK_METHOD1(OnDeleteMostVisitedItem, void(const GURL& url));
  MOCK_METHOD1(OnUndoMostVisitedDeletion, void(const GURL& url));
  MOCK_METHOD0(OnUndoAllMostVisitedDeletions, void());
  MOCK_METHOD2(OnAddCustomLink,
               bool(const GURL& url, const std::string& title));
  MOCK_METHOD3(OnUpdateCustomLink,
               bool(const GURL& url,
                    const GURL& new_url,
                    const std::string& new_title));
  MOCK_METHOD1(OnDeleteCustomLink, bool(const GURL& url));
  MOCK_METHOD0(OnUndoCustomLinkAction, void());
  MOCK_METHOD0(OnResetCustomLinks, void());
  MOCK_METHOD2(
      OnDoesUrlResolve,
      void(const GURL& url,
           chrome::mojom::EmbeddedSearch::DoesUrlResolveCallback callback));
  MOCK_METHOD2(OnLogEvent, void(NTPLoggingEventType event,
                                base::TimeDelta time));
  MOCK_METHOD1(OnLogMostVisitedImpression,
               void(const ntp_tiles::NTPTileImpression& impression));
  MOCK_METHOD1(OnLogMostVisitedNavigation,
               void(const ntp_tiles::NTPTileImpression& impression));
  MOCK_METHOD1(PasteIntoOmnibox, void(const base::string16&));
  MOCK_METHOD1(ChromeIdentityCheck, bool(const base::string16& identity));
  MOCK_METHOD0(HistorySyncCheck, bool());
  MOCK_METHOD1(OnSetCustomBackgroundURL, void(const GURL& url));
  MOCK_METHOD4(OnSetCustomBackgroundURLWithAttributions,
               void(const GURL& background_url,
                    const std::string& attribution_line_1,
                    const std::string& attribution_line_2,
                    const GURL& action_url));
  MOCK_METHOD0(OnSelectLocalBackgroundImage, void());
};

class MockEmbeddedSearchClientFactory
    : public SearchIPCRouter::EmbeddedSearchClientFactory {
 public:
  MOCK_METHOD0(GetEmbeddedSearchClient,
               chrome::mojom::EmbeddedSearchClient*(void));
};

}  // namespace

class SearchTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SearchTabHelperTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    SearchTabHelper::CreateForWebContents(web_contents());
    auto* search_tab = SearchTabHelper::FromWebContents(web_contents());
    auto factory = std::make_unique<MockEmbeddedSearchClientFactory>();
    ON_CALL(*factory, GetEmbeddedSearchClient())
        .WillByDefault(Return(&mock_embedded_search_client_));
    search_tab->ipc_router_for_testing()
        .set_embedded_search_client_factory_for_testing(std::move(factory));
  }

  void TearDown() override {
    // |identity_test_env_adaptor_| must be destroyed before profile().
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::BrowserContext* CreateBrowserContext() override {
    TestingProfile::TestingFactories factories = {
        {ProfileSyncServiceFactory::GetInstance(),
         base::BindRepeating(&BuildMockProfileSyncService)}};

    // Per comments on content::RenderViewHostTestHarness, it takes ownership of
    // the returned object.
    return IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(factories)
            .release();
  }

  // Associates |email| with profile as the primary account. |email|
  // should not be empty.
  void SetUpAccount(const std::string& email) {
    ASSERT_FALSE(email.empty());
    identity_test_env()->SetPrimaryAccount(email);
  }

  // Configure the account to |sync_history| or not.
  void SetHistorySync(bool sync_history) {
    browser_sync::ProfileSyncServiceMock* sync_service =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile()));

    syncer::ModelTypeSet result;
    if (sync_history) {
      result.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    }
    EXPECT_CALL(*sync_service, GetPreferredDataTypes())
        .WillRepeatedly(Return(result));
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

  MockEmbeddedSearchClient* mock_embedded_search_client() {
    return &mock_embedded_search_client_;
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    DCHECK(identity_test_env_adaptor_);
    return identity_test_env_adaptor_->identity_test_env();
  }

 private:
  MockSearchIPCRouterDelegate delegate_;
  MockEmbeddedSearchClient mock_embedded_search_client_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(SearchTabHelperTest, ChromeIdentityCheckMatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetUpAccount("foo@bar.com");
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  EXPECT_TRUE(search_tab_helper->ChromeIdentityCheck(test_identity));
}

TEST_F(SearchTabHelperTest, ChromeIdentityCheckMatchSlightlyDifferentGmail) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetUpAccount("foobar123@gmail.com");
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  // For gmail, canonicalization is done so that email addresses have a
  // standard form.
  const base::string16 test_identity =
      base::ASCIIToUTF16("Foo.Bar.123@gmail.com");
  EXPECT_TRUE(search_tab_helper->ChromeIdentityCheck(test_identity));
}

TEST_F(SearchTabHelperTest, ChromeIdentityCheckMatchSlightlyDifferentGmail2) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetUpAccount("chrome.user.7FOREVER");
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  // For gmail/googlemail, canonicalization is done so that email addresses have
  // a standard form.
  const base::string16 test_identity =
      base::ASCIIToUTF16("chromeuser7forever@googlemail.com");
  EXPECT_TRUE(search_tab_helper->ChromeIdentityCheck(test_identity));
}

TEST_F(SearchTabHelperTest, ChromeIdentityCheckMismatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetUpAccount("foo@bar.com");
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("bar@foo.com");
  EXPECT_FALSE(search_tab_helper->ChromeIdentityCheck(test_identity));
}

TEST_F(SearchTabHelperTest, ChromeIdentityCheckSignedOutMismatch) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  // This test does not sign in.
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  const base::string16 test_identity = base::ASCIIToUTF16("bar@foo.com");
  EXPECT_FALSE(search_tab_helper->ChromeIdentityCheck(test_identity));
}

TEST_F(SearchTabHelperTest, HistorySyncCheckSyncing) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetHistorySync(true);
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  EXPECT_TRUE(search_tab_helper->HistorySyncCheck());
}

TEST_F(SearchTabHelperTest, HistorySyncCheckNotSyncing) {
  NavigateAndCommit(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetHistorySync(false);
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  EXPECT_FALSE(search_tab_helper->HistorySyncCheck());
}

TEST_F(SearchTabHelperTest, FileSelectedUpdatesLastSelectedDirectory) {
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  SearchTabHelper* search_tab_helper =
      SearchTabHelper::FromWebContents(web_contents());
  ASSERT_NE(nullptr, search_tab_helper);

  base::FilePath filePath =
      base::FilePath::FromUTF8Unsafe("a/b/c/Picture/kitten.png");
  search_tab_helper->FileSelected(filePath, 0, {});
  Profile* profile = search_tab_helper->profile();
  EXPECT_EQ(filePath.DirName(), profile->last_selected_directory());
}

TEST_F(SearchTabHelperTest, TitleIsSetForNTP) {
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE),
            web_contents()->GetTitle());
}
