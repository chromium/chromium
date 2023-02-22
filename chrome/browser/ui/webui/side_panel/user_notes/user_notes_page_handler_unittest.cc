// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes.mojom-test-utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
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
    features_.InitAndEnableFeature(power_bookmarks::kPowerBookmarkBackend);
    BrowserWithTestWindowTest::SetUp();
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

}  // namespace
