// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_page_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace {

constexpr char kTabUrl1[] = "http://foo/1";
constexpr char kTabUrl3[] = "http://foo/3";

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName3[] = "Tab 3";

bool IsItemEnabledInMenu(ui::MenuModel* menu, int command_id) {
  ui::MenuModel* model = menu;
  size_t index = 0;
  return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                     &index) &&
         menu->IsEnabledAt(index);
}

class MockPage : public reading_list::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<reading_list::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<reading_list::mojom::Page> receiver_{this};

  MOCK_METHOD(void,
              ItemsChanged,
              (reading_list::mojom::ReadLaterEntriesByStatusPtr));
  MOCK_METHOD(void,
              CurrentPageActionButtonStateChanged,
              (reading_list::mojom::CurrentPageActionButtonState));
};

void ExpectNewReadLaterEntry(const reading_list::mojom::ReadLaterEntry* entry,
                             const GURL& url,
                             const std::string& title) {
  EXPECT_EQ(title, entry->title);
  EXPECT_EQ(url.spec(), entry->url.spec());
}

class TestReadingListPageHandler : public ReadingListPageHandler {
 public:
  explicit TestReadingListPageHandler(
      mojo::PendingRemote<reading_list::mojom::Page> page,
      content::WebUI* test_web_ui)
      : ReadingListPageHandler(
            mojo::PendingReceiver<reading_list::mojom::PageHandler>(),
            std::move(page),
            nullptr,
            test_web_ui) {}
};

class TestReadingListPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    incognito_profile_ =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(mock_incognito_browser_window_, GetProfile())
        .WillByDefault(testing::Return(incognito_profile_));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    webui::SetBrowserWindowInterface(web_contents_.get(),
                                     &mock_browser_window_);
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_.get());

    handler_ = std::make_unique<TestReadingListPageHandler>(
        page_.BindAndGetRemote(), test_web_ui_.get());
    model_ = ReadingListModelFactory::GetForBrowserContext(profile());
    ReadingListLoadObserver(model_).Wait();
    handler_->SetActiveTabURL(GURL("http://www.google.com"));

    model()->AddOrReplaceEntry(GURL(kTabUrl1), kTabName1,
                               reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
                               /*estimated_read_time=*/std::nullopt,
                               /*creation_time=*/std::nullopt);
    model()->AddOrReplaceEntry(GURL(kTabUrl3), kTabName3,
                               reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
                               /*estimated_read_time=*/std::nullopt,
                               /*creation_time=*/std::nullopt);
  }

  void TearDown() override {
    webui::SetBrowserWindowInterface(web_contents_.get(), nullptr);
    handler_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    incognito_profile_ = nullptr;
    model_ = nullptr;
    testing::Mock::VerifyAndClear(&mock_browser_window_);
    testing::Mock::VerifyAndClear(&mock_incognito_browser_window_);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        ReadingListModelFactory::GetInstance(),
        ReadingListModelFactory::GetDefaultFactoryForTesting()}};
  }

  MockBrowserWindowInterface* mock_browser_window() {
    return &mock_browser_window_;
  }
  MockBrowserWindowInterface* mock_incognito_browser_window() {
    return &mock_incognito_browser_window_;
  }
  ReadingListModel* model() { return model_; }
  TestReadingListPageHandler* handler() { return handler_.get(); }

 protected:
  void GetAndVerifyReadLaterEntries(
      size_t unread_size,
      size_t read_size,
      const std::vector<std::pair<GURL, std::string>>& expected_unread_data,
      const std::vector<std::pair<GURL, std::string>>& expected_read_data) {
    EXPECT_EQ(unread_size, expected_unread_data.size());
    reading_list::mojom::PageHandler::GetReadLaterEntriesCallback callback =
        base::BindLambdaForTesting(
            [&](reading_list::mojom::ReadLaterEntriesByStatusPtr
                    entries_by_status) {
              ASSERT_EQ(unread_size, entries_by_status->unread_entries.size());
              ASSERT_EQ(read_size, entries_by_status->read_entries.size());

              // Verify the entries appear in order of last added to first.
              for (size_t i = 0u; i < expected_unread_data.size(); i++) {
                auto* entry = entries_by_status->unread_entries[i].get();
                ExpectNewReadLaterEntry(entry, expected_unread_data[i].first,
                                        expected_unread_data[i].second);
              }

              // Verify the entries appear in order of last added to first.
              for (size_t i = 0u; i < expected_read_data.size(); i++) {
                auto* entry = entries_by_status->read_entries[i].get();
                ExpectNewReadLaterEntry(entry, expected_read_data[i].first,
                                        expected_read_data[i].second);
              }
            });
    handler()->GetReadLaterEntries(std::move(callback));
  }

  ui::mojom::ClickModifiersPtr GetClickModifiers() {
    ui::mojom::ClickModifiersPtr info = ui::mojom::ClickModifiers::New();
    info->middle_button = false;
    info->alt_key = false;
    info->ctrl_key = false;
    info->meta_key = false;
    info->shift_key = false;
    return info;
  }

  testing::StrictMock<MockPage> page_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  testing::NiceMock<MockBrowserWindowInterface> mock_incognito_browser_window_;
  raw_ptr<Profile> incognito_profile_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<TestReadingListPageHandler> handler_;
  raw_ptr<ReadingListModel> model_ = nullptr;
};

