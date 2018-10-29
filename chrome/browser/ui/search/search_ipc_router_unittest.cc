// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search_ipc_router_policy_impl.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/mock_embedded_search_client.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/favicon_base/favicon_types.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using testing::_;
using testing::Field;
using testing::Return;

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
                    const std::string& attribution1,
                    const std::string& attribution2,
                    const GURL& attributionActionUrl));
  MOCK_METHOD0(OnSelectLocalBackgroundImage, void());
};

class MockSearchIPCRouterPolicy : public SearchIPCRouter::Policy {
 public:
  ~MockSearchIPCRouterPolicy() override {}

  MOCK_METHOD1(ShouldProcessFocusOmnibox, bool(bool));
  MOCK_METHOD0(ShouldProcessDeleteMostVisitedItem, bool());
  MOCK_METHOD0(ShouldProcessUndoMostVisitedDeletion, bool());
  MOCK_METHOD0(ShouldProcessUndoAllMostVisitedDeletions, bool());
  MOCK_METHOD0(ShouldProcessAddCustomLink, bool());
  MOCK_METHOD0(ShouldProcessUpdateCustomLink, bool());
  MOCK_METHOD0(ShouldProcessDeleteCustomLink, bool());
  MOCK_METHOD0(ShouldProcessUndoCustomLinkAction, bool());
  MOCK_METHOD0(ShouldProcessResetCustomLinks, bool());
  MOCK_METHOD0(ShouldProcessDoesUrlResolve, bool());
  MOCK_METHOD0(ShouldProcessLogEvent, bool());
  MOCK_METHOD1(ShouldProcessPasteIntoOmnibox, bool(bool));
  MOCK_METHOD0(ShouldProcessChromeIdentityCheck, bool());
  MOCK_METHOD0(ShouldProcessHistorySyncCheck, bool());
  MOCK_METHOD0(ShouldProcessSetCustomBackgroundURL, bool());
  MOCK_METHOD0(ShouldProcessSetCustomBackgroundURLWithAttributions, bool());
  MOCK_METHOD0(ShouldProcessSelectLocalBackgroundImage, bool());
  MOCK_METHOD1(ShouldSendSetInputInProgress, bool(bool));
  MOCK_METHOD0(ShouldSendOmniboxFocusChanged, bool());
  MOCK_METHOD0(ShouldSendMostVisitedItems, bool());
  MOCK_METHOD0(ShouldSendThemeBackgroundInfo, bool());
};

class MockEmbeddedSearchClientFactory
    : public SearchIPCRouter::EmbeddedSearchClientFactory {
 public:
  MOCK_METHOD0(GetEmbeddedSearchClient,
               chrome::mojom::EmbeddedSearchClient*(void));
};

}  // namespace

class SearchIPCRouterTest : public BrowserWithTestWindowTest {
 public:
  SearchIPCRouterTest() : field_trial_list_(NULL) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("chrome://blank"));
    SearchTabHelper::CreateForWebContents(web_contents());

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.new_tab_url = "https://foo.com/newtab";
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");

    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SearchTabHelper* GetSearchTabHelper(
      content::WebContents* web_contents) {
    EXPECT_NE(static_cast<content::WebContents*>(NULL), web_contents);
    return SearchTabHelper::FromWebContents(web_contents);
  }

  void SetupMockDelegateAndPolicy() {
    content::WebContents* contents = web_contents();
    ASSERT_NE(static_cast<content::WebContents*>(NULL), contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    ASSERT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    search_tab_helper->ipc_router_for_testing().set_delegate_for_testing(
        mock_delegate());
    search_tab_helper->ipc_router_for_testing().set_policy_for_testing(
        base::WrapUnique(new MockSearchIPCRouterPolicy));
    auto factory = std::make_unique<MockEmbeddedSearchClientFactory>();
    ON_CALL(*factory, GetEmbeddedSearchClient())
        .WillByDefault(Return(&mock_embedded_search_client_));
    GetSearchIPCRouter().set_embedded_search_client_factory_for_testing(
        std::move(factory));
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

  MockSearchIPCRouterPolicy* GetSearchIPCRouterPolicy() {
    content::WebContents* contents = web_contents();
    EXPECT_NE(static_cast<content::WebContents*>(NULL), contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    EXPECT_NE(static_cast<SearchTabHelper*>(NULL), search_tab_helper);
    return static_cast<MockSearchIPCRouterPolicy*>(
        search_tab_helper->ipc_router_for_testing().policy_for_testing());
  }

  SearchIPCRouter& GetSearchIPCRouter() {
    return GetSearchTabHelper(web_contents())->ipc_router_for_testing();
  }

  int GetSearchIPCRouterSeqNo() {
    return GetSearchIPCRouter().page_seq_no_for_testing();
  }

  bool IsActiveTab(content::WebContents* contents) {
    return GetSearchTabHelper(contents)
        ->ipc_router_for_testing()
        .is_active_tab_;
  }

  MockEmbeddedSearchClient* mock_embedded_search_client() {
    return &mock_embedded_search_client_;
  }

 private:
  MockSearchIPCRouterDelegate delegate_;
  base::FieldTrialList field_trial_list_;
  MockEmbeddedSearchClient mock_embedded_search_client_;
};

TEST_F(SearchIPCRouterTest, ProcessFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(1);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab))
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().FocusOmnibox(GetSearchIPCRouterSeqNo(),
                                    OMNIBOX_FOCUS_VISIBLE);
}

