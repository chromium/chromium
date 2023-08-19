// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);
const char kBookmarkURL[] = "/bookmark.html";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("bookmark page");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class BookmarkBubbleViewInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList test_features_{
      power_bookmarks::kSimplifiedBookmarkSaveFlow};
};

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewInteractiveTest,
                       SimplifiedSaveFlow_NewBookmark) {
  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab,
                          embedded_test_server()->GetURL(kBookmarkURL)),

      PressButton(kBookmarkStarViewElementId),

      // The simplified flow should not show the name and folder fields by
      // default for new bookmarks.
      WaitForShow(kBookmarkSaveLocationTextId),
      EnsureNotPresent(kBookmarkNameFieldId),
      CheckViewProperty(kBookmarkBubbleOkButtonId, &views::View::HasFocus,
                        true),
      EnsureNotPresent(kBookmarkFolderFieldId),

      // Pressing the cancel button will show the fields to modify the bookmark.
      PressButton(kBookmarkSecondaryButtonId),

      WaitForShow(kBookmarkNameFieldId), EnsurePresent(kBookmarkFolderFieldId),
      EnsureNotPresent(kBookmarkSaveLocationTextId),

      FlushEvents());
}

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewInteractiveTest,
                       SimplifiedSaveFlow_ExistingBookmark) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());

  RunTestSequence(InstrumentTab(kTestTab),
                  NavigateWebContents(
                      kTestTab, embedded_test_server()->GetURL(kBookmarkURL)));

  // Add the bookmark before clicking on the star so it's "existing".
  model->AddURL(model->other_node(), 0, u"bookmark",
                browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  RunTestSequence(
      PressButton(kBookmarkStarViewElementId),

      // The simplified flow should not be shown in this case. The
      // edit fields should be visible.
      WaitForShow(kBookmarkNameFieldId),
      CheckViewProperty(kBookmarkNameFieldId, &views::View::HasFocus, true),
      EnsurePresent(kBookmarkFolderFieldId),
      EnsureNotPresent(kBookmarkSaveLocationTextId),

      FlushEvents());
}
