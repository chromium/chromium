// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_action_adapter.h"

#include <memory>

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "content/public/browser/page_navigator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::Field;
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
       OpenBookmarkSkipsJavascriptInNewTab) {
  const bookmarks::BookmarkNode* js_node = model_->AddURL(
      model_->bookmark_bar_node(), 0, u"JS", GURL("javascript:alert(1)"));

  adapter_.OpenBookmark(js_node, WindowOpenDisposition::kNewBackgroundTab);

  EXPECT_CALL(browser_,
              OpenURL(MatchesOpenUrlParams(GURL("javascript:alert(1)"),
                                           WindowOpenDisposition::kCurrentTab),
                      _));
  adapter_.OpenBookmark(js_node, WindowOpenDisposition::kCurrentTab);
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
