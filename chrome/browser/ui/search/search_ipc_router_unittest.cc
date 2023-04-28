// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ipc_router.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
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
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/mock_embedded_search_client.h"
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
using testing::NiceMock;
using testing::Return;

namespace {

class MockSearchIPCRouterDelegate : public SearchIPCRouter::Delegate {
 public:
  virtual ~MockSearchIPCRouterDelegate() {}

  MOCK_METHOD(void, FocusOmnibox, (bool focus));
  MOCK_METHOD(void, OnDeleteMostVisitedItem, (const GURL& url));
  MOCK_METHOD(void, OnUndoMostVisitedDeletion, (const GURL& url));
  MOCK_METHOD(void, OnUndoAllMostVisitedDeletions, ());
  MOCK_METHOD(void, OnSetCustomBackgroundURL, (const GURL& url));
};

class MockSearchIPCRouterPolicy : public SearchIPCRouter::Policy {
 public:
  ~MockSearchIPCRouterPolicy() override {}

  MOCK_METHOD(bool, ShouldProcessFocusOmnibox, (bool));
  MOCK_METHOD(bool, ShouldProcessDeleteMostVisitedItem, ());
  MOCK_METHOD(bool, ShouldProcessUndoMostVisitedDeletion, ());
  MOCK_METHOD(bool, ShouldProcessUndoAllMostVisitedDeletions, ());
  MOCK_METHOD(bool, ShouldSendSetInputInProgress, (bool));
  MOCK_METHOD(bool, ShouldSendOmniboxFocusChanged, ());
  MOCK_METHOD(bool, ShouldSendMostVisitedInfo, ());
  MOCK_METHOD(bool, ShouldSendNtpTheme, ());
  MOCK_METHOD(bool, ShouldProcessThemeChangeMessages, ());
};

class MockEmbeddedSearchClientFactory
    : public SearchIPCRouter::EmbeddedSearchClientFactory {
 public:
  MOCK_METHOD(search::mojom::EmbeddedSearchClient*,
              GetEmbeddedSearchClient,
              ());

  MOCK_METHOD(void,
              BindFactoryReceiver,
              (mojo::PendingAssociatedReceiver<
                   search::mojom::EmbeddedSearchConnector> receiver,
               content::RenderFrameHost* rfh),
              (override));
};

}  // namespace

class SearchIPCRouterTest : public BrowserWithTestWindowTest {
 public:
  SearchIPCRouterTest() {}

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
    data.SetShortName(u"foo.com");
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
    EXPECT_NE(nullptr, web_contents);
    return SearchTabHelper::FromWebContents(web_contents);
  }

  void SetupMockDelegateAndPolicy() {
    content::WebContents* contents = web_contents();
    ASSERT_NE(nullptr, contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    ASSERT_NE(nullptr, search_tab_helper);
    search_tab_helper->ipc_router_for_testing().set_delegate_for_testing(
        mock_delegate());
    search_tab_helper->ipc_router_for_testing().set_policy_for_testing(
        base::WrapUnique(new MockSearchIPCRouterPolicy));
    auto factory =
        std::make_unique<NiceMock<MockEmbeddedSearchClientFactory>>();
    ON_CALL(*factory, GetEmbeddedSearchClient())
        .WillByDefault(Return(&mock_embedded_search_client_));
    GetSearchIPCRouter().set_embedded_search_client_factory_for_testing(
        std::move(factory));
  }

  MockSearchIPCRouterDelegate* mock_delegate() { return &delegate_; }

  MockSearchIPCRouterPolicy* GetSearchIPCRouterPolicy() {
    content::WebContents* contents = web_contents();
    EXPECT_NE(nullptr, contents);
    SearchTabHelper* search_tab_helper = GetSearchTabHelper(contents);
    EXPECT_NE(nullptr, search_tab_helper);
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
  MockEmbeddedSearchClient mock_embedded_search_client_;
};

// TODO(aee): ProcessFocusOmniboxMsg and IgnoreFocusOmniboxMsg both pass with
// unknown URLs. I'm not sure this is testing anything.
TEST_F(SearchIPCRouterTest, ProcessFocusOmniboxMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
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
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(),
            browser()->tab_strip_model()->GetIndexOfWebContents(contents));
  EXPECT_TRUE(IsActiveTab(contents));
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

TEST_F(SearchIPCRouterTest, SendOmniboxFocusChange) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
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
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
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
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetInputInProgress(true))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), SetInputInProgress(_));
  GetSearchIPCRouter().SetInputInProgress(true);
}

TEST_F(SearchIPCRouterTest, DoNotSendSetInputInProgress) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendSetInputInProgress(true))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), SetInputInProgress(_)).Times(0);
  GetSearchIPCRouter().SetInputInProgress(true);
}

TEST_F(SearchIPCRouterTest, SendMostVisitedInfoMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedInfo())
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), MostVisitedInfoChanged(_));
  GetSearchIPCRouter().SendMostVisitedInfo(InstantMostVisitedInfo());
}

TEST_F(SearchIPCRouterTest, DoNotSendMostVisitedInfoMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendMostVisitedInfo())
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), MostVisitedInfoChanged(_))
      .Times(0);
  GetSearchIPCRouter().SendMostVisitedInfo(InstantMostVisitedInfo());
}

TEST_F(SearchIPCRouterTest, SendNtpThemeMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendNtpTheme()).Times(1).WillOnce(Return(true));

  EXPECT_CALL(*mock_embedded_search_client(), ThemeChanged(_));
  GetSearchIPCRouter().SendNtpTheme(NtpTheme());
}

TEST_F(SearchIPCRouterTest, DoNotSendNtpThemeMsg) {
  NavigateAndCommitActiveTab(GURL("chrome-search://foo/baz"));
  SetupMockDelegateAndPolicy();
  MockSearchIPCRouterPolicy* policy = GetSearchIPCRouterPolicy();
  EXPECT_CALL(*policy, ShouldSendNtpTheme()).Times(1).WillOnce(Return(false));

  EXPECT_CALL(*mock_embedded_search_client(), ThemeChanged(_)).Times(0);
  GetSearchIPCRouter().SendNtpTheme(NtpTheme());
}