TEST_F(TestReadingListPageHandlerTest, GetReadLaterEntries) {
  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);
  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 2u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3),
       std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadingListPageHandlerTest, OpenURLOnNTP) {
  handler()->SetActiveTabURL(GURL("chrome://newtab/"));
  EXPECT_CALL(
      *mock_browser_window(),
      OpenURL(testing::AllOf(
                  testing::Field(&content::OpenURLParams::url, GURL(kTabUrl3)),
                  testing::Field(&content::OpenURLParams::disposition,
                                 WindowOpenDisposition::CURRENT_TAB)),
              testing::_))
      .Times(1);

  handler()->OpenURL(GURL(kTabUrl3), GetClickModifiers());
  handler()->UpdateReadStatus(GURL(kTabUrl3), true);

  // Expect ItemsChanged to be called 5 times.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called twice.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(2);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 1u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3)});
}

TEST_F(TestReadingListPageHandlerTest, OpenURLNotOnNTP) {
  handler()->SetActiveTabURL(GURL("http://www.google.com"));
  EXPECT_CALL(
      *mock_browser_window(),
      OpenURL(testing::AllOf(
                  testing::Field(&content::OpenURLParams::url, GURL(kTabUrl3)),
                  testing::Field(&content::OpenURLParams::disposition,
                                 WindowOpenDisposition::CURRENT_TAB)),
              testing::_))
      .Times(1);

  handler()->OpenURL(GURL(kTabUrl3), GetClickModifiers());
  handler()->UpdateReadStatus(GURL(kTabUrl3), true);

  // Expect ItemsChanged to be called 5 times.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 1u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3)});
}

TEST_F(TestReadingListPageHandlerTest, OpenURLNotInReadingList) {
  const GURL not_in_reading_list_url("http://not-in-reading-list.com");
  EXPECT_FALSE(model()->GetEntryByURL(not_in_reading_list_url));

  base::UserActionTester user_action_tester;

  EXPECT_CALL(*mock_browser_window(), OpenURL(testing::_, testing::_)).Times(0);

  // Try to open a URL that is not in the reading list.
  handler()->OpenURL(not_in_reading_list_url, GetClickModifiers());

  // Try to open with middle click (which would open a new tab if URL were in
  // reading list).
  auto click_modifiers = GetClickModifiers();
  click_modifiers->middle_button = true;
  handler()->OpenURL(not_in_reading_list_url, std::move(click_modifiers));

  // Verify that no navigation metrics were recorded.
  EXPECT_EQ(
      user_action_tester.GetActionCount("SidePanel.ReadingList.Navigation"), 0);

  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp().
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 2u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3),
       std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadingListPageHandlerTest, UpdateReadStatus) {
  handler()->UpdateReadStatus(GURL(kTabUrl3), true);

  // Expect ItemsChanged to be called 5 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the OpenURL call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 1u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {std::make_pair(GURL(kTabUrl3), kTabName3)});
}

TEST_F(TestReadingListPageHandlerTest, RemoveEntry) {
  handler()->RemoveEntry(GURL(kTabUrl3));

  // Expect ItemsChanged to be called 5 times.
  // Four for the two AddEntry calls in SetUp().
  // Once for the RemoveEntry call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadingListPageHandlerTest, UpdateAndRemoveEntry) {
  EXPECT_FALSE(model()->IsPerformingBatchUpdates());
  handler()->OpenURL(GURL(kTabUrl3), GetClickModifiers());
  handler()->RemoveEntry(GURL(kTabUrl3));
  EXPECT_FALSE(model()->IsPerformingBatchUpdates());

  // Expect ItemsChanged to be called 6 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the RemoveEntry call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadingListPageHandlerTest, PostBatchUpdate) {
  auto token = model()->BeginBatchUpdates();
  EXPECT_TRUE(model()->IsPerformingBatchUpdates());
  handler()->OpenURL(GURL(kTabUrl3), GetClickModifiers());
  handler()->RemoveEntry(GURL(kTabUrl3));
  token.reset();
  EXPECT_FALSE(model()->IsPerformingBatchUpdates());

  // Expect ItemsChanged to be called 5 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for the two updates above performed during a batch update.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(5);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadingListPageHandlerTest, NoUpdateWhenHidden) {
  // Set WebContents to be hidden.
  content::WebContents::CreateParams params =
      content::WebContents::CreateParams(profile());
  params.initially_hidden = true;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(params);
  webui::SetBrowserWindowInterface(web_contents.get(), mock_browser_window());
  handler()->set_web_contents_for_testing(web_contents.get());

  handler()->OpenURL(GURL(kTabUrl3), GetClickModifiers());
  handler()->RemoveEntry(GURL(kTabUrl3));

  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() and the two above calls to not trigger an ItemsChanged call because
  // the WebContents is not visible.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);

  // Get Read later entries. Calling GetReadLaterEntries will trigger an update.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 1u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});

  // Unbind mock_browser_window from web_contents and restore handler's
  // WebContents back to web_contents_ to unobserve web_contents before it is
  // destroyed at function end.
  webui::SetBrowserWindowInterface(web_contents.get(), nullptr);
  handler()->set_web_contents_for_testing(web_contents_.get());
}

