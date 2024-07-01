// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

using base::ASCIIToUTF16;
using content::NavigationSimulator;
using content::WebContentsTester;

namespace {

class TestBackForwardMenuDelegate : public ui::MenuModelDelegate {
 public:
  explicit TestBackForwardMenuDelegate(base::OnceClosure quit_closure)
      : was_icon_changed_called_(false),
        was_menu_model_changed_called_(false),
        quit_closure_(std::move(quit_closure)) {}

  TestBackForwardMenuDelegate(const TestBackForwardMenuDelegate&) = delete;
  TestBackForwardMenuDelegate& operator=(const TestBackForwardMenuDelegate&) =
      delete;

  void OnIconChanged(int command_id) override {
    was_icon_changed_called_ = true;
    std::move(quit_closure_).Run();
  }

  void OnMenuStructureChanged() override {
    was_menu_model_changed_called_ = true;
    std::move(quit_closure_).Run();
  }

  bool was_icon_changed_called() const { return was_icon_changed_called_; }
  bool was_menu_model_changed_called() const {
    return was_menu_model_changed_called_;
  }

 private:
  bool was_icon_changed_called_;
  bool was_menu_model_changed_called_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class BackFwdMenuModelTest : public ChromeRenderViewHostTestHarness {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                HistoryServiceFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                FaviconServiceFactory::GetInstance(),
                FaviconServiceFactory::GetDefaultFactory()}};
  }

  void ValidateModel(BackForwardMenuModel* model,
                     size_t history_items,
                     size_t chapter_stops) {
    size_t h = std::min(BackForwardMenuModel::kMaxHistoryItems, history_items);
    size_t c = std::min(BackForwardMenuModel::kMaxChapterStops, chapter_stops);
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

class BackFwdMenuModelIncognitoTest : public ChromeRenderViewHostTestHarness {
 public:
  BackFwdMenuModelIncognitoTest() = default;

  void LoadURLAndUpdateState(const char* url, const std::u16string& title) {
    NavigateAndCommit(GURL(url));
    web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                        title);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BackFwdMenuModelTest, BasicCase) {
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));

  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  std::unique_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kForward));
  forward_model->set_test_web_contents(web_contents());

  EXPECT_EQ(0u, back_model->GetItemCount());
  EXPECT_EQ(0u, forward_model->GetItemCount());
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
  EXPECT_EQ(9u, back_model->GetItemCount());
  EXPECT_EQ(0u, forward_model->GetItemCount());
  EXPECT_EQ(u"C2", back_model->GetLabelAt(0));
  EXPECT_EQ(u"A1", back_model->GetLabelAt(6));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetLabelAt(8));

  EXPECT_TRUE(back_model->ItemHasCommand(0));
  EXPECT_TRUE(back_model->ItemHasCommand(6));
  EXPECT_TRUE(back_model->IsSeparator(7));
  EXPECT_TRUE(back_model->ItemHasCommand(8));
  EXPECT_FALSE(back_model->ItemHasCommand(9));
  EXPECT_FALSE(back_model->ItemHasCommand(9));

  NavigateToOffset(-7);

  EXPECT_EQ(0u, back_model->GetItemCount());
  EXPECT_EQ(9u, forward_model->GetItemCount());
  EXPECT_EQ(u"A2", forward_model->GetLabelAt(0));
  EXPECT_EQ(u"C3", forward_model->GetLabelAt(6));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
            forward_model->GetLabelAt(8));

  EXPECT_TRUE(forward_model->ItemHasCommand(0));
  EXPECT_TRUE(forward_model->ItemHasCommand(6));
  EXPECT_TRUE(forward_model->IsSeparator(7));
  EXPECT_TRUE(forward_model->ItemHasCommand(8));
  EXPECT_FALSE(forward_model->ItemHasCommand(7));
  EXPECT_FALSE(forward_model->ItemHasCommand(9));

  NavigateToOffset(4);

  EXPECT_EQ(6u, back_model->GetItemCount());
  EXPECT_EQ(5u, forward_model->GetItemCount());
  EXPECT_EQ(u"B1", back_model->GetLabelAt(0));
  EXPECT_EQ(u"A1", back_model->GetLabelAt(3));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetLabelAt(5));
  EXPECT_EQ(u"C1", forward_model->GetLabelAt(0));
  EXPECT_EQ(u"C3", forward_model->GetLabelAt(2));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
            forward_model->GetLabelAt(4));
}

