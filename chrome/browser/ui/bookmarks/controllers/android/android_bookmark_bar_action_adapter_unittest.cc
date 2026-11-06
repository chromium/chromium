// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_action_adapter.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;
using testing::Truly;
using ui::mojom::WindowOpenDisposition;

auto MatchesOpenUrlParams(const GURL& url, WindowOpenDisposition disposition) {
  return AllOf(Field(&content::OpenURLParams::url, url),
               Field(&content::OpenURLParams::disposition, disposition),
               Field(&content::OpenURLParams::transition,
                     Truly([](ui::PageTransition transition) {
                       return ui::PageTransitionCoreTypeIs(
                           transition, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
                     })));
}

class AndroidBookmarkBarActionAdapterTest : public testing::Test {
 protected:
  std::unique_ptr<bookmarks::BookmarkModel> model_ =
      bookmarks::TestBookmarkClient::CreateModel();
  testing::StrictMock<MockBrowserWindowInterface> browser_;
  AndroidBookmarkBarActionAdapter adapter_{&browser_};
};

TEST_F(AndroidBookmarkBarActionAdapterTest, OpenBookmarkOpensUrlInBrowser) {
  const bookmarks::BookmarkNode* url = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"A", GURL("https://a.test"));

  EXPECT_CALL(browser_, OpenURL(MatchesOpenUrlParams(
                                    GURL("https://a.test"),
                                    WindowOpenDisposition::kNewBackgroundTab),
                                _));
  adapter_.OpenBookmark(url, WindowOpenDisposition::kNewBackgroundTab);
}

TEST_F(AndroidBookmarkBarActionAdapterTest,
       OpenBookmarkIgnoresUnsupportedDispositions) {
  const bookmarks::BookmarkNode* url = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"A", GURL("https://a.test"));

  // The StrictMock fails if `OpenURL()` is called.
  adapter_.OpenBookmark(url, WindowOpenDisposition::kSaveToDisk);
  adapter_.OpenBookmark(url, WindowOpenDisposition::kNewSplitView);
}

TEST_F(AndroidBookmarkBarActionAdapterTest,
       OpenBookmarkSetsBookmarkIdOnNavigation) {
  const bookmarks::BookmarkNode* url = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"A", GURL("https://a.test"));

  ChromeNavigationUIData ui_data;
  testing::NiceMock<content::MockNavigationHandle> handle;
  ON_CALL(handle, GetNavigationUIData()).WillByDefault(Return(&ui_data));
  EXPECT_CALL(browser_,
              OpenURL(MatchesOpenUrlParams(GURL("https://a.test"),
                                           WindowOpenDisposition::kCurrentTab),
                      _))
      .WillOnce([&](const content::OpenURLParams&,
                    base::OnceCallback<void(content::NavigationHandle&)>
                        navigation_handle_callback) {
        std::move(navigation_handle_callback).Run(handle);
        return nullptr;
      });
  adapter_.OpenBookmark(url, WindowOpenDisposition::kCurrentTab);

  EXPECT_EQ(ui_data.bookmark_id(), url->id());
}

// Java records the same histogram, so the bucket layouts must match.
TEST_F(AndroidBookmarkBarActionAdapterTest,
       OpenBookmarkRecordsTimeIntervalWithJavaLayout) {
  const bookmarks::BookmarkNode* url = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"A", GURL("https://a.test"));
  base::HistogramTester histograms;
  EXPECT_CALL(browser_, OpenURL(_, _));
  adapter_.OpenBookmark(url, WindowOpenDisposition::kCurrentTab);

  histograms.ExpectTotalCount("Bookmarks.OpenBookmarkTimeInterval2.Normal", 1);
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(
          "Bookmarks.OpenBookmarkTimeInterval2.Normal");
  ASSERT_TRUE(histogram);
  // `RecordHistogram.recordCustomTimesHistogram(name, ms, 1, 30 days, 50)`.
  EXPECT_EQ(histogram->GetHistogramType(), base::HISTOGRAM);
  EXPECT_TRUE(histogram->HasConstructionArguments(
      1, std::numeric_limits<int32_t>::max() - 1, 50));
}

TEST_F(AndroidBookmarkBarActionAdapterTest,
       OpenFolderNodesOpensChildUrlsInBrowser) {
  const bookmarks::BookmarkNode* folder1 =
      model_->AddFolder(model_->bookmark_bar_node(), 0, u"Folder 1");
  model_->AddURL(folder1, 0, u"A", GURL("https://a.test"));
  model_->AddFolder(folder1, 1, u"Nested Folder");
  model_->AddURL(folder1, 2, u"B", GURL("https://b.test"));

  const bookmarks::BookmarkNode* folder2 =
      model_->AddFolder(model_->bookmark_bar_node(), 1, u"Folder 2");
  model_->AddURL(folder2, 0, u"C", GURL("https://c.test"));

  testing::InSequence seq;
  EXPECT_CALL(browser_, OpenURL(MatchesOpenUrlParams(
                                    GURL("https://a.test"),
                                    WindowOpenDisposition::kNewBackgroundTab),
                                _));
  EXPECT_CALL(browser_, OpenURL(MatchesOpenUrlParams(
                                    GURL("https://b.test"),
                                    WindowOpenDisposition::kNewBackgroundTab),
                                _));
  EXPECT_CALL(browser_, OpenURL(MatchesOpenUrlParams(
                                    GURL("https://c.test"),
                                    WindowOpenDisposition::kNewBackgroundTab),
                                _));
  adapter_.OpenFolderNodes({folder1, folder2},
                           WindowOpenDisposition::kNewBackgroundTab);
}

TEST_F(AndroidBookmarkBarActionAdapterTest, CanPasteFromClipboardReturnsFalse) {
  base::test::TestFuture<bool> future;
  adapter_.CanPasteFromClipboard({model_->bookmark_bar_node()},
                                 future.GetCallback());
  EXPECT_FALSE(future.Get());
}

}  // namespace