TEST_F(TestReadingListPageHandlerTest, OpenURLAndReadd) {
  EXPECT_CALL(
      *mock_browser_window(),
      OpenURL(testing::Field(&content::OpenURLParams::url, GURL(kTabUrl3)),
              testing::_))
      .Times(1);

  handler()->OpenURL(GURL(kTabUrl3), GetClickModifiers());
  handler()->UpdateReadStatus(GURL(kTabUrl3), true);
  handler()->SetActiveTabURL(GURL(kTabUrl3));

  // Expect CurrentPageActionButtonState to be add, due to the current
  // tab entry being marked as read on open.
  EXPECT_EQ(handler()->GetCurrentPageActionButtonStateForTesting(),
            reading_list::mojom::CurrentPageActionButtonState::kAdd);
  model()->AddOrReplaceEntry(GURL(kTabUrl3), kTabName3,
                             reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/std::nullopt,
                             /*creation_time=*/std::nullopt);

  // Expect CurrentPageActionButtonState to be mark as read, due to the current
  // tab being unread on the reading list.
  EXPECT_EQ(handler()->GetCurrentPageActionButtonStateForTesting(),
            reading_list::mojom::CurrentPageActionButtonState::kMarkAsRead);
  // Expect ItemsChanged to be called 7 times.
  // Four times for the two AddEntry calls in SetUp().
  // Once for UpdateReadStatus.
  // Twice for the AddEntry call above.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(7);
  // Expect CurrentPageActionButtonStateChanged to be called twice.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(2);

  // Get Read later entries.
  GetAndVerifyReadLaterEntries(
      /* unread_size= */ 2u, /* read_size= */ 0u,
      /* expected_unread_data= */
      {std::make_pair(GURL(kTabUrl3), kTabName3),
       std::make_pair(GURL(kTabUrl1), kTabName1)},
      /* expected_read_data= */ {});
}

TEST_F(TestReadingListPageHandlerTest,
       CurrentPageActionButtonStateChangedOnActiveTabChange) {
  handler()->SetActiveTabURL(GURL("http://google.com"));
  EXPECT_EQ(handler()->GetCurrentPageActionButtonStateForTesting(),
            reading_list::mojom::CurrentPageActionButtonState::kAdd);
  handler()->SetActiveTabURL(GURL("google.com"));
  EXPECT_EQ(handler()->GetCurrentPageActionButtonStateForTesting(),
            reading_list::mojom::CurrentPageActionButtonState::kDisabled);
  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called twice.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(2);
}

TEST_F(TestReadingListPageHandlerTest, OpenInIncognitoEnabledWhenNotInOTRMode) {
  std::unique_ptr<ui::SimpleMenuModel> read_later_context_menu =
      handler()->GetItemContextMenuModelForTesting(mock_browser_window(),
                                                   model(), GURL(kTabUrl1));

  // "Open Link In New Incognito Window" command option should be enabled when
  // not in OTR mode.
  EXPECT_TRUE(IsItemEnabledInMenu(read_later_context_menu.get(),
                                  IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));

  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);
}

TEST_F(TestReadingListPageHandlerTest, OpenInIncognitoDisabledWhenInOTRMode) {
  std::unique_ptr<ui::SimpleMenuModel> otr_read_later_context_menu =
      handler()->GetItemContextMenuModelForTesting(
          mock_incognito_browser_window(), model(), GURL(kTabUrl1));

  // "Open Link In New Incognito Window" command option should be disabled
  // when in OTR mode.
  EXPECT_FALSE(IsItemEnabledInMenu(otr_read_later_context_menu.get(),
                                   IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));

  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);
}

TEST_F(TestReadingListPageHandlerTest,
       OpenInIncognitoRespectsIncognitoModePolicy) {
  // Disable incognito mode, the menu option should be disabled.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  std::unique_ptr<ui::SimpleMenuModel> read_later_context_menu =
      handler()->GetItemContextMenuModelForTesting(mock_browser_window(),
                                                   model(), GURL(kTabUrl1));
  EXPECT_FALSE(IsItemEnabledInMenu(read_later_context_menu.get(),
                                   IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));

  // Enable incognito mode, the menu option should appear as expected.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kEnabled);
  read_later_context_menu = handler()->GetItemContextMenuModelForTesting(
      mock_browser_window(), model(), GURL(kTabUrl1));
  EXPECT_TRUE(IsItemEnabledInMenu(read_later_context_menu.get(),
                                  IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD));

  // Expect ItemsChanged to be called four times from the two AddEntry calls in
  // SetUp() each AddEntry call while the reading list is open triggers items to
  // be marked as read which triggers an ItemsChanged call.
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(4);
  // Expect CurrentPageActionButtonStateChanged to be called once.
  EXPECT_CALL(page_, CurrentPageActionButtonStateChanged(testing::_)).Times(1);
}

}  // namespace
