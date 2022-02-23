// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::UrlAndTitle;

namespace {
const char kPersistBookmarkURL[] = "http://www.cnn.com/";
const char16_t kPersistBookmarkTitle[] = u"CNN";

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

  TestBookmarkTabHelperObserver(const TestBookmarkTabHelperObserver&) = delete;
  TestBookmarkTabHelperObserver& operator=(
      const TestBookmarkTabHelperObserver&) = delete;

  ~TestBookmarkTabHelperObserver() override {}

  void URLStarredChanged(content::WebContents*, bool starred) override {
    starred_ = starred;
  }
  bool is_starred() const { return starred_; }

 private:
  bool starred_;
};

class BookmarkBrowsertest : public InProcessBrowserTest {
 public:
  BookmarkBrowsertest() {
    // This needs to be disabled so that animations are guaranteed to work.
#if BUILDFLAG(IS_WIN)
    feature_list_.InitWithFeatures(
        {}, {features::kApplyNativeOcclusionToCompositor});
#endif
  }

  BookmarkBrowsertest(const BookmarkBrowsertest&) = delete;
  BookmarkBrowsertest& operator=(const BookmarkBrowsertest&) = delete;

  bool IsVisible() {
    return browser()->bookmark_bar_state() == BookmarkBar::SHOW;
  }

  static void CheckAnimation(Browser* browser, base::RunLoop* loop) {
    if (!browser->window()->IsBookmarkBarAnimating())
      loop->Quit();
  }

  base::TimeDelta WaitForBookmarkBarAnimationToFinish() {
    base::Time start(base::Time::Now());
    {
      base::RunLoop loop;
      base::RepeatingTimer timer;
      timer.Start(FROM_HERE, base::Milliseconds(15),
                  base::BindRepeating(&CheckAnimation, browser(), &loop));
      loop.Run();
    }
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
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list_;
#endif

  // We make the histogram tester a member field to make sure it starts
  // recording as early as possible.
  base::HistogramTester histogram_tester_;
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
                                kPersistBookmarkTitle);
}

#if BUILDFLAG(IS_WIN)
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
  ASSERT_EQ(kPersistBookmarkTitle, urls[0].title);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // No multi-profile on ChromeOS.

// Sanity check that bookmarks from different profiles are separate.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MultiProfile) {
  BookmarkModel* bookmark_model1 = WaitForBookmarkModel(browser()->profile());

  base::RunLoop run_loop;
  Profile* profile2 = nullptr;
  g_browser_process->profile_manager()->CreateMultiProfileAsync(
      u"New Profile", 0, false,
      base::BindLambdaForTesting(
          [&](Profile* profile, Profile::CreateStatus status) {
            if (status == Profile::CREATE_STATUS_INITIALIZED) {
              profile2 = profile;
              run_loop.Quit();
            }
          }));
  run_loop.Run();
  BookmarkModel* bookmark_model2 = WaitForBookmarkModel(profile2);

  bookmarks::AddIfNotBookmarked(bookmark_model1, GURL(kPersistBookmarkURL),
                                kPersistBookmarkTitle);
  std::vector<UrlAndTitle> urls1, urls2;
  bookmark_model1->GetBookmarks(&urls1);
  bookmark_model2->GetBookmarks(&urls2);
  ASSERT_EQ(1u, urls1.size());
  ASSERT_TRUE(urls2.empty());
}

#endif

// Sanity check that bookmarks from Incognito mode persist Incognito restart.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, IncognitoPersistence) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  BookmarkModel* bookmark_model =
      WaitForBookmarkModel(incognito_browser->profile());

  // Add bookmark for Incognito and ensure it is added.
  bookmarks::AddIfNotBookmarked(bookmark_model, GURL(kPersistBookmarkURL),
                                kPersistBookmarkTitle);

  std::vector<UrlAndTitle> urls;
  bookmark_model->GetBookmarks(&urls);
  ASSERT_EQ(1u, urls.size());

  // Restart Incognito, and check again.
  CloseBrowserSynchronously(incognito_browser);
  incognito_browser = CreateIncognitoBrowser();
  bookmark_model = WaitForBookmarkModel(incognito_browser->profile());
  urls.clear();
  bookmark_model->GetBookmarks(&urls);
  ASSERT_EQ(1u, urls.size());

  // Ensure it is also available in regular mode.
  bookmark_model = WaitForBookmarkModel(browser()->profile());
  urls.clear();
  bookmark_model->GetBookmarks(&urls);
  ASSERT_EQ(1u, urls.size());
}

