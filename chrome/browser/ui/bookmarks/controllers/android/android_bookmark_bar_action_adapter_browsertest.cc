// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_action_adapter.h"

#include <vector>

#include "base/test/run_until.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

class AndroidBookmarkBarActionAdapterBrowserTest
    : public BrowserWindowAndroidBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    BrowserWindowAndroidBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(AndroidBookmarkBarActionAdapterBrowserTest,
                       OpenFolderNodes_NewWindow_OpensAllUrlsInNewWindow) {
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_EQ(1u, windows.size());
  BrowserWindowInterface* original_window = windows[0];

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* folder =
      model->AddFolder(model->bookmark_bar_node(), 0, u"Folder");
  const std::vector<GURL> urls = {
      embedded_test_server()->GetURL("/title1.html"),
      embedded_test_server()->GetURL("/title2.html"),
      embedded_test_server()->GetURL("/title3.html"),
  };
  for (size_t i = 0; i < urls.size(); ++i) {
    model->AddURL(folder, i, u"Bookmark", urls[i]);
  }

  AndroidBookmarkBarActionAdapter adapter(original_window);
  adapter.OpenFolderNodes({folder},
                          ui::mojom::WindowOpenDisposition::kNewWindow);

  // The first URL opens the new window, and the rest open in it once it's
  // ready.
  TabListInterface* new_tab_list = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&] {
    for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
      if (window != original_window) {
        new_tab_list = TabListInterface::From(window);
      }
    }
    return new_tab_list &&
           new_tab_list->GetTabCount() == static_cast<int>(urls.size());
  }));
  for (size_t i = 0; i < urls.size(); ++i) {
    EXPECT_EQ(urls[i], new_tab_list->GetTab(i)->GetContents()->GetVisibleURL());
  }
  EXPECT_EQ(1, TabListInterface::From(original_window)->GetTabCount());
}
