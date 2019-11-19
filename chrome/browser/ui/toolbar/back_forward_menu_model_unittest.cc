// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

using base::ASCIIToUTF16;
using content::NavigationSimulator;
using content::WebContentsTester;

namespace {

// Creates a bitmap of the specified color.
SkBitmap CreateBitmap(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(color);
  return bitmap;
}

class FaviconDelegate : public ui::MenuModelDelegate {
 public:
  FaviconDelegate() : was_called_(false) {}

  void OnIconChanged(int model_index) override {
    was_called_ = true;
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  bool was_called() const { return was_called_; }

 private:
  bool was_called_;

  DISALLOW_COPY_AND_ASSIGN(FaviconDelegate);
};

}  // namespace

class BackFwdMenuModelTest : public ChromeRenderViewHostTestHarness {
 public:
  void ValidateModel(BackForwardMenuModel* model, int history_items,
                     int chapter_stops) {
    int h = std::min(BackForwardMenuModel::kMaxHistoryItems, history_items);
    int c = std::min(BackForwardMenuModel::kMaxChapterStops, chapter_stops);
    EXPECT_EQ(h, model->GetHistoryItemCount());
    EXPECT_EQ(c, model->GetChapterStopCount(h));
    if (h > 0)
      h += 2;  // Separator and View History link.
    if (c > 0)
      ++c;
    EXPECT_EQ(h + c, model->GetItemCount());
  }

  void LoadURLAndUpdateState(const char* url, const char* title) {
    NavigateAndCommit(GURL(url));
    web_contents()->UpdateTitleForEntry(
        controller().GetLastCommittedEntry(), base::UTF8ToUTF16(title));
  }

  // Navigate back or forward the given amount and commits the entry (which
  // will be pending after we ask to navigate there).
  void NavigateToOffset(int offset) {
    controller().GoToOffset(offset);
    WebContentsTester::For(web_contents())->CommitPendingNavigation();
  }

  // Same as NavigateToOffset but goes to an absolute index.
  void NavigateToIndex(int index) {
    controller().GoToIndex(index);
    WebContentsTester::For(web_contents())->CommitPendingNavigation();
  }
};

TEST_F(BackFwdMenuModelTest, BasicCase) {
  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  std::unique_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kForward));
  forward_model->set_test_web_contents(web_contents());

  EXPECT_EQ(0, back_model->GetItemCount());
  EXPECT_EQ(0, forward_model->GetItemCount());
  EXPECT_FALSE(back_model->ItemHasCommand(1));

  // Seed the controller with a few URLs
  LoadURLAndUpdateState("http://www.a.com/1", "A1");
  LoadURLAndUpdateState("http://www.a.com/2", "A2");
  LoadURLAndUpdateState("http://www.a.com/3", "A3");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://www.b.com/2", "B2");
  LoadURLAndUpdateState("http://www.c.com/1", "C1");
  LoadURLAndUpdateState("http://www.c.com/2", "C2");
  LoadURLAndUpdateState("http://www.c.com/3", "C3");

  // There're two more items here: a separator and a "Show Full History".
  EXPECT_EQ(9, back_model->GetItemCount());
  EXPECT_EQ(0, forward_model->GetItemCount());
  EXPECT_EQ(ASCIIToUTF16("C2"), back_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("A1"), back_model->GetLabelAt(6));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetLabelAt(8));

  EXPECT_TRUE(back_model->ItemHasCommand(0));
  EXPECT_TRUE(back_model->ItemHasCommand(6));
  EXPECT_TRUE(back_model->IsSeparator(7));
  EXPECT_TRUE(back_model->ItemHasCommand(8));
  EXPECT_FALSE(back_model->ItemHasCommand(9));
  EXPECT_FALSE(back_model->ItemHasCommand(9));

  NavigateToOffset(-7);

  EXPECT_EQ(0, back_model->GetItemCount());
  EXPECT_EQ(9, forward_model->GetItemCount());
  EXPECT_EQ(ASCIIToUTF16("A2"), forward_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("C3"), forward_model->GetLabelAt(6));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
            forward_model->GetLabelAt(8));

  EXPECT_TRUE(forward_model->ItemHasCommand(0));
  EXPECT_TRUE(forward_model->ItemHasCommand(6));
  EXPECT_TRUE(forward_model->IsSeparator(7));
  EXPECT_TRUE(forward_model->ItemHasCommand(8));
  EXPECT_FALSE(forward_model->ItemHasCommand(7));
  EXPECT_FALSE(forward_model->ItemHasCommand(9));

  NavigateToOffset(4);

  EXPECT_EQ(6, back_model->GetItemCount());
  EXPECT_EQ(5, forward_model->GetItemCount());
  EXPECT_EQ(ASCIIToUTF16("B1"), back_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("A1"), back_model->GetLabelAt(3));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetLabelAt(5));
  EXPECT_EQ(ASCIIToUTF16("C1"), forward_model->GetLabelAt(0));
  EXPECT_EQ(ASCIIToUTF16("C3"), forward_model->GetLabelAt(2));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
            forward_model->GetLabelAt(4));
}

