// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/bookmarks/test_bookmark_navigation_wrapper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

class PromptCapturingBookmarkNavigationWrapper
    : public TestingBookmarkNavigationWrapper {
 public:
  PromptCapturingBookmarkNavigationWrapper() = default;
  ~PromptCapturingBookmarkNavigationWrapper() override = default;

  void PromptForOpenAll(BrowserWindowInterface* browser,
                        size_t child_count,
                        base::OnceCallback<void(bool)> callback) override {
    prompt_child_count_ = child_count;
    prompt_callback_ = std::move(callback);
  }

  bool has_pending_prompt() const { return !prompt_callback_.is_null(); }
  std::optional<size_t> prompt_child_count() const {
    return prompt_child_count_;
  }

  void ResolvePrompt(bool confirmed) {
    CHECK(prompt_callback_);
    std::move(prompt_callback_).Run(confirmed);
  }

 private:
  std::optional<size_t> prompt_child_count_;
  base::OnceCallback<void(bool)> prompt_callback_;
};

class BookmarkUIUtilsTest : public testing::Test {
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BookmarkUIUtilsTest, HasBookmarkURLs) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;

  // This tests that |nodes| contains an URL.
  const BookmarkNode* page1 = model->AddURL(
      model->bookmark_bar_node(), 0, u"Google", GURL("http://google.com"));
  nodes.push_back(page1);
  EXPECT_TRUE(bookmarks::HasBookmarkURLs(nodes));

  nodes.clear();

  // This tests that |nodes| does not contain any URL.
  const BookmarkNode* folder1 =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder1");
  nodes.push_back(folder1);
  EXPECT_FALSE(bookmarks::HasBookmarkURLs(nodes));

  // This verifies if HasBookmarkURLs iterates through immediate children.
  model->AddURL(folder1, 0, u"Foo", GURL("http://randomsite.com"));
  EXPECT_TRUE(bookmarks::HasBookmarkURLs(nodes));

  // This verifies that HasBookmarkURLS does not iterate through descendants.
  // i.e, it should not find an URL inside a two or three level hierarchy.
  // So we add another folder to |folder1| and add another page to that new
  // folder to create a two level hierarchy.

  // But first we have to remove the URL from |folder1|.
  model->Remove(folder1->children().front().get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  const BookmarkNode* subfolder1 = model->AddFolder(folder1, 0, u"Subfolder1");

  // Now add the URL to that |subfolder1|.
  model->AddURL(subfolder1, 0, u"BAR", GURL("http://bar-foo.com"));
  EXPECT_FALSE(bookmarks::HasBookmarkURLs(nodes));
}

TEST_F(BookmarkUIUtilsTest, HasBookmarkURLsAllowedInIncognitoMode) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  const GURL history_page = GURL(chrome::kChromeUIHistoryURL);

  // This tests that |nodes| contains an disabled-in-incognito URL.
  const BookmarkNode* page1 =
      model->AddURL(model->bookmark_bar_node(), 0, u"History", history_page);
  nodes.push_back(page1);
  EXPECT_FALSE(bookmarks::HasBookmarkURLsAllowedInIncognitoMode(nodes));
  nodes.clear();

  // This tests that |nodes| contains an URL that can be opened in incognito
  // mode.
  const BookmarkNode* page2 = model->AddURL(
      model->bookmark_bar_node(), 0, u"Google", GURL("http://google.com"));
  nodes.push_back(page2);
  EXPECT_TRUE(bookmarks::HasBookmarkURLsAllowedInIncognitoMode(nodes));

  nodes.clear();

  // This tests that |nodes| does not contain any URL.
  const BookmarkNode* folder1 =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder1");
  nodes.push_back(folder1);
  EXPECT_FALSE(bookmarks::HasBookmarkURLsAllowedInIncognitoMode(nodes));

  // This verifies if HasBookmarkURLsAllowedInIncognitoMode iterates through
  // immediate children.
  // Add disabled-in-incognito url.
  model->AddURL(folder1, 0, u"Foo", history_page);
  EXPECT_FALSE(bookmarks::HasBookmarkURLsAllowedInIncognitoMode(nodes));
  // Add normal url.
  model->AddURL(folder1, 0, u"Foo", GURL("http://randomsite.com"));
  EXPECT_TRUE(bookmarks::HasBookmarkURLsAllowedInIncognitoMode(nodes));

  // This verifies that HasBookmarkURLsAllowedInIncognitoMode does not iterate
  // through descendants.
  // i.e, it should not find an URL inside a two or three level hierarchy.
  // So we add another folder to |folder1| and add another page to that new
  // folder to create a two level hierarchy.

  // But first we have to remove the URL from |folder1|.
  model->Remove(folder1->children().front().get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  const BookmarkNode* subfolder1 = model->AddFolder(folder1, 0, u"Subfolder1");

  // Now add the URL to that |subfolder1|.
  model->AddURL(subfolder1, 0, u"BAR", GURL("http://bar-foo.com"));
  EXPECT_FALSE(bookmarks::HasBookmarkURLsAllowedInIncognitoMode(nodes));
}