TEST_F(BackFwdMenuModelTest, MaxItemsTest) {
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));

  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  std::unique_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kForward));
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
  size_t chapter_stop_offset = 6;
  EXPECT_EQ(BackForwardMenuModel::kMaxHistoryItems + 2 + chapter_stop_offset,
            back_model->GetItemCount());
  EXPECT_EQ(0u, forward_model->GetItemCount());
  EXPECT_EQ(u"K1", back_model->GetLabelAt(0));
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
  EXPECT_EQ(0u, back_model->GetItemCount());
  EXPECT_EQ(u"A2", forward_model->GetLabelAt(0));
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
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));

  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  std::unique_ptr<BackForwardMenuModel> forward_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kForward));
  forward_model->set_test_web_contents(web_contents());

  // Seed the controller with 32 URLs.
  size_t i = 0;
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
  size_t index = BackForwardMenuModel::kMaxHistoryItems;
  // Empty string indicates item is a separator.
  EXPECT_EQ(std::u16string(), back_model->GetLabelAt(index++));
  EXPECT_EQ(u"F3", back_model->GetLabelAt(index++));
  EXPECT_EQ(u"E3", back_model->GetLabelAt(index++));
  EXPECT_EQ(u"D3", back_model->GetLabelAt(index++));
  EXPECT_EQ(u"C3", back_model->GetLabelAt(index++));
  // The menu should only show a maximum of 5 chapter stops.
  EXPECT_EQ(u"B3", back_model->GetLabelAt(index));
  // Empty string indicates item is a separator.
  EXPECT_EQ(std::u16string(), back_model->GetLabelAt(index + 1));
  EXPECT_EQ(back_model->GetShowFullHistoryLabel(),
            back_model->GetLabelAt(index + 2));

  // If we go back two we should still see the same chapter stop at the end.
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(u"B3", back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(u"B3", back_model->GetLabelAt(index));
  // But if we go back again, it should change.
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(u"A3", back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(u"A3", back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  EXPECT_EQ(u"A3", back_model->GetLabelAt(index));
  NavigationSimulator::GoBack(web_contents());
  // It is now a separator.
  EXPECT_EQ(std::u16string(), back_model->GetLabelAt(index));
  // Undo our position change.
  NavigateToOffset(6);

  // Go back enough to make sure no chapter stops should appear.
  NavigateToOffset(-static_cast<int>(BackForwardMenuModel::kMaxHistoryItems));
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
  EXPECT_EQ(std::u16string(), forward_model->GetLabelAt(index++));
  EXPECT_EQ(u"E3", forward_model->GetLabelAt(index++));
  EXPECT_EQ(u"F3", forward_model->GetLabelAt(index++));
  EXPECT_EQ(u"G3", forward_model->GetLabelAt(index++));
  EXPECT_EQ(u"H3", forward_model->GetLabelAt(index++));
  // The menu should only show a maximum of 5 chapter stops.
  EXPECT_EQ(u"I3", forward_model->GetLabelAt(index));
  // Empty string indicates item is a separator.
  EXPECT_EQ(std::u16string(), forward_model->GetLabelAt(index + 1));
  EXPECT_EQ(forward_model->GetShowFullHistoryLabel(),
      forward_model->GetLabelAt(index + 2));

  // If we advance one we should still see the same chapter stop at the end.
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(u"I3", forward_model->GetLabelAt(index));
  // But if we advance one again, it should change.
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(u"J3", forward_model->GetLabelAt(index));
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(u"J3", forward_model->GetLabelAt(index));
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(u"J3", forward_model->GetLabelAt(index));
  NavigationSimulator::GoForward(web_contents());
  EXPECT_EQ(u"K3", forward_model->GetLabelAt(index));

  // Now test the boundary cases by using the chapter stop function directly.
  // Out of bounds, first decrementing, then incrementing.
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(33, false).has_value());
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(33, true).has_value());
  // Test being at end and going right, then at beginning going left.
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(32, true).has_value());
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(0, false).has_value());
  // Test success: beginning going right and end going left.
  EXPECT_EQ(2u, back_model->GetIndexOfNextChapterStop(0, true));
  EXPECT_EQ(29u, back_model->GetIndexOfNextChapterStop(32, false));
  // Now see when the chapter stops begin to show up.
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(1, false).has_value());
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(2, false).has_value());
  EXPECT_EQ(2u, back_model->GetIndexOfNextChapterStop(3, false));
  // Now see when the chapter stops end.
  EXPECT_EQ(32u, back_model->GetIndexOfNextChapterStop(30, true));
  EXPECT_EQ(32u, back_model->GetIndexOfNextChapterStop(31, true));
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(32, true).has_value());

  if (content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    // The case below currently fails on the linux-bfcache-rel bot with
    // back/forward cache enabled, so return early.
    // TODO(crbug.com/40780539): re-enable this test.
    return;
  }

  // Bug found during review (two different sites, but first wasn't considered
  // a chapter-stop).
  // Go to A1;
  NavigateToIndex(0);
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  EXPECT_EQ(0u, back_model->GetIndexOfNextChapterStop(1, false));
  EXPECT_EQ(1u, back_model->GetIndexOfNextChapterStop(0, true));

  // Now see if it counts 'www.x.com' and 'mail.x.com' as same domain, which
  // it should.
  // Go to A1.
  NavigateToIndex(0);
  LoadURLAndUpdateState("http://mail.a.com/2", "A2-mai");
  LoadURLAndUpdateState("http://www.b.com/1", "B1");
  LoadURLAndUpdateState("http://mail.b.com/2", "B2-mai");
  LoadURLAndUpdateState("http://new.site.com", "new");
  EXPECT_EQ(1u, back_model->GetIndexOfNextChapterStop(0, true));
  EXPECT_EQ(3u, back_model->GetIndexOfNextChapterStop(1, true));
  EXPECT_EQ(3u, back_model->GetIndexOfNextChapterStop(2, true));
  EXPECT_EQ(4u, back_model->GetIndexOfNextChapterStop(3, true));
  // And try backwards as well.
  EXPECT_EQ(3u, back_model->GetIndexOfNextChapterStop(4, false));
  EXPECT_EQ(1u, back_model->GetIndexOfNextChapterStop(3, false));
  EXPECT_EQ(1u, back_model->GetIndexOfNextChapterStop(2, false));
  EXPECT_FALSE(back_model->GetIndexOfNextChapterStop(1, false).has_value());
}