TEST_F(BackFwdMenuModelTest, MaxItemsTest) {
  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  std::unique_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kForward));
  forward_model->set_test_web_contents(web_contents());

  // Seed the controller with 32 URLs
  LoadURLAndUpdateState("http://www.a.com/1", "A1");
  LoadURLAndUpdateState("http://www.a.com/2", "A2");
  LoadURLAndUpdateState("http://www.a.com/3", "A3");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://www.b.com/2", "B2");
  LoadURLAndUpdateState("http://www.b.com/3", "B3");
  LoadURLAndUpdateState("http://www.c.com/1", "C1");
  LoadURLAndUpdateState("http://www.c.com/2", "C2");
  LoadURLAndUpdateState("http://www.c.com/3", "C3");
  LoadURLAndUpdateState("http://www.d.com/1", "D1");
  LoadURLAndUpdateState("http://www.d.com/2", "D2");
  LoadURLAndUpdateState("http://www.d.com/3", "D3");
  LoadURLAndUpdateState("http://www.e.com/1", "E1");
  LoadURLAndUpdateState("http://www.e.com/2", "E2");
  LoadURLAndUpdateState("http://www.e.com/3", "E3");
  LoadURLAndUpdateState("http://www.f.com/1", "F1");
  LoadURLAndUpdateState("http://www.f.com/2", "F2");
  LoadURLAndUpdateState("http://www.f.com/3", "F3");
  LoadURLAndUpdateState("http://www.g.com/1", "G1");
  LoadURLAndUpdateState("http://www.g.com/2", "G2");
  LoadURLAndUpdateState("http://www.g.com/3", "G3");
  LoadURLAndUpdateState("http://www.h.com/1", "H1");
  LoadURLAndUpdateState("http://www.h.com/2", "H2");
  LoadURLAndUpdateState("http://www.h.com/3", "H3");
  LoadURLAndUpdateState("http://www.i.com/1", "I1");
  LoadURLAndUpdateState("http://www.i.com/2", "I2");
  LoadURLAndUpdateState("http://www.i.com/3", "I3");
  LoadURLAndUpdateState("http://www.j.com/1", "J1");
  LoadURLAndUpdateState("http://www.j.com/2", "J2");
  LoadURLAndUpdateState("http://www.j.com/3", "J3");
  LoadURLAndUpdateState("http://www.k.com/1", "K1");
  LoadURLAndUpdateState("http://www.k.com/2", "K2");

  // Also there're two more for a separator and a "Show Full History".
  int chapter_stop_offset = 6;
  EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
            back_model->GetItemCount());
  EXPECT_EQ(0, forward_model->GetItemCount());
  EXPECT_EQ(ASCIIToUTF16("K1"), back_model->GetLabelAt(0));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
      back_model->GetLabelAt(BackForwardMenuModel::kMaxHistoryItems + 1 +
                               chapter_stop_offset));

  // Test for out of bounds (beyond Show Full History).
  EXPECT_FALSE(back_model->ItemHasCommand(
      BackForwardMenuModel::kMaxHistoryItems + chapter_stop_offset + 2));

  EXPECT_TRUE(back_model->ItemHasCommand(
              BackForwardMenuModel::kMaxHistoryItems - 1));
  EXPECT_TRUE(back_model->IsSeparator(
              BackForwardMenuModel::kMaxHistoryItems));

  NavigateToIndex(0);

  EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
            forward_model->GetItemCount());
  EXPECT_EQ(0, back_model->GetItemCount());
  EXPECT_EQ(ASCIIToUTF16("A2"), forward_model->GetLabelAt(0));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
      forward_model->GetLabelAt(BackForwardMenuModel::kMaxHistoryItems + 1 +
                                    chapter_stop_offset));

  // Out of bounds
  EXPECT_FALSE(forward_model->ItemHasCommand(
      BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset));

  EXPECT_TRUE(forward_model->ItemHasCommand(
      BackForwardMenuModel::kMaxHistoryItems - 1));
  EXPECT_TRUE(forward_model->IsSeparator(
      BackForwardMenuModel::kMaxHistoryItems));
}