TEST_F(SearchIPCRouterTest, IgnoreFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), FocusOmnibox(OMNIBOX_FOCUS_VISIBLE)).Times(0);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);
  EXPECT_CALL(*policy, ShouldProcessFocusOmnibox(is_active_tab))
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().FocusOmnibox(GetSearchIPCRouterSeqNo(),
                                    OMNIBOX_FOCUS_VISIBLE);
}

TEST_F(SearchIPCRouterTest, HandleTabChangedEvents) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  content::WebContents* contents = web_contents();
  EXPECT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));

  // Add a new tab to deactivate the current tab.
  AddTab(browser(), GURL(url::kAboutBlankURL));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(IsActiveTab(contents));

  // Activate the first tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(),
            browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));
}

TEST_F(SearchIPCRouterTest, ProcessLogEventMsg) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(123);
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_ALL_TILES_LOADED, delta))
      .Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1).WillOnce(Return(true));

  GetSearchIPCRouter().LogEvent(GetSearchIPCRouterSeqNo(), NTP_ALL_TILES_LOADED,
                                delta);
}

TEST_F(SearchIPCRouterTest, IgnoreLogEventMsg) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(123);
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogEvent(NTP_ALL_TILES_LOADED, delta))
      .Times(0);
  EXPECT_CALL(*policy, ShouldProcessLogEvent())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().LogEvent(GetSearchIPCRouterSeqNo(), NTP_ALL_TILES_LOADED,
                                delta);
}

TEST_F(SearchIPCRouterTest, ProcessLogMostVisitedImpressionMsg) {
  const ntp_tiles::NTPTileImpression impression(
      3, ntp_tiles::TileSource::SUGGESTIONS_SERVICE,
      ntp_tiles::TileTitleSource::UNKNOWN, ntp_tiles::TileVisualType::THUMBNAIL,
      favicon_base::IconType::kInvalid, base::Time(), GURL());
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogMostVisitedImpression(Field(
                                    &ntp_tiles::NTPTileImpression::index, 3)))
      .Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1).WillOnce(Return(true));

  GetSearchIPCRouter().LogMostVisitedImpression(GetSearchIPCRouterSeqNo(),
                                                impression);
}

TEST_F(SearchIPCRouterTest, ProcessLogMostVisitedNavigationMsg) {
  const ntp_tiles::NTPTileImpression impression(
      3, ntp_tiles::TileSource::SUGGESTIONS_SERVICE,
      ntp_tiles::TileTitleSource::UNKNOWN, ntp_tiles::TileVisualType::THUMBNAIL,
      favicon_base::IconType::kInvalid, base::Time(), GURL());
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnLogMostVisitedNavigation(Field(
                                    &ntp_tiles::NTPTileImpression::index, 3)))
      .Times(1);
  EXPECT_CALL(*policy, ShouldProcessLogEvent()).Times(1).WillOnce(Return(true));

  GetSearchIPCRouter().LogMostVisitedNavigation(GetSearchIPCRouterSeqNo(),
                                                impression);
}

TEST_F(SearchIPCRouterTest, ProcessChromeIdentityCheckMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  EXPECT_CALL(*mock_delegate(), ChromeIdentityCheck(test_identity))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*policy, ShouldProcessChromeIdentityCheck())
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<SearchIPCRouter::ChromeIdentityCheckCallback> callback;
  EXPECT_CALL(callback, Run(true));
  GetSearchIPCRouter().ChromeIdentityCheck(GetSearchIPCRouterSeqNo(),
                                           test_identity, callback.Get());
}

