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

namespace {

class TestUserNotesPageHandler : public UserNotesPageHandler {
 public:
  explicit TestUserNotesPageHandler(Profile* profile)
      : UserNotesPageHandler(
            mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler>(),
            profile,
            nullptr) {}
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
    handler_ = std::make_unique<TestUserNotesPageHandler>(profile());

    GURL url1(u"https://url1");
    GURL url2(u"https://url2");
    GURL url3(u"https://url3");

    std::vector<Note> notes;
    notes.push_back({url1, "note1"});
    notes.push_back({url2, "note2"});
    notes.push_back({url3, "note3"});
    notes.push_back({url1, "note4"});

    side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
    for (auto& note : notes) {
      handler_->SetCurrentTabUrlForTesting(note.url);
      ASSERT_TRUE(waiter.NewNoteFinished(note.text));
    }
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestUserNotesPageHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<TestUserNotesPageHandler> handler_;
  base::test::ScopedFeatureList features_;
};

TEST_F(UserNotesPageHandlerTest, GetNotes) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(2u, notes.size());

  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url2"));
  auto notes2 = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes2.size());
  ASSERT_EQ("note2", notes2[0]->text);
}

TEST_F(UserNotesPageHandlerTest, GetNoteOverviews) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  auto note_overviews = waiter.GetNoteOverviews("");
  ASSERT_EQ(3u, note_overviews.size());
}

TEST_F(UserNotesPageHandlerTest, CreateAndDeleteNote) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url5"));
  ASSERT_TRUE(waiter.NewNoteFinished("note5"));

  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes.size());
  std::string guid = notes[0]->guid;

  ASSERT_TRUE(waiter.DeleteNote(guid));

  auto notes2 = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(0u, notes2.size());
}

TEST_F(UserNotesPageHandlerTest, UpdateNote) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url5"));
  ASSERT_TRUE(waiter.NewNoteFinished("note5"));

  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes.size());
  ASSERT_EQ("note5", notes[0]->text);
  std::string guid = notes[0]->guid;

  ASSERT_TRUE(waiter.UpdateNote(guid, "note6"));

  auto notes2 = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(1u, notes.size());
  ASSERT_EQ(guid, notes2[0]->guid);
  ASSERT_EQ("note6", notes2[0]->text);
}

TEST_F(UserNotesPageHandlerTest, DeleteNotesForUrl) {
  side_panel::mojom::UserNotesPageHandlerAsyncWaiter waiter(handler());
  ASSERT_TRUE(waiter.DeleteNotesForUrl(GURL(u"https://url1")));

  handler()->SetCurrentTabUrlForTesting(GURL(u"https://url1"));
  auto notes = waiter.GetNotesForCurrentTab();
  ASSERT_EQ(0u, notes.size());
}

}  // namespace