TEST_F(BackFwdMenuModelTest, ChapterStops) {
  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  std::unique_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kForward));
  forward_model->set_test_web_contents(web_contents());

  // Seed the controller with 32 URLs.
  int i = 0;
  LoadURLAndUpdateState("http://www.a.com/1", "A1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.a.com/2", "A2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.a.com/3", "A3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.b.com/2", "B2");
  ValidateModel(back_model.get(), i++, 0);
  // i = 5
  LoadURLAndUpdateState("http://www.b.com/3", "B3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.c.com/1", "C1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.c.com/2", "C2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.c.com/3", "C3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.d.com/1", "D1");
  ValidateModel(back_model.get(), i++, 0);
  // i = 10
  LoadURLAndUpdateState("http://www.d.com/2", "D2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.d.com/3", "D3");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.e.com/1", "E1");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.e.com/2", "E2");
  ValidateModel(back_model.get(), i++, 0);
  LoadURLAndUpdateState("http://www.e.com/3", "E3");
  ValidateModel(back_model.get(), i++, 0);
  // i = 15
  LoadURLAndUpdateState("http://www.f.com/1", "F1");
  ValidateModel(back_model.get(), i++, 1);
  LoadURLAndUpdateState("http://www.f.com/2", "F2");
  ValidateModel(back_model.get(), i++, 1);
  LoadURLAndUpdateState("http://www.f.com/3", "F3");
  ValidateModel(back_model.get(), i++, 1);
  LoadURLAndUpdateState("http://www.g.com/1", "G1");
  ValidateModel(back_model.get(), i++, 2);
  LoadURLAndUpdateState("http://www.g.com/2", "G2");
  ValidateModel(back_model.get(), i++, 2);
  // i = 20
  LoadURLAndUpdateState("http://www.g.com/3", "G3");
  ValidateModel(back_model.get(), i++, 2);
  LoadURLAndUpdateState("http://www.h.com/1", "H1");
  ValidateModel(back_model.get(), i++, 3);
  LoadURLAndUpdateState("http://www.h.com/2", "H2");
  ValidateModel(back_model.get(), i++, 3);
  LoadURLAndUpdateState("http://www.h.com/3", "H3");
  ValidateModel(back_model.get(), i++, 3);
  LoadURLAndUpdateState("http://www.i.com/1", "I1");
  ValidateModel(back_model.get(), i++, 4);
  // i = 25
  LoadURLAndUpdateState("http://www.i.com/2", "I2");
  ValidateModel(back_model.get(), i++, 4);
  LoadURLAndUpdateState("http://www.i.com/3", "I3");
  ValidateModel(back_model.get(), i++, 4);
  LoadURLAndUpdateState("http://www.j.com/1", "J1");
  ValidateModel(back_model.get(), i++, 5);
  LoadURLAndUpdateState("http://www.j.com/2", "J2");
  ValidateModel(back_model.get(), i++, 5);
  LoadURLAndUpdateState("http://www.j.com/3", "J3");
  ValidateModel(back_model.get(), i++, 5);
  // i = 30
  LoadURLAndUpdateState("http://www.k.com/1", "K1");
  ValidateModel(back_model.get(), i++, 6);
  LoadURLAndUpdateState("http://www.k.com/2", "K2");
  ValidateModel(back_model.get(), i++, 6);
  // i = 32
  LoadURLAndUpdateState("http://www.k.com/3", "K3");
  ValidateModel(back_model.get(), i++, 6);

  // A chapter stop is defined as the last page the user
  // browsed to within the same domain.

  // Check to see if the chapter stops have the right labels.
  int index = BackForwardMenuModel::kMaxHistoryItems;
  // Empty string indicates item is a separator.
  EXPECT_EQ(base::string16(), back_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("F3"), back_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("E3"), back_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("D3"), back_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("C3"), back_model->GetLabelAt(index++));
  // The menu should only show a maximum of 5 chapter stops.
  EXPECT_EQ(ASCIIToUTF16("B3"), back_model->GetLabelAt(index));
  // Empty string indicates item is a separator.
  EXPECT_EQ(base::string16(), back_model->GetLabelAt(index + 1));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetLabelAt(index + 2));

  // If we go back two we should still see the same chapter stop at the end.
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(ASCIIToUTF16("B3"), back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(ASCIIToUTF16("B3"), back_model->GetLabelAt(index));
  // But if we go back again, it should change.
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(ASCIIToUTF16("A3"), back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(ASCIIToUTF16("A3"), back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(ASCIIToUTF16("A3"), back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  // It is now a separator.
  EXPECT_EQ(base::string16(), back_model->GetLabelAt(index));
  // Undo our position change.
  NavigateToOffset(6);

  // Go back enough to make sure no chapter stops should appear.
  NavigateToOffset(-BackForwardMenuModel::kMaxHistoryItems);
  ValidateModel(forward_model.get(), BackForwardMenuModel::kMaxHistoryItems, 0);
  // Go forward (still no chapter stop)
  NavigationSimulator::GoForward(web_contents());
  ValidateModel(forward_model.get(),
                BackForwardMenuModel::kMaxHistoryItems - 1, 0);
  // Go back two (one chapter stop should show up)
  NavigationSimulator::GoBack(web_contents());
  NavigationSimulator::GoBack(web_contents());
  ValidateModel(forward_model.get(),
                BackForwardMenuModel::kMaxHistoryItems, 1);

  // Go to beginning.
  NavigateToIndex(0);

  // Check to see if the chapter stops have the right labels.
  index = BackForwardMenuModel::kMaxHistoryItems;
  // Empty string indicates item is a separator.
  EXPECT_EQ(base::string16(), forward_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("E3"), forward_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("F3"), forward_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("G3"), forward_model->GetLabelAt(index++));
  EXPECT_EQ(ASCIIToUTF16("H3"), forward_model->GetLabelAt(index++));
  // The menu should only show a maximum of 5 chapter stops.
  EXPECT_EQ(ASCIIToUTF16("I3"), forward_model->GetLabelAt(index));
  // Empty string indicates item is a separator.
  EXPECT_EQ(base::string16(), forward_model->GetLabelAt(index + 1));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
      forward_model->GetLabelAt(index + 2));

  // If we advance one we should still see the same chapter stop at the end.
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(ASCIIToUTF16("I3"), forward_model->GetLabelAt(index));
  // But if we advance one again, it should change.
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(ASCIIToUTF16("J3"), forward_model->GetLabelAt(index));
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(ASCIIToUTF16("J3"), forward_model->GetLabelAt(index));
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(ASCIIToUTF16("J3"), forward_model->GetLabelAt(index));
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(ASCIIToUTF16("K3"), forward_model->GetLabelAt(index));

  // Now test the boundary cases by using the chapter stop function directly.
  // Out of bounds, first too far right (incrementing), then too far left.
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(33, false));
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(-1, true));
  // Test being at end and going right, then at beginning going left.
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(32, true));
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(0, false));
  // Test success: beginning going right and end going left.
  EXPECT_EQ(2,  back_model->GetIndexOfNextChapterStop(0, true));
  EXPECT_EQ(29, back_model->GetIndexOfNextChapterStop(32, false));
  // Now see when the chapter stops begin to show up.
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(1, false));
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(2, false));
  EXPECT_EQ(2,  back_model->GetIndexOfNextChapterStop(3, false));
  // Now see when the chapter stops end.
  EXPECT_EQ(32, back_model->GetIndexOfNextChapterStop(30, true));
  EXPECT_EQ(32, back_model->GetIndexOfNextChapterStop(31, true));
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(32, true));

  // Bug found during review (two different sites, but first wasn't considered
  // a chapter-stop).
  // Go to A1;
  NavigateToIndex(0);
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  EXPECT_EQ(0, back_model->GetIndexOfNextChapterStop(1, false));
  EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(0, true));

  // Now see if it counts 'www.x.com' and 'mail.x.com' as same domain, which
  // it should.
  // Go to A1.
  NavigateToIndex(0);
  LoadURLAndUpdateState("http://mail.a.com/2", "A2-mai");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://mail.b.com/2", "B2-mai");
  LoadURLAndUpdateState("http://new.site.com", "new");
  EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(0, true));
  EXPECT_EQ(3, back_model->GetIndexOfNextChapterStop(1, true));
  EXPECT_EQ(3, back_model->GetIndexOfNextChapterStop(2, true));
  EXPECT_EQ(4, back_model->GetIndexOfNextChapterStop(3, true));
  // And try backwards as well.
  EXPECT_EQ(3, back_model->GetIndexOfNextChapterStop(4, false));
  EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(3, false));
  EXPECT_EQ(1, back_model->GetIndexOfNextChapterStop(2, false));
  EXPECT_EQ(-1, back_model->GetIndexOfNextChapterStop(1, false));
}