TEST_F(SearchIPCRouterTest, IgnoreChromeIdentityCheckMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  const base::string16 test_identity = base::ASCIIToUTF16("foo@bar.com");
  EXPECT_CALL(*mock_delegate(), ChromeIdentityCheck(test_identity)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessChromeIdentityCheck())
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<SearchIPCRouter::ChromeIdentityCheckCallback> callback;
  EXPECT_CALL(callback, Run(false));
  GetSearchIPCRouter().ChromeIdentityCheck(GetSearchIPCRouterSeqNo(),
                                           test_identity, callback.Get());
}

TEST_F(SearchIPCRouterTest, ProcessHistorySyncCheckMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), HistorySyncCheck())
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*policy, ShouldProcessHistorySyncCheck())
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<SearchIPCRouter::ChromeIdentityCheckCallback> callback;
  EXPECT_CALL(callback, Run(true));
  GetSearchIPCRouter().HistorySyncCheck(GetSearchIPCRouterSeqNo(),
                                        callback.Get());
}

TEST_F(SearchIPCRouterTest, IgnoreHistorySyncCheckMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  EXPECT_CALL(*mock_delegate(), HistorySyncCheck()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessHistorySyncCheck())
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<SearchIPCRouter::ChromeIdentityCheckCallback> callback;
  EXPECT_CALL(callback, Run(false));
  GetSearchIPCRouter().HistorySyncCheck(GetSearchIPCRouterSeqNo(),
                                        callback.Get());
}

TEST_F(SearchIPCRouterTest, ProcessDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().DeleteMostVisitedItem(GetSearchIPCRouterSeqNo(),
                                             item_url);
}

TEST_F(SearchIPCRouterTest, IgnoreDeleteMostVisitedItemMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteMostVisitedItem(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteMostVisitedItem())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().DeleteMostVisitedItem(GetSearchIPCRouterSeqNo(),
                                             item_url);
}

TEST_F(SearchIPCRouterTest, ProcessUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().UndoMostVisitedDeletion(GetSearchIPCRouterSeqNo(),
                                               item_url);
}

TEST_F(SearchIPCRouterTest, IgnoreUndoMostVisitedDeletionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnUndoMostVisitedDeletion(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoMostVisitedDeletion())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().UndoMostVisitedDeletion(GetSearchIPCRouterSeqNo(),
                                               item_url);
}

TEST_F(SearchIPCRouterTest, ProcessUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().UndoAllMostVisitedDeletions(GetSearchIPCRouterSeqNo());
}

TEST_F(SearchIPCRouterTest, IgnoreUndoAllMostVisitedDeletionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoAllMostVisitedDeletions()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoAllMostVisitedDeletions())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().UndoAllMostVisitedDeletions(GetSearchIPCRouterSeqNo());
}

TEST_F(SearchIPCRouterTest, ProcessAddCustomLinkMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  std::string item_title("foo");
  EXPECT_CALL(*mock_delegate(), OnAddCustomLink(item_url, item_title))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*policy, ShouldProcessAddCustomLink())
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<SearchIPCRouter::AddCustomLinkCallback> callback;
  EXPECT_CALL(callback, Run(true));
  GetSearchIPCRouter().AddCustomLink(GetSearchIPCRouterSeqNo(), item_url,
                                     item_title, callback.Get());
}

TEST_F(SearchIPCRouterTest, IgnoreAddCustomLinkMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  std::string item_title("foo");
  EXPECT_CALL(*mock_delegate(), OnAddCustomLink(item_url, item_title)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessAddCustomLink())
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<SearchIPCRouter::AddCustomLinkCallback> callback;
  EXPECT_CALL(callback, Run(false));
  GetSearchIPCRouter().AddCustomLink(GetSearchIPCRouterSeqNo(), item_url,
                                     item_title, callback.Get());
}

TEST_F(SearchIPCRouterTest, ProcessUpdateCustomLinkMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  GURL new_url("www.foo1.com");
  std::string new_title("foo");
  EXPECT_CALL(*mock_delegate(),
              OnUpdateCustomLink(item_url, new_url, new_title))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*policy, ShouldProcessUpdateCustomLink())
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<SearchIPCRouter::UpdateCustomLinkCallback> callback;
  EXPECT_CALL(callback, Run(true));
  GetSearchIPCRouter().UpdateCustomLink(GetSearchIPCRouterSeqNo(), item_url,
                                        new_url, new_title, callback.Get());
}

