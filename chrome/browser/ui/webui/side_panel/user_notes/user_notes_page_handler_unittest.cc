// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes.mojom-test-utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/user_notes/user_notes_features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class TestUserNotesPageHandler : public UserNotesPageHandler {
 public:
  explicit TestUserNotesPageHandler(
      mojo::PendingRemote<side_panel::mojom::UserNotesPage> page,
      Profile* profile,
      Browser* browser)
      : UserNotesPageHandler(
            mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler>(),
            std::move(page),
            profile,
            browser,
            false,
            nullptr) {}
};

class MockUserNotesPage : public side_panel::mojom::UserNotesPage {
 public:
  MockUserNotesPage() = default;
  ~MockUserNotesPage() override = default;

  mojo::PendingRemote<side_panel::mojom::UserNotesPage> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<side_panel::mojom::UserNotesPage> receiver_{this};

  MOCK_METHOD0(NotesChanged, void());
  MOCK_METHOD1(CurrentTabUrlChanged, void(bool));
  MOCK_METHOD1(SortByNewestPrefChanged, void(bool));
  MOCK_METHOD0(StartNoteCreation, void());
};

struct Note {
  GURL url;
  std::string text;
};

class UserNotesPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    features_.InitWithFeatures(
        {user_notes::kUserNotes, power_bookmarks::kPowerBookmarkBackend}, {});
    BrowserWithTestWindowTest::SetUp();
    BookmarkModelFactory::GetInstance()->SetTestingFactory(
        profile(), BookmarkModelFactory::GetDefaultFactory());
    ASSERT_TRUE(bookmark_model_ =
                    BookmarkModelFactory::GetForBrowserContext(profile()));
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    handler_ = std::make_unique<TestUserNotesPageHandler>(
        page_.BindAndGetRemote(), profile(), browser());
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestUserNotesPageHandler* handler() { return handler_.get(); }

 protected:
  MockUserNotesPage page_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

 private:
  std::unique_ptr<TestUserNotesPageHandler> handler_;
  base::test::ScopedFeatureList features_;
};

TEST_F(UserNotesPageHandlerTest, GetNotes) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note11"));
  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(2u, notes.size());

  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url2"));
  ASSERT_TRUE(waiter.NewNoteFinished("note2"));
  auto notes2 = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes2.size());
  ASSERT_EQ("note2", notes2[0]->text);
}

TEST_F(UserNotesPageHandlerTest, GetNoteOverviews) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url2"));
  ASSERT_TRUE(waiter.NewNoteFinished("note2"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url3"));
  ASSERT_TRUE(waiter.NewNoteFinished("note3"));
  auto note_overviews = waiter.GetNoteOverviews("");
  ASSERT_EQ(3u, note_overviews.size());
}

TEST_F(UserNotesPageHandlerTest, GetNoteOverviewIsCurrentTab) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));

  // Note overview is for current tab.
  auto note_overviews = waiter.GetNoteOverviews("");
  ASSERT_EQ(1u, note_overviews.size());
  ASSERT_TRUE(note_overviews[0]->is_current_tab);

  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url2"));

  // Note overview is not for current tab.
  note_overviews = waiter.GetNoteOverviews("");
  ASSERT_EQ(1u, note_overviews.size());
  ASSERT_FALSE(note_overviews[0]->is_current_tab);
}

TEST_F(UserNotesPageHandlerTest, GetNoteOverviewWithBookmarkTitle) {
  // Add a new bookmark with url.
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  const bookmarks::BookmarkNode* bb_node = bookmark_model_->bookmark_bar_node();
  GURL url("https://google.com");
  bookmark_model_->AddNewURL(bb_node, 0, u"title", url);

  // Make sure the note overview title is read from the bookmark.
  handler()->SetCurrentTabUrlForTesting(url);
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));
  auto note_overviews = waiter.GetNoteOverviews("");
  ASSERT_EQ(1u, note_overviews.size());
  ASSERT_EQ("title", note_overviews[0]->title);
}

TEST_F(UserNotesPageHandlerTest, GetNoteOverviewsReturnMatchedText) {
  // Searching notes should match case insensitive.
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("Note1"));
  auto note_overviews = waiter.GetNoteOverviews("");
  ASSERT_EQ(1u, note_overviews.size());
  ASSERT_EQ("Note1", note_overviews[0]->text);
}

