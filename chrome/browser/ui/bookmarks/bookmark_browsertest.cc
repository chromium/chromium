// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/image/image_skia.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::UrlAndTitle;

namespace {
const char kPersistBookmarkURL[] = "http://www.cnn.com/";
const char kPersistBookmarkTitle[] = "CNN";

bool IsShowingInterstitial(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  return helper &&
         helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
}

}  // namespace

class TestBookmarkTabHelperObserver : public BookmarkTabHelperObserver {
 public:
  TestBookmarkTabHelperObserver() : starred_(false) {}
  ~TestBookmarkTabHelperObserver() override {}

  void URLStarredChanged(content::WebContents*, bool starred) override {
    starred_ = starred;
  }
  bool is_starred() const { return starred_; }

 private:
  bool starred_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkTabHelperObserver);
};

class BookmarkBrowsertest : public InProcessBrowserTest {
 public:
  BookmarkBrowsertest() {}

  bool IsVisible() {
    return browser()->bookmark_bar_state() == BookmarkBar::SHOW;
  }

  static void CheckAnimation(Browser* browser, const base::Closure& quit_task) {
    if (!browser->window()->IsBookmarkBarAnimating())
      quit_task.Run();
  }

  base::TimeDelta WaitForBookmarkBarAnimationToFinish() {
    base::Time start(base::Time::Now());
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;

    base::RepeatingTimer timer;
    timer.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(15),
        base::BindRepeating(&CheckAnimation, browser(), runner->QuitClosure()));
    runner->Run();
    return base::Time::Now() - start;
  }

  BookmarkModel* WaitForBookmarkModel(Profile* profile) {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    return bookmark_model;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  // We make the histogram tester a member field to make sure it starts
  // recording as early as possible.
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBrowsertest);
};

// Test of bookmark bar toggling, visibility, and animation.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, BookmarkBarVisibleWait) {
  ASSERT_FALSE(IsVisible());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  base::TimeDelta delay = WaitForBookmarkBarAnimationToFinish();
  LOG(INFO) << "Took " << delay.InMilliseconds() << " ms to show bookmark bar";
  ASSERT_TRUE(IsVisible());
  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
  delay = WaitForBookmarkBarAnimationToFinish();
  LOG(INFO) << "Took " << delay.InMilliseconds() << " ms to hide bookmark bar";
  ASSERT_FALSE(IsVisible());
}

// Verify that bookmarks persist browser restart.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_Persist) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());

  bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kPersistBookmarkURL),
                                base::ASCIIToUTF16(kPersistBookmarkTitle));
}

#if defined(OS_WIN)
// TODO(crbug.com/935607): The test fails on Windows.
#define MAYBE_Persist DISABLED_Persist
#else
#define MAYBE_Persist Persist
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MAYBE_Persist) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());

  std::vector<UrlAndTitle> urls;
  bookmark_model->GetBookmarks(&urls);

  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GURL(kPersistBookmarkURL), urls[0].url);
  ASSERT_EQ(base::ASCIIToUTF16(kPersistBookmarkTitle), urls[0].title);
}

#if !defined(OS_CHROMEOS)  // No multi-profile on ChromeOS.

// Sanity check that bookmarks from different profiles are separate.
// DISABLED_ because it regularly times out: http://crbug.com/159002.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DISABLED_MultiProfile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  BookmarkModel* bookmark_model1 = WaitForBookmarkModel(browser()->profile());

  g_browser_process->profile_manager()->CreateMultiProfileAsync(
      base::string16(), std::string(), ProfileManager::CreateCallback());
  Browser* browser2 = ui_test_utils::WaitForBrowserToOpen();
  BookmarkModel* bookmark_model2 = WaitForBookmarkModel(browser2->profile());

  bookmarks::AddIfNotBookmarked(bookmark_model1, GURL(kPersistBookmarkURL),
                                base::ASCIIToUTF16(kPersistBookmarkTitle));
  std::vector<UrlAndTitle> urls1, urls2;
  bookmark_model1->GetBookmarks(&urls1);
  bookmark_model2->GetBookmarks(&urls2);
  ASSERT_EQ(1u, urls1.size());
  ASSERT_TRUE(urls2.empty());
}

#endif

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest,
                       HideStarOnNonbookmarkedInterstitial) {
  // Start an HTTPS server with a certificate error.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  GURL bookmark_url = embedded_test_server()->GetURL("example.test", "/");
  bookmarks::AddIfNotBookmarked(bookmark_model, bookmark_url,
                                base::ASCIIToUTF16("Bookmark"));

  TestBookmarkTabHelperObserver bookmark_observer;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  tab_helper->AddObserver(&bookmark_observer);

  // Go to a bookmarked url. Bookmark star should show.
  ui_test_utils::NavigateToURL(browser(), bookmark_url);
  EXPECT_FALSE(IsShowingInterstitial(web_contents));
  EXPECT_TRUE(bookmark_observer.is_starred());
  // Now go to a non-bookmarked url which triggers an SSL warning. Bookmark
  // star should disappear.
  GURL error_url = https_server.GetURL("/");
  ui_test_utils::NavigateToURL(browser(), error_url);
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsShowingInterstitial(web_contents));
  EXPECT_FALSE(bookmark_observer.is_starred());

  tab_helper->RemoveObserver(&bookmark_observer);
}

