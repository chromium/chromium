// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/read_later/read_later_test_utils.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/reading_list/core/reading_list_model.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

constexpr char kTabUrl1[] = "http://foo/1";
constexpr char kTabUrl2[] = "http://foo/2";
constexpr char kTabUrl3[] = "http://foo/3";
constexpr char kTabUrl4[] = "http://foo/4";

constexpr char kTabName1[] = "Tab 1";
constexpr char kTabName2[] = "Tab 2";
constexpr char kTabName3[] = "Tab 3";
constexpr char kTabName4[] = "Tab 4";

class MockPage : public read_later::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<read_later::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<read_later::mojom::Page> receiver_{this};

  MOCK_METHOD1(ItemsChanged,
               void(read_later::mojom::ReadLaterEntriesByStatusPtr));
};

void ExpectNewReadLaterEntry(const read_later::mojom::ReadLaterEntry* entry,
                             const GURL& url,
                             const std::string& title) {
  EXPECT_EQ(title, entry->title);
  EXPECT_EQ(url.spec(), entry->url.spec());
}

class TestReadLaterPageHandler : public ReadLaterPageHandler {
 public:
  explicit TestReadLaterPageHandler(
      mojo::PendingRemote<read_later::mojom::Page> page,
      content::WebUI* test_web_ui)
      : ReadLaterPageHandler(
            mojo::PendingReceiver<read_later::mojom::PageHandler>(),
            std::move(page),
            nullptr,
            test_web_ui) {}
};

class TestReadLaterPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    BrowserList::SetLastActive(browser());

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents_.get());

    handler_ = std::make_unique<TestReadLaterPageHandler>(
        page_.BindAndGetRemote(), test_web_ui_.get());
    model_ =
        ReadingListModelFactory::GetForBrowserContext(browser()->profile());
    test::ReadingListLoadObserver(model_).Wait();

    AddTabWithTitle(browser(), GURL(kTabUrl1), kTabName1);
    AddTabWithTitle(browser(), GURL(kTabUrl2), kTabName2);
    AddTabWithTitle(browser(), GURL(kTabUrl3), kTabName3);
    AddTabWithTitle(browser(), GURL(kTabUrl4), kTabName4);

    model()->AddEntry(GURL(kTabUrl1), kTabName1,
                      reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
    model()->AddEntry(GURL(kTabUrl3), kTabName3,
                      reading_list::EntrySource::ADDED_VIA_CURRENT_APP);
  }

  void TearDown() override {
    handler_.reset();
    test_web_ui_.reset();
    web_contents_.reset();
    browser()->tab_strip_model()->CloseAllTabs();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{ReadingListModelFactory::GetInstance(),
             ReadingListModelFactory::GetDefaultFactoryForTesting()}};
  }

  ReadingListModel* model() { return model_; }
  TestReadLaterPageHandler* handler() { return handler_.get(); }

 protected:
  void AddTabWithTitle(Browser* browser,
                       const GURL url,
                       const std::string title) {
    AddTab(browser, url);
    NavigateAndCommitActiveTabWithTitle(browser, url,
                                        base::ASCIIToUTF16(title));
  }

  testing::StrictMock<MockPage> page_;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<TestReadLaterPageHandler> handler_;
  ReadingListModel* model_;
};

TEST_F(TestReadLaterPageHandlerTest, GetReadLaterEntries) {
  // Get Read later entries.
  read_later::mojom::PageHandler::GetReadLaterEntriesCallback callback1 =
      base::BindLambdaForTesting(
          [&](read_later::mojom::ReadLaterEntriesByStatusPtr
                  entries_by_status) {
            ASSERT_EQ(2u, entries_by_status->unread_entries.size());
            ASSERT_EQ(0u, entries_by_status->read_entries.size());

            // Verify the entries appear in order of last added to first.
            auto* entry1 = entries_by_status->unread_entries[0].get();
            ExpectNewReadLaterEntry(entry1, GURL(kTabUrl3), kTabName3);

            auto* entry2 = entries_by_status->unread_entries[1].get();
            ExpectNewReadLaterEntry(entry2, GURL(kTabUrl1), kTabName1);
          });

  handler()->GetReadLaterEntries(std::move(callback1));
}