TEST_F(SearchIPCRouterTest, IgnoreUpdateCustomLinkMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  GURL new_url("www.foo1.com");
  std::string new_title("foo");
  EXPECT_CALL(*mock_delegate(),
              OnUpdateCustomLink(item_url, new_url, new_title))
      .Times(0);
  EXPECT_CALL(*policy, ShouldProcessUpdateCustomLink())
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<SearchIPCRouter::UpdateCustomLinkCallback> callback;
  EXPECT_CALL(callback, Run(false));
  GetSearchIPCRouter().UpdateCustomLink(GetSearchIPCRouterSeqNo(), item_url,
                                        new_url, new_title, callback.Get());
}

TEST_F(SearchIPCRouterTest, ProcessDeleteCustomLinkMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteCustomLink(item_url))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(*policy, ShouldProcessDeleteCustomLink())
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<SearchIPCRouter::DeleteCustomLinkCallback> callback;
  EXPECT_CALL(callback, Run(true));
  GetSearchIPCRouter().DeleteCustomLink(GetSearchIPCRouterSeqNo(), item_url,
                                        callback.Get());
}

TEST_F(SearchIPCRouterTest, IgnoreDeleteCustomLinkMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL item_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDeleteCustomLink(item_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDeleteCustomLink())
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<SearchIPCRouter::DeleteCustomLinkCallback> callback;
  EXPECT_CALL(callback, Run(false));
  GetSearchIPCRouter().DeleteCustomLink(GetSearchIPCRouterSeqNo(), item_url,
                                        callback.Get());
}

TEST_F(SearchIPCRouterTest, ProcessUndoCustomLinkActionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoCustomLinkAction()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessUndoCustomLinkAction())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().UndoCustomLinkAction(GetSearchIPCRouterSeqNo());
}

TEST_F(SearchIPCRouterTest, IgnoreUndoCustomLinkActionMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnUndoCustomLinkAction()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessUndoCustomLinkAction())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().UndoCustomLinkAction(GetSearchIPCRouterSeqNo());
}

TEST_F(SearchIPCRouterTest, ProcessResetCustomLinksMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnResetCustomLinks()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessResetCustomLinks())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().ResetCustomLinks(GetSearchIPCRouterSeqNo());
}

TEST_F(SearchIPCRouterTest, IgnoreResetCustomLinksMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*mock_delegate(), OnResetCustomLinks()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessResetCustomLinks())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().ResetCustomLinks(GetSearchIPCRouterSeqNo());
}

TEST_F(SearchIPCRouterTest, ProcessDoesUrlResolve) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDoesUrlResolve(url, _)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessDoesUrlResolve())
      .Times(1)
      .WillOnce(Return(true));

  base::MockCallback<SearchIPCRouter::DoesUrlResolveCallback> callback;
  EXPECT_CALL(callback, Run(_, _)).Times(0);
  GetSearchIPCRouter().DoesUrlResolve(GetSearchIPCRouterSeqNo(), url,
                                      callback.Get());
}

TEST_F(SearchIPCRouterTest, IgnoreDoesUrlResolveMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnDoesUrlResolve(url, _)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessDoesUrlResolve())
      .Times(1)
      .WillOnce(Return(false));

  base::MockCallback<SearchIPCRouter::DoesUrlResolveCallback> callback;
  EXPECT_CALL(callback, Run(true, false));
  GetSearchIPCRouter().DoesUrlResolve(GetSearchIPCRouterSeqNo(), url,
                                      callback.Get());
}

TEST_F(SearchIPCRouterTest, ProcessPasteAndOpenDropdownMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab))
      .Times(1)
      .WillOnce(Return(true));
  GetSearchIPCRouter().PasteAndOpenDropdown(GetSearchIPCRouterSeqNo(), text);
}

TEST_F(SearchIPCRouterTest, IgnorePasteAndOpenDropdownMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  base::string16 text;
  EXPECT_CALL(*mock_delegate(), PasteIntoOmnibox(text)).Times(0);

  content::WebContents* contents = web_contents();
  bool is_active_tab = IsActiveTab(contents);
  EXPECT_TRUE(is_active_tab);

  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldProcessPasteIntoOmnibox(is_active_tab))
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().PasteAndOpenDropdown(GetSearchIPCRouterSeqNo(), text);
}

TEST_F(SearchIPCRouterTest, SendOmniboxFocusChange) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendOmniboxFocusChanged())
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), FocusChanged(_, _));
  GetSearchIPCRouter().OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
}