TEST_F(BookmarkUIUtilsTest, OpenAllIfAllowedOnBookmarksOpenedCallback) {
  std::unique_ptr<BookmarkModel> model(
      bookmarks::TestBookmarkClient::CreateModel());
  PromptCapturingBookmarkNavigationWrapper wrapper;
  bookmarks::BookmarkNavigationWrapper::SetInstanceForTesting(&wrapper);
  base::ScopedClosureRunner reset_nav_wrapper(base::BindOnce(
      &bookmarks::BookmarkNavigationWrapper::SetInstanceForTesting, nullptr));

  const BookmarkNode* empty_folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Empty");
  size_t opened_callback_count = 0;
  bookmarks::OpenAllIfAllowed(
      /*browser=*/nullptr, {empty_folder},
      ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      bookmarks::OpenAllBookmarksContext::kNone,
      /*navigation_initiator=*/std::nullopt,
      /*launch_action=*/std::nullopt,
      base::BindLambdaForTesting([&]() { ++opened_callback_count; }));
  EXPECT_EQ(0u, opened_callback_count);
  EXPECT_TRUE(wrapper.urls().empty());

  // Opening a folder below the prompt threshold invokes `on_bookmarks_opened`
  // once even when multiple URLs are opened.
  const BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 1, u"Folder");
  model->AddURL(folder, 0, u"JS", GURL("javascript:void(0)"));
  model->AddURL(folder, 1, u"One", GURL("https://example.com/1"));
  model->AddURL(folder, 2, u"Two", GURL("https://example.com/2"));

  bookmarks::OpenAllIfAllowed(
      /*browser=*/nullptr, {folder},
      ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      bookmarks::OpenAllBookmarksContext::kNone,
      /*navigation_initiator=*/std::nullopt,
      /*launch_action=*/std::nullopt,
      base::BindLambdaForTesting([&]() { ++opened_callback_count; }));
  EXPECT_EQ(1u, opened_callback_count);
  EXPECT_EQ(2u, wrapper.urls().size());

  // When prompting is required, `on_bookmarks_opened` runs only if confirmed.
  base::AutoReset<size_t> prompt_threshold(
      &bookmarks::kNumBookmarkUrlsBeforePrompting, 2);

  size_t cancelled_callback_count = 0;
  bookmarks::OpenAllIfAllowed(
      /*browser=*/nullptr, {folder},
      ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      bookmarks::OpenAllBookmarksContext::kNone,
      /*navigation_initiator=*/std::nullopt,
      /*launch_action=*/std::nullopt,
      base::BindLambdaForTesting([&]() { ++cancelled_callback_count; }));
  ASSERT_TRUE(wrapper.has_pending_prompt());
  EXPECT_EQ(3u, wrapper.prompt_child_count());
  EXPECT_EQ(0u, cancelled_callback_count);
  wrapper.ResolvePrompt(/*confirmed=*/false);
  EXPECT_EQ(0u, cancelled_callback_count);
  EXPECT_EQ(2u, wrapper.urls().size());

  size_t confirmed_callback_count = 0;
  bookmarks::OpenAllIfAllowed(
      /*browser=*/nullptr, {folder},
      ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      bookmarks::OpenAllBookmarksContext::kNone,
      /*navigation_initiator=*/std::nullopt,
      /*launch_action=*/std::nullopt,
      base::BindLambdaForTesting([&]() { ++confirmed_callback_count; }));
  ASSERT_TRUE(wrapper.has_pending_prompt());
  EXPECT_EQ(0u, confirmed_callback_count);
  wrapper.ResolvePrompt(/*confirmed=*/true);
  EXPECT_EQ(1u, confirmed_callback_count);
  EXPECT_EQ(4u, wrapper.urls().size());
}

}  // namespace