TEST_F(BackFwdMenuModelTest, EscapeLabel) {
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));

  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());

  EXPECT_EQ(0u, back_model->GetItemCount());
  EXPECT_FALSE(back_model->ItemHasCommand(1));

  LoadURLAndUpdateState("http://www.a.com/1", "A B");
  LoadURLAndUpdateState("http://www.a.com/2", "A & B");
  LoadURLAndUpdateState("http://www.a.com/3", "A && B");
  LoadURLAndUpdateState("http://www.a.com/4", "A &&& B");
  LoadURLAndUpdateState("http://www.a.com/5", "");

  EXPECT_EQ(6u, back_model->GetItemCount());

  EXPECT_EQ(u"A B", back_model->GetLabelAt(3));
  EXPECT_EQ(u"A && B", back_model->GetLabelAt(2));
  EXPECT_EQ(u"A &&&& B", back_model->GetLabelAt(1));
  EXPECT_EQ(u"A &&&&&& B", back_model->GetLabelAt(0));
}

// Test asynchronous loading of favicon from history service.
TEST_F(BackFwdMenuModelTest, FaviconLoadTest) {
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));
  base::RunLoop loop;
  TestBackForwardMenuDelegate delegate(loop.QuitWhenIdleClosure());

  BackForwardMenuModel back_model(browser.get(),
                                  BackForwardMenuModel::ModelType::kBackward);
  back_model.set_test_web_contents(web_contents());
  back_model.SetMenuModelDelegate(&delegate);

  SkBitmap new_icon_bitmap(gfx::test::CreateBitmap(/*size=*/16, SK_ColorRED));

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
  ui::ImageModel default_icon = back_model.GetIconAt(0);

  // Make the favicon service run GetFavIconForURL,
  // MenuModelDelegate.OnIconChanged will be called.
  loop.Run();

  // Verify that the callback executed.
  EXPECT_TRUE(delegate.was_icon_changed_called());

  // Verify the bitmaps match.
  // This time we will get the new favicon returned.
  ui::ImageModel valid_icon = back_model.GetIconAt(0);

  SkBitmap default_icon_bitmap = *default_icon.GetImage().ToSkBitmap();
  SkBitmap valid_icon_bitmap = *valid_icon.GetImage().ToSkBitmap();

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