// Regression for crash caused by opening folder as a group in an incognito
// window when the folder contains URLs that cannot be displayed in incognito.
// See discussion starting at crbug.com/1242351#c15
IN_PROC_BROWSER_TEST_F(
    BookmarkBrowsertest,
    OpenFolderAsGroupInIncognitoWhenBookmarksCantOpenInIncognito) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  const BookmarkNode* const folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Folder");
  const BookmarkNode* const page1 = bookmark_model->AddURL(
      folder, 0, u"BookmarkManager", GURL(chrome::kChromeUIBookmarksURL));
  const BookmarkNode* const page2 = bookmark_model->AddURL(
      folder, 1, u"Settings", GURL(chrome::kChromeUISettingsURL));

  Browser* incognito_browser = CreateIncognitoBrowser();
  BookmarkModel* incognito_model =
      WaitForBookmarkModel(incognito_browser->profile());
  ASSERT_FALSE(incognito_model->root_node()->children().empty());
  ASSERT_TRUE(incognito_model->root_node()->children()[0]->is_folder());
  BookmarkNode* const incognito_folder =
      incognito_model->bookmark_bar_node()->children()[0].get();
  ASSERT_EQ(2U, incognito_folder->children().size());
  EXPECT_EQ(page1->url(), incognito_folder->children()[0]->url());
  EXPECT_EQ(page2->url(), incognito_folder->children()[1]->url());

  const int browser_tabs = browser()->tab_strip_model()->GetTabCount();
  const int incognito_tabs =
      incognito_browser->tab_strip_model()->GetTabCount();

  chrome::OpenAllIfAllowed(
      incognito_browser, base::BindLambdaForTesting([=]() {
        return static_cast<content::PageNavigator*>(incognito_browser);
      }),
      {incognito_folder}, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      /* add_to_group =*/true);

  EXPECT_EQ(incognito_tabs,
            incognito_browser->tab_strip_model()->GetTabCount());
  EXPECT_EQ(browser_tabs + 2, browser()->tab_strip_model()->GetTabCount());
}

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
  bookmarks::AddIfNotBookmarked(bookmark_model, bookmark_url, u"Bookmark");

  TestBookmarkTabHelperObserver bookmark_observer;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  tab_helper->AddObserver(&bookmark_observer);

  // Go to a bookmarked url. Bookmark star should show.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bookmark_url));
  EXPECT_FALSE(IsShowingInterstitial(web_contents));
  EXPECT_TRUE(bookmark_observer.is_starred());
  // Now go to a non-bookmarked url which triggers an SSL warning. Bookmark
  // star should disappear.
  GURL error_url = https_server.GetURL("/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsShowingInterstitial(web_contents));
  EXPECT_FALSE(bookmark_observer.is_starred());

  tab_helper->RemoveObserver(&bookmark_observer);
}

// Provides coverage for the Bookmark Manager bookmark drag and drag image
// generation for dragging a single bookmark.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DragSingleBookmark) {
  BookmarkModel* model = WaitForBookmarkModel(browser()->profile());
  const std::u16string page_title(u"foo");
  const GURL page_url("http://www.google.com");
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* node = model->AddURL(root, 0, page_title, page_url);
  const gfx::Point expected_point(100, 100);

  auto run_loop = std::make_unique<base::RunLoop>();

  chrome::DoBookmarkDragCallback cb = base::BindLambdaForTesting(
      [&run_loop, page_title, page_url, expected_point](
          std::unique_ptr<ui::OSExchangeData> drag_data,
          gfx::NativeView native_view, ui::mojom::DragEventSource source,
          gfx::Point point, int operation) {
        GURL url;
        std::u16string title;
        EXPECT_TRUE(drag_data->provider().GetURLAndTitle(
            ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES, &url, &title));
        EXPECT_EQ(page_url, url);
        EXPECT_EQ(page_title, title);
#if !BUILDFLAG(IS_WIN)
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
       browser()->tab_strip_model()->GetActiveWebContents(),
       ui::mojom::DragEventSource::kMouse,
       expected_point},
      std::move(cb));

  run_loop->Run();
}