TEST_F(SearchIPCRouterTest, DoNotSendOmniboxFocusChange) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendOmniboxFocusChanged())
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), FocusChanged(_, _)).Times(0);
  GetSearchIPCRouter().OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                           OMNIBOX_FOCUS_CHANGE_EXPLICIT);
}

TEST_F(SearchIPCRouterTest, SendSetInputInProgress) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetInputInProgress(true))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), SetInputInProgress(_));
  GetSearchIPCRouter().SetInputInProgress(true);
}

TEST_F(SearchIPCRouterTest, DoNotSendSetInputInProgress) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetInputInProgress(true))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), SetInputInProgress(_)).Times(0);
  GetSearchIPCRouter().SetInputInProgress(true);
}

TEST_F(SearchIPCRouterTest, SendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems())
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), MostVisitedChanged(_, false));
  GetSearchIPCRouter().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>(), false);
}

TEST_F(SearchIPCRouterTest, DoNotSendMostVisitedItemsMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedItems())
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), MostVisitedChanged(_, false))
      .Times(0);
  GetSearchIPCRouter().SendMostVisitedItems(
      std::vector<InstantMostVisitedItem>(), false);
}

TEST_F(SearchIPCRouterTest, SendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo())
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), ThemeChanged(_));
  GetSearchIPCRouter().SendThemeBackgroundInfo(ThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterTest, DoNotSendThemeBackgroundInfoMsg) {
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendThemeBackgroundInfo())
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), ThemeChanged(_)).Times(0);
  GetSearchIPCRouter().SendThemeBackgroundInfo(ThemeBackgroundInfo());
}

TEST_F(SearchIPCRouterTest, ProcessSetCustomBackgroundURLMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL bg_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnSetCustomBackgroundURL(bg_url)).Times(1);
  EXPECT_CALL(*policy, ShouldProcessSetCustomBackgroundURL())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().SetCustomBackgroundURL(bg_url);
}

TEST_F(SearchIPCRouterTest, IgnoreSetCustomBackgroundURLMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL bg_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnSetCustomBackgroundURL(bg_url)).Times(0);
  EXPECT_CALL(*policy, ShouldProcessSetCustomBackgroundURL())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().SetCustomBackgroundURL(bg_url);
}

TEST_F(SearchIPCRouterTest, ProcessSetCustomBackgroundURLWithAttributionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL bg_url("www.foo.com");
  std::string attr1("foo");
  std::string attr2("bar");
  GURL action_url("www.bar.com");
  EXPECT_CALL(*mock_delegate(), OnSetCustomBackgroundURLWithAttributions(
                                    bg_url, attr1, attr2, action_url))
      .Times(1);
  EXPECT_CALL(*policy, ShouldProcessSetCustomBackgroundURLWithAttributions())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().SetCustomBackgroundURLWithAttributions(
      bg_url, attr1, attr2, action_url);
}

TEST_F(SearchIPCRouterTest, IgnoreSetCustomBackgroundURLWithAttributionsMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL bg_url("www.foo.com");
  std::string attr1("foo");
  std::string attr2("bar");
  GURL action_url("www.bar.com");
  EXPECT_CALL(*mock_delegate(), OnSetCustomBackgroundURLWithAttributions(
                                    bg_url, attr1, attr2, action_url))
      .Times(0);
  EXPECT_CALL(*policy, ShouldProcessSetCustomBackgroundURLWithAttributions())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().SetCustomBackgroundURLWithAttributions(
      bg_url, attr1, attr2, action_url);
}

TEST_F(SearchIPCRouterTest, ProcessSelectLocalBackgroundImageMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL bg_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnSelectLocalBackgroundImage()).Times(1);
  EXPECT_CALL(*policy, ShouldProcessSelectLocalBackgroundImage())
      .Times(1)
      .WillOnce(Return(true));

  GetSearchIPCRouter().SelectLocalBackgroundImage();
}

TEST_F(SearchIPCRouterTest, IgnoreSelectLocalBackgroundImageMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/bar"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  GURL bg_url("www.foo.com");
  EXPECT_CALL(*mock_delegate(), OnSelectLocalBackgroundImage()).Times(0);
  EXPECT_CALL(*policy, ShouldProcessSelectLocalBackgroundImage())
      .Times(1)
      .WillOnce(Return(false));

  GetSearchIPCRouter().SelectLocalBackgroundImage();
}
