// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"

#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestBookmarksPageHandler : public BookmarksPageHandler {
 public:
  TestBookmarksPageHandler()
      : BookmarksPageHandler(
            mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler>(),
            static_cast<BookmarksSidePanelUI*>(nullptr)) {}
};

class BookmarksPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<TestBookmarksPageHandler>();
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestBookmarksPageHandler* handler() { return handler_.get(); }

 private:
  std::unique_ptr<TestBookmarksPageHandler> handler_;
};

TEST_F(BookmarksPageHandlerTest, SetSortOrder) {
  auto sort_order = side_panel::mojom::SortOrder::kOldest;
  handler()->SetSortOrder(sort_order);
  PrefService* pref_service = profile()->GetPrefs();
  ASSERT_EQ(
      pref_service->GetInteger(bookmarks_webui::prefs::kBookmarksSortOrder),
      static_cast<int>(sort_order));
}

TEST_F(BookmarksPageHandlerTest, SetViewType) {
  auto view_type = side_panel::mojom::ViewType::kExpanded;
  handler()->SetViewType(view_type);
  PrefService* pref_service = profile()->GetPrefs();
  ASSERT_EQ(
      pref_service->GetInteger(bookmarks_webui::prefs::kBookmarksViewType),
      static_cast<int>(view_type));
}

}  // namespace