// Provides coverage for the Bookmark Manager bookmark drag and drag image
// generation for dragging multiple bookmarks.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DragMultipleBookmarks) {
  BookmarkModel* model = WaitForBookmarkModel(browser()->profile());
  const std::u16string page_title(u"foo");
  const GURL page_url("http://www.google.com");
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* node1 = model->AddURL(root, 0, page_title, page_url);
  const BookmarkNode* node2 = model->AddFolder(root, 0, page_title);
  const gfx::Point expected_point(100, 100);

  auto run_loop = std::make_unique<base::RunLoop>();

  chrome::DoBookmarkDragCallback cb = base::BindLambdaForTesting(
      [&run_loop, expected_point](std::unique_ptr<ui::OSExchangeData> drag_data,
                                  gfx::NativeView native_view,
                                  ui::mojom::DragEventSource source,
                                  gfx::Point point, int operation) {
#if !BUILDFLAG(IS_MAC)
        GURL url;
        std::u16string title;
        // On Mac 10.11 and 10.12, this returns true, even though we set no url.
        // See https://crbug.com/893432.
        EXPECT_FALSE(drag_data->provider().GetURLAndTitle(
            ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES, &url, &title));
#endif
#if !BUILDFLAG(IS_WIN)
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
  chrome::DragBookmarksForTest(
      browser()->profile(),
      {
          {node1, node2},
          kDragNodeIndex,
          browser()->tab_strip_model()->GetActiveWebContents(),
          ui::mojom::DragEventSource::kMouse,
          expected_point,
      },
      std::move(cb));

  run_loop->Run();
}

// ChromeOS initializes two profiles (Default and test-user) and it's impossible
// to distinguish UMA samples separately.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_EmitUmaForDuplicates) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  const BookmarkNode* parent = bookmarks::GetParentForNewNodes(bookmark_model);
  const BookmarkNode* other_parent =
      bookmark_model->AddFolder(parent, 0, u"Folder");

  // Add one bookmark with a unique URL, two other bookmarks with a shared URL,
  // and three more with another shared URL.
  bookmark_model->AddURL(parent, parent->children().size(), u"title1",
                         GURL("http://a.com"));
  bookmark_model->AddURL(parent, parent->children().size(), u"title2",
                         GURL("http://b.com"));
  bookmark_model->AddURL(parent, parent->children().size(), u"title3",
                         GURL("http://b.com"));
  bookmark_model->AddURL(parent, parent->children().size(), u"title4",
                         GURL("http://c.com"));
  bookmark_model->AddURL(parent, parent->children().size(), u"title5",
                         GURL("http://c.com"));
  bookmark_model->AddURL(parent, parent->children().size(), u"title5",
                         GURL("http://c.com"));
  bookmark_model->AddURL(other_parent, other_parent->children().size(),
                         u"title5", GURL("http://c.com"));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, EmitUmaForDuplicates) {
  WaitForBookmarkModel(browser()->profile());

  // The total number of bookmarks is 7, but it gets rounded down due to
  // bucketing.
  ASSERT_THAT(
      histogram_tester()->GetAllSamples("Bookmarks.Count.OnProfileLoad"),
      testing::ElementsAre(base::Bucket(/*min=*/6, /*count=*/1)));

  // 2 bookmarks have URL http://b.com and 4 have http://c.com. This counts as 4
  // duplicates.
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.DuplicateUrl2"),
              testing::ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  // 3 bookmarks have the pair (http://c.com, title5). This counts as 2
  // duplicates when considering URLs and titles.
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.DuplicateUrlAndTitle"),
              testing::ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  // Among the three above, only two have the same parent. This means only one
  // counts as duplicate when considering all three attributes.
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Count.OnProfileLoad.DuplicateUrlAndTitleAndParent"),
      testing::ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  // The remaining histograms are the result of substracting the number of
  // duplicates from the total, which is 7 despite the bucket for the first
  // histogram above suggesting 6.
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.UniqueUrl"),
              testing::ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.UniqueUrlAndTitle"),
              testing::ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Count.OnProfileLoad.UniqueUrlAndTitleAndParent"),
              testing::ElementsAre(base::Bucket(/*min=*/6, /*count=*/1)));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class BookmarkPrerenderBrowsertest : public BookmarkBrowsertest {
 public:
  BookmarkPrerenderBrowsertest()
      : prerender_helper_(
            base::BindRepeating(&BookmarkPrerenderBrowsertest::GetWebContents,
                                base::Unretained(this))) {}
  ~BookmarkPrerenderBrowsertest() override = default;
  BookmarkPrerenderBrowsertest(const BookmarkPrerenderBrowsertest&) = delete;

  BookmarkPrerenderBrowsertest& operator=(const BookmarkPrerenderBrowsertest&) =
      delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    BookmarkBrowsertest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    BookmarkBrowsertest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(BookmarkPrerenderBrowsertest,
                       PrerenderingShouldNotUpdateStarredState) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  GURL bookmark_url = embedded_test_server()->GetURL("/title1.html");
  bookmarks::AddIfNotBookmarked(bookmark_model, bookmark_url, u"Bookmark");

  TestBookmarkTabHelperObserver bookmark_observer;
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(GetWebContents());
  tab_helper->AddObserver(&bookmark_observer);

  // Load a prerender page and prerendering should not notify to
  // URLStarredChanged listener.
  const int host_id = prerender_test_helper().AddPrerender(bookmark_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_FALSE(bookmark_observer.is_starred());

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(bookmark_url);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(bookmark_observer.is_starred());

  tab_helper->RemoveObserver(&bookmark_observer);
}