TEST_F(UserNotesPageHandlerTest, FindNoteOverviewsSearchOnlyText) {
  // Searching notes should only match texts, not URL.
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://new_url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://new_url2"));
  ASSERT_TRUE(waiter.NewNoteFinished("note2"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://new_url3"));
  ASSERT_TRUE(waiter.NewNoteFinished("3"));
  auto note_overviews = waiter.GetNoteOverviews("n");
  ASSERT_EQ(2u, note_overviews.size());
}

TEST_F(UserNotesPageHandlerTest, FindNoteOverviewsCaseInsensitive) {
  // Searching notes should match case insensitive.
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("Note1"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url2"));
  ASSERT_TRUE(waiter.NewNoteFinished("Note2"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url3"));
  ASSERT_TRUE(waiter.NewNoteFinished("3"));
  auto note_overviews = waiter.GetNoteOverviews("n");
  ASSERT_EQ(2u, note_overviews.size());
}

TEST_F(UserNotesPageHandlerTest, FindNoteOverviewsReturnMatchedText) {
  // Searching notes should match case insensitive.
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("Note1"));
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url2"));
  ASSERT_TRUE(waiter.NewNoteFinished("foo"));
  auto note_overviews = waiter.GetNoteOverviews("Note");
  ASSERT_EQ(1u, note_overviews.size());
  ASSERT_EQ("Note1", note_overviews[0]->text);
}

TEST_F(UserNotesPageHandlerTest, CreateAndDeleteNote) {
  EXPECT_CALL(page_, NotesChanged()).Times(2);
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));

  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes.size());
  std::string guid = notes[0]->guid;

  ASSERT_TRUE(waiter.DeleteNote(guid));

  auto notes2 = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(0u, notes2.size());
}

TEST_F(UserNotesPageHandlerTest, ShouldNotCreateNoteWithEmptyURL) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL::EmptyGURL());
  ASSERT_FALSE(waiter.NewNoteFinished("note5"));

  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(0u, notes.size());
}

TEST_F(UserNotesPageHandlerTest, UpdateNote) {
  EXPECT_CALL(page_, NotesChanged()).Times(2);
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));

  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes.size());
  ASSERT_EQ("note1", notes[0]->text);
  std::string guid = notes[0]->guid;

  ASSERT_TRUE(waiter.UpdateNote(guid, "note2"));

  auto notes2 = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes.size());
  ASSERT_EQ(guid, notes2[0]->guid);
  ASSERT_EQ("note2", notes2[0]->text);
}

TEST_F(UserNotesPageHandlerTest, DeleteNotesForUrl) {
  EXPECT_CALL(page_, NotesChanged()).Times(3);
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note2"));
  ASSERT_TRUE(waiter.DeleteNotesForUrl(GURL(u"https://url1")));

  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(0u, notes.size());
}

TEST_F(UserNotesPageHandlerTest, CurrentTabUrlChangedWithTabStripModelChanged) {
  AddTab(browser(), GURL(u"https://newurl1"));
  ASSERT_EQ(GURL(u"https://newurl1"), handler()->GetCurrentTabUrlForTesting());
  AddTab(browser(), GURL(u"https://newurl2"));
  ASSERT_EQ(GURL(u"https://newurl2"), handler()->GetCurrentTabUrlForTesting());
  browser()->tab_strip_model()->SelectNextTab();
  ASSERT_EQ(GURL(u"https://newurl1"), handler()->GetCurrentTabUrlForTesting());
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabCloseTypes::CLOSE_NONE);
  ASSERT_EQ(GURL(u"https://newurl2"), handler()->GetCurrentTabUrlForTesting());
}

TEST_F(UserNotesPageHandlerTest, CurrentTabUrlChangedWithNavigation) {
  AddTab(browser(), GURL(u"https://newurl1"));
  ASSERT_EQ(GURL(u"https://newurl1"), handler()->GetCurrentTabUrlForTesting());
  NavigateAndCommitActiveTab(GURL(u"https://newurl2"));
  ASSERT_EQ(GURL(u"https://newurl2"), handler()->GetCurrentTabUrlForTesting());
  AddTab(browser(), GURL(u"https://newurl3"));
  ASSERT_EQ(GURL(u"https://newurl3"), handler()->GetCurrentTabUrlForTesting());
  NavigateAndCommitActiveTab(GURL(u"https://newurl4"));
  ASSERT_EQ(GURL(u"https://newurl4"), handler()->GetCurrentTabUrlForTesting());
}

TEST_F(UserNotesPageHandlerTest, HasNotesOnAnyPages) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  ASSERT_EQ(false, waiter.HasNotesInAnyPages());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  ASSERT_TRUE(waiter.NewNoteFinished("note1"));
  ASSERT_EQ(true, waiter.HasNotesInAnyPages());
}

}  // namespace