TEST_F(BackFwdMenuModelTest, NavigationWhenMenuShownTest) {
  Browser::CreateParams native_params(profile(), true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));
  base::RunLoop loop;
  TestBackForwardMenuDelegate delegate(loop.QuitWhenIdleClosure());

  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kBackward));
  back_model->set_test_web_contents(web_contents());
  back_model->SetMenuModelDelegate(&delegate);

  EXPECT_EQ(0u, back_model->GetItemCount());

  LoadURLAndUpdateState("http://www.a.com/1", "A1");
  LoadURLAndUpdateState("http://www.a.com/2", "A2");

  EXPECT_EQ(3u, back_model->GetItemCount());
  back_model->MenuWillShow();

  // Trigger a navigation while the menu is open
  LoadURLAndUpdateState("http://www.b.com", "B");

  // Confirm delegate is notified about menu contents has changed
  loop.Run();
  EXPECT_TRUE(delegate.was_menu_model_changed_called());

  EXPECT_EQ(4u, back_model->GetItemCount());
}

// Test to check the menu in Incognito mode.
TEST_F(BackFwdMenuModelIncognitoTest, IncognitoCaseTest) {
  Browser::CreateParams native_params(profile()->GetPrimaryOTRProfile(true),
                                      true);
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForParams(native_params));

  std::unique_ptr<BackForwardMenuModel> back_model(new BackForwardMenuModel(
      browser.get(), BackForwardMenuModel::ModelType::kBackward));

  back_model->set_test_web_contents(web_contents());

  EXPECT_EQ(0u, back_model->GetItemCount());
  EXPECT_FALSE(back_model->ItemHasCommand(1));

  // Seed the controller with a few URLs
  LoadURLAndUpdateState("http://www.a.com/1", u"A1");
  LoadURLAndUpdateState("http://www.a.com/2", u"A2");
  LoadURLAndUpdateState("http://www.a.com/3", u"A3");

  // There're should be only the visited pages but not "Show Full History" item
  // and its separator.
  EXPECT_EQ(2u, back_model->GetItemCount());
  EXPECT_EQ(u"A2", back_model->GetLabelAt(0));
  EXPECT_EQ(u"A1", back_model->GetLabelAt(1));

  EXPECT_TRUE(back_model->ItemHasCommand(0));
  EXPECT_TRUE(back_model->ItemHasCommand(1));
}