// Provides coverage for the Bookmark Manager bookmark drag and drag image
// generation for dragging a single bookmark.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DragSingleBookmark) {
  BookmarkModel* model = WaitForBookmarkModel(browser()->profile());
  const base::string16 page_title(base::ASCIIToUTF16("foo"));
  const GURL page_url("http://www.google.com");
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* node = model->AddURL(root, 0, page_title, page_url);
  const gfx::Point expected_point(100, 100);

  auto run_loop = std::make_unique<base::RunLoop>();

  chrome::DoBookmarkDragCallback cb = base::BindLambdaForTesting(
      [&run_loop, page_title, page_url, expected_point](
          std::unique_ptr<ui::OSExchangeData> drag_data,
          gfx::NativeView native_view,
          ui::DragDropTypes::DragEventSource source, gfx::Point point,
          int operation) {
        GURL url;
        base::string16 title;
        EXPECT_TRUE(drag_data->provider().GetURLAndTitle(
            ui::OSExchangeData::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES,
            &url, &title));
        EXPECT_EQ(page_url, url);
        EXPECT_EQ(page_title, title);
#if !defined(OS_WIN)
        // On Windows, GetDragImage() is a NOTREACHED() as the Windows
        // implementation of OSExchangeData just sets the drag image on the OS
        // API.
        // See https://crbug.com/893388.
        EXPECT_FALSE(drag_data->provider().GetDragImage().isNull());
#endif
        EXPECT_EQ(expected_point, point);
        run_loop->Quit();
      });

  constexpr int kDragNodeIndex = 0;
  chrome::DragBookmarksForTest(
      browser()->profile(),
      {{node},
       kDragNodeIndex,
       platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()),
       ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE,
       expected_point},
      std::move(cb));

  run_loop->Run();
}

// Provides coverage for the Bookmark Manager bookmark drag and drag image
// generation for dragging multiple bookmarks.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DragMultipleBookmarks) {
  BookmarkModel* model = WaitForBookmarkModel(browser()->profile());
  const base::string16 page_title(base::ASCIIToUTF16("foo"));
  const GURL page_url("http://www.google.com");
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* node1 = model->AddURL(root, 0, page_title, page_url);
  const BookmarkNode* node2 = model->AddFolder(root, 0, page_title);
  const gfx::Point expected_point(100, 100);

  auto run_loop = std::make_unique<base::RunLoop>();

  chrome::DoBookmarkDragCallback cb = base::BindLambdaForTesting(
      [&run_loop, expected_point](std::unique_ptr<ui::OSExchangeData> drag_data,
                                  gfx::NativeView native_view,
                                  ui::DragDropTypes::DragEventSource source,
                                  gfx::Point point, int operation) {
#if !defined(OS_MACOSX)
        GURL url;
        base::string16 title;
        // On Mac 10.11 and 10.12, this returns true, even though we set no url.
        // See https://crbug.com/893432.
        EXPECT_FALSE(drag_data->provider().GetURLAndTitle(
            ui::OSExchangeData::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES,
            &url, &title));
#endif
#if !defined(OS_WIN)
        // On Windows, GetDragImage() is a NOTREACHED() as the Windows
        // implementation of OSExchangeData just sets the drag image on the OS
        // API.
        // See https://crbug.com/893388.
        EXPECT_FALSE(drag_data->provider().GetDragImage().isNull());
#endif
        EXPECT_EQ(expected_point, point);
        run_loop->Quit();
      });

  constexpr int kDragNodeIndex = 1;
  chrome::DragBookmarksForTest(browser()->profile(),
                               {
                                   {node1, node2},
                                   kDragNodeIndex,
                                   platform_util::GetViewForWindow(
                                       browser()->window()->GetNativeWindow()),
                                   ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE,
                                   expected_point,
                               },
                               std::move(cb));

  run_loop->Run();
}

// ChromeOS initializes two profiles (Default and test-user) and it's impossible
// to distinguish UMA samples separately.
#if !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_EmitUmaForDuplicates) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  const BookmarkNode* parent = bookmarks::GetParentForNewNodes(bookmark_model);
  // Add one bookmark with a unique URL, two other bookmarks with a shared URL,
  // and three more with another shared URL.
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title1"), GURL("http://a.com"));
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title2"), GURL("http://b.com"));
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title3"), GURL("http://b.com"));
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title4"), GURL("http://c.com"));
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title5"), GURL("http://c.com"));
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title6"), GURL("http://c.com"));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, EmitUmaForDuplicates) {
  WaitForBookmarkModel(browser()->profile());

  ASSERT_THAT(
      histogram_tester()->GetAllSamples("Bookmarks.Count.OnProfileLoad"),
      testing::ElementsAre(base::Bucket(/*min=*/6, /*count=*/1)));
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.DuplicateUrl"),
              testing::ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_EmitUmaForEmptyTitles) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  const BookmarkNode* parent = bookmarks::GetParentForNewNodes(bookmark_model);
  // Add two bookmarks with a non-empty title and three with an empty one.
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title1"), GURL("http://a.com"));
  bookmark_model->AddURL(parent, parent->children().size(),
                         base::ASCIIToUTF16("title2"), GURL("http://b.com"));
  bookmark_model->AddURL(parent, parent->children().size(), base::string16(),
                         GURL("http://c.com"));
  bookmark_model->AddURL(parent, parent->children().size(), base::string16(),
                         GURL("http://d.com"));
  bookmark_model->AddURL(parent, parent->children().size(), base::string16(),
                         GURL("http://e.com"));
}

// TODO(crbug.com/1017731): Flaky on Windows
#if defined(OS_WIN)
#define MAYBE_EmitUmaForEmptyTitles DISABLED_EmitUmaForEmptyTitles
#else
#define MAYBE_EmitUmaForEmptyTitles EmitUmaForEmptyTitles
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MAYBE_EmitUmaForEmptyTitles) {
  WaitForBookmarkModel(browser()->profile());

  ASSERT_THAT(
      histogram_tester()->GetAllSamples("Bookmarks.Count.OnProfileLoad"),
      testing::ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.EmptyTitle"),
              testing::ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
}

#endif  // !defined(OS_CHROMEOS)