TEST_F(BackFwdMenuModelTest, EscapeLabel) {
  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      nullptr, BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  EXPECT_EQ(0, back_model->GetItemCount());
  EXPECT_FALSE(back_model->ItemHasCommand(1));

  // Note: Multiple navigations to the same URL in a row have to be
  // renderer-initiated.  If they were browser-initiated, the
  // NavigationController would treat them as reloads.
  LoadURLAndUpdateState("http://www.a.com/1", "A B");
  NavigationSimulator::NavigateAndCommitFromDocument(GURL("http://www.a.com/1"),
                                                     main_rfh());
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      base::UTF8ToUTF16("A & B"));
  LoadURLAndUpdateState("http://www.a.com/2", "A && B");
  NavigationSimulator::NavigateAndCommitFromDocument(GURL("http://www.a.com/2"),
                                                     main_rfh());
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      base::UTF8ToUTF16("A &&& B"));
  LoadURLAndUpdateState("http://www.a.com/3", "");

  EXPECT_EQ(6, back_model->GetItemCount());

  EXPECT_EQ(ASCIIToUTF16("A B"), back_model->GetLabelAt(3));
  EXPECT_EQ(ASCIIToUTF16("A && B"), back_model->GetLabelAt(2));
  EXPECT_EQ(ASCIIToUTF16("A &&&& B"), back_model->GetLabelAt(1));
  EXPECT_EQ(ASCIIToUTF16("A &&&&&& B"), back_model->GetLabelAt(0));
}