TEST_F(TestReadLaterPageHandlerTest, OpenSavedEntryOnNTP) {
  // Open and navigate to NTP.
  AddTabWithTitle(browser(), GURL(chrome::kChromeUINewTabURL), "NTP");

  // Check that OpenSavedEntry from the NTP does not open a new tab.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);
  handler()->OpenSavedEntry(GURL(kTabUrl3));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);

  // Get Read later entries.
  read_later::mojom::PageHandler::GetReadLaterEntriesCallback callback1 =
      base::BindLambdaForTesting(
          [&](read_later::mojom::ReadLaterEntriesByStatusPtr
                  entries_by_status) {
            ASSERT_EQ(1u, entries_by_status->unread_entries.size());
            ASSERT_EQ(1u, entries_by_status->read_entries.size());

            auto* entry1 = entries_by_status->unread_entries[0].get();
            ExpectNewReadLaterEntry(entry1, GURL(kTabUrl1), kTabName1);

            auto* entry2 = entries_by_status->read_entries[0].get();
            ExpectNewReadLaterEntry(entry2, GURL(kTabUrl3), kTabName3);
          });

  handler()->GetReadLaterEntries(std::move(callback1));
}

TEST_F(TestReadLaterPageHandlerTest, OpenSavedEntryNotOnNTP) {
  // Check that OpenSavedEntry opens a new tab when not on the NTP.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 4);
  handler()->OpenSavedEntry(GURL(kTabUrl3));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 5);

  // Get Read later entries.
  read_later::mojom::PageHandler::GetReadLaterEntriesCallback callback1 =
      base::BindLambdaForTesting(
          [&](read_later::mojom::ReadLaterEntriesByStatusPtr
                  entries_by_status) {
            ASSERT_EQ(1u, entries_by_status->unread_entries.size());
            ASSERT_EQ(1u, entries_by_status->read_entries.size());

            auto* entry1 = entries_by_status->unread_entries[0].get();
            ExpectNewReadLaterEntry(entry1, GURL(kTabUrl1), kTabName1);

            auto* entry2 = entries_by_status->read_entries[0].get();
            ExpectNewReadLaterEntry(entry2, GURL(kTabUrl3), kTabName3);
          });

  handler()->GetReadLaterEntries(std::move(callback1));
}

TEST_F(TestReadLaterPageHandlerTest, UpdateReadStatus) {
  handler()->UpdateReadStatus(GURL(kTabUrl3), true);
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(1);

  // Get Read later entries.
  read_later::mojom::PageHandler::GetReadLaterEntriesCallback callback1 =
      base::BindLambdaForTesting(
          [&](read_later::mojom::ReadLaterEntriesByStatusPtr
                  entries_by_status) {
            ASSERT_EQ(1u, entries_by_status->unread_entries.size());
            ASSERT_EQ(1u, entries_by_status->read_entries.size());

            auto* entry1 = entries_by_status->unread_entries[0].get();
            ExpectNewReadLaterEntry(entry1, GURL(kTabUrl1), kTabName1);

            auto* entry2 = entries_by_status->read_entries[0].get();
            ExpectNewReadLaterEntry(entry2, GURL(kTabUrl3), kTabName3);
          });

  handler()->GetReadLaterEntries(std::move(callback1));
}

TEST_F(TestReadLaterPageHandlerTest, RemoveEntry) {
  handler()->RemoveEntry(GURL(kTabUrl3));
  EXPECT_CALL(page_, ItemsChanged(testing::_)).Times(1);

  // Get Read later entries.
  read_later::mojom::PageHandler::GetReadLaterEntriesCallback callback1 =
      base::BindLambdaForTesting(
          [&](read_later::mojom::ReadLaterEntriesByStatusPtr
                  entries_by_status) {
            ASSERT_EQ(1u, entries_by_status->unread_entries.size());
            ASSERT_EQ(0u, entries_by_status->read_entries.size());

            auto* entry1 = entries_by_status->unread_entries[0].get();
            ExpectNewReadLaterEntry(entry1, GURL(kTabUrl1), kTabName1);
          });

  handler()->GetReadLaterEntries(std::move(callback1));
}

}  // namespace