// Test asynchronous loading of favicon from history service.
TEST_F(BackFwdMenuModelTest, FaviconLoadTest) {
  ASSERT_TRUE(profile()->CreateHistoryService(true, false));
  profile()->CreateFaviconService();
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(&native_params));
  FaviconDelegate favicon_delegate;

  BackForwardMenuModel back_model(browser.get(),
                                  BackForwardMenuModel::ModelType::kBackward);
  back_model.set_test_web_contents(controller().GetWebContents());
  back_model.SetMenuModelDelegate(&favicon_delegate);

  SkBitmap new_icon_bitmap(CreateBitmap(SK_ColorRED));

  GURL url1 = GURL("http://www.a.com/1");
  GURL url2 = GURL("http://www.a.com/2");
  GURL url1_favicon("http://www.a.com/1/favicon.ico");

  NavigateAndCommit(url1);
  // Navigate to a new URL so that url1 will be in the BackForwardMenuModel.
  NavigateAndCommit(url2);

  // Set the desired favicon for url1.
  HistoryServiceFactory::GetForProfile(profile(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPage(url1, base::Time::Now(), history::SOURCE_BROWSED);
  FaviconServiceFactory::GetForProfile(profile(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetFavicons({url1}, url1_favicon, favicon_base::IconType::kFavicon,
                    gfx::Image::CreateFrom1xBitmap(new_icon_bitmap));

  // Will return the current icon (default) but start an anync call
  // to retrieve the favicon from the favicon service.
  gfx::Image default_icon;
  back_model.GetIconAt(0, &default_icon);

  // Make the favicon service run GetFavIconForURL,
  // FaviconDelegate.OnIconChanged will be called.
  base::RunLoop().Run();

  // Verify that the callback executed.
  EXPECT_TRUE(favicon_delegate.was_called());

  // Verify the bitmaps match.
  gfx::Image valid_icon;
  // This time we will get the new favicon returned.
  back_model.GetIconAt(0, &valid_icon);

  SkBitmap default_icon_bitmap = *default_icon.ToSkBitmap();
  SkBitmap valid_icon_bitmap = *valid_icon.ToSkBitmap();

  // Verify we did not get the default favicon.
  EXPECT_NE(
      0, memcmp(default_icon_bitmap.getPixels(), valid_icon_bitmap.getPixels(),
                default_icon_bitmap.computeByteSize()));
  // Verify we did get the expected favicon.
  EXPECT_EQ(0,
            memcmp(new_icon_bitmap.getPixels(), valid_icon_bitmap.getPixels(),
                   new_icon_bitmap.computeByteSize()));

  // Make sure the browser deconstructor doesn't have problems.
  browser->tab_strip_model()->CloseAllTabs();
}
