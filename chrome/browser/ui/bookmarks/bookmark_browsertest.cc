// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
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
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
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
const base::Time kPersistLastUsedTime =
    base::Time() + base::Days(7) + base::Hours(2) + base::Minutes(55) +
    base::Seconds(24) + base::Milliseconds(133);

}  // namespace

class TestBookmarkTabHelperObserver : public BookmarkTabHelperObserver {
 public:
  explicit TestBookmarkTabHelperObserver(BookmarkTabHelper* helper) {
    observation_.Observe(helper);
  }

  TestBookmarkTabHelperObserver(const TestBookmarkTabHelperObserver&) = delete;
  TestBookmarkTabHelperObserver& operator=(
      const TestBookmarkTabHelperObserver&) = delete;

  ~TestBookmarkTabHelperObserver() override = default;

  void URLStarredChanged(content::WebContents*, bool starred) override {
    starred_ = starred;
  }
  bool is_starred() const { return starred_; }

 private:
  base::ScopedObservation<BookmarkTabHelper, BookmarkTabHelperObserver>
      observation_{this};

  bool starred_ = false;
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

  const BookmarkNode* node = bookmarks::AddIfNotBookmarked(
      bookmark_model, GURL(kPersistBookmarkURL), kPersistBookmarkTitle);
  bookmark_model->UpdateLastUsedTime(node, kPersistLastUsedTime,
                                     /*just_opened=*/true);
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/41443454): The test fails on Windows.
#define MAYBE_Persist DISABLED_Persist
#else
#define MAYBE_Persist Persist
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MAYBE_Persist) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());

  GURL url(kPersistBookmarkURL);
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      bookmark_model->GetNodesByURL(url);

  ASSERT_EQ(1u, nodes.size());
  ASSERT_EQ(url, nodes[0]->url());
  ASSERT_EQ(kPersistBookmarkTitle, nodes[0]->GetTitledUrlNodeTitle());
  EXPECT_EQ(kPersistLastUsedTime, nodes[0]->date_last_used());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)  // No multi-profile on ChromeOS.

// Sanity check that bookmarks from different profiles are separate.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, MultiProfile) {
  BookmarkModel* bookmark_model1 = WaitForBookmarkModel(browser()->profile());

  base::RunLoop run_loop;
  Profile* profile2 = nullptr;
  g_browser_process->profile_manager()->CreateMultiProfileAsync(
      u"New Profile", 0, false,
      base::BindLambdaForTesting([&](Profile* profile) {
        if (profile) {
          profile2 = profile;
          run_loop.Quit();
        }
      }));
  run_loop.Run();
  BookmarkModel* bookmark_model2 = WaitForBookmarkModel(profile2);

  bookmarks::AddIfNotBookmarked(bookmark_model1, GURL(kPersistBookmarkURL),
                                kPersistBookmarkTitle);

  ASSERT_EQ(1u, bookmark_model1->GetUniqueUrls().size());
  ASSERT_TRUE(bookmark_model2->GetUniqueUrls().empty());
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

  ASSERT_EQ(1u, bookmark_model->GetUniqueUrls().size());

  // Restart Incognito, and check again.
  CloseBrowserSynchronously(incognito_browser);
  incognito_browser = CreateIncognitoBrowser();
  bookmark_model = WaitForBookmarkModel(incognito_browser->profile());
  ASSERT_EQ(1u, bookmark_model->GetUniqueUrls().size());

  // Ensure it is also available in regular mode.
  bookmark_model = WaitForBookmarkModel(browser()->profile());
  ASSERT_EQ(1u, bookmark_model->GetUniqueUrls().size());
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

  chrome::OpenAllIfAllowed(incognito_browser, {incognito_folder},
                           WindowOpenDisposition::NEW_BACKGROUND_TAB,
                           /* add_to_group =*/true);

  EXPECT_EQ(incognito_tabs,
            incognito_browser->tab_strip_model()->GetTabCount());
  EXPECT_EQ(browser_tabs + 2, browser()->tab_strip_model()->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, OpenAllBookmarks) {
  Browser* regular_browser = browser();
  BookmarkModel* bookmark_model =
      WaitForBookmarkModel(regular_browser->profile());
  const BookmarkNode* const bbar = bookmark_model->bookmark_bar_node();
  ASSERT_TRUE(bbar->children().empty());

  Browser* incognito_browser = CreateIncognitoBrowser();
  BookmarkModel* incognito_model =
      WaitForBookmarkModel(incognito_browser->profile());
  const BookmarkNode* const incognito_bbar =
      incognito_model->bookmark_bar_node();

  auto close_all_tabs_except_first = [](Browser* browser) {
    int num_tabs = browser->tab_strip_model()->GetTabCount();
    for (int i = 0; i < num_tabs - 1; ++i) {
      browser->tab_strip_model()->CloseWebContentsAt(num_tabs - 1 - i, 0);
    }
    EXPECT_EQ(1, browser->tab_strip_model()->count());
  };

  auto open_urls_and_test = [&regular_browser, &incognito_browser, &bbar,
                             &close_all_tabs_except_first, this]() {
    // open all in new tab from regular browser
    {
      close_all_tabs_except_first(regular_browser);
      close_all_tabs_except_first(incognito_browser);
      chrome::OpenAllIfAllowed(regular_browser, {bbar},
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               false);
      int num_tabs_regular = regular_browser->tab_strip_model()->GetTabCount();
      int num_tabs_incognito =
          incognito_browser->tab_strip_model()->GetTabCount();
      EXPECT_EQ(num_tabs_regular, 5);
      EXPECT_EQ(num_tabs_incognito, 1);
    }

    // open all in a new window from regular browser
    {
      close_all_tabs_except_first(regular_browser);
      close_all_tabs_except_first(incognito_browser);
      chrome::OpenAllIfAllowed(regular_browser, {bbar},
                               WindowOpenDisposition::NEW_WINDOW, false);
      Browser* regular_browser2 = nullptr;
      for (Browser* browser_instance : *BrowserList::GetInstance()) {
        if (browser_instance != incognito_browser &&
            browser_instance != regular_browser)
          regular_browser2 = browser_instance;
      }
      // new browser needs to be opened
      EXPECT_NE(regular_browser2, nullptr);
      int num_tabs_regular = regular_browser->tab_strip_model()->GetTabCount();
      int num_tabs_regular2 =
          regular_browser2->tab_strip_model()->GetTabCount();
      EXPECT_EQ(num_tabs_regular, 1);
      EXPECT_EQ(num_tabs_regular2, 4);
      CloseBrowserSynchronously(regular_browser2);
    }

    // open all in a new incognito window from regular browser
    {
      close_all_tabs_except_first(regular_browser);
      close_all_tabs_except_first(incognito_browser);
      chrome::OpenAllIfAllowed(regular_browser, {bbar},
                               WindowOpenDisposition::OFF_THE_RECORD, false);
      int num_tabs_incognito =
          incognito_browser->tab_strip_model()->GetTabCount();
      EXPECT_EQ(num_tabs_incognito, 3);
    }
  };

  auto open_urls_from_incognito_and_test = [&regular_browser,
                                            &incognito_browser, &incognito_bbar,
                                            &close_all_tabs_except_first,
                                            this]() {
    // open all in new tab from incognito
    {
      close_all_tabs_except_first(regular_browser);
      close_all_tabs_except_first(incognito_browser);
      chrome::OpenAllIfAllowed(incognito_browser, {incognito_bbar},
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               false);
      int num_tabs_regular = regular_browser->tab_strip_model()->GetTabCount();
      int num_tabs_incognito =
          incognito_browser->tab_strip_model()->GetTabCount();
      EXPECT_EQ(num_tabs_regular, 3);
      EXPECT_EQ(num_tabs_incognito, 3);
    }

    // open all in new window from incognito
    {
      close_all_tabs_except_first(regular_browser);
      close_all_tabs_except_first(incognito_browser);
      chrome::OpenAllIfAllowed(incognito_browser, {incognito_bbar},
                               WindowOpenDisposition::NEW_WINDOW, false);
      Browser* incognito_browser2 = nullptr;
      for (Browser* browser_instance : *BrowserList::GetInstance()) {
        if (browser_instance != incognito_browser &&
            browser_instance != regular_browser)
          incognito_browser2 = browser_instance;
      }
      // new browser needs to be opened
      EXPECT_NE(incognito_browser2, nullptr);
      int num_tabs_regular = regular_browser->tab_strip_model()->GetTabCount();
      int num_tabs_incognito =
          incognito_browser->tab_strip_model()->GetTabCount();
      int num_tabs_incognito2 =
          incognito_browser2->tab_strip_model()->GetTabCount();
      EXPECT_EQ(num_tabs_regular, 3);
      EXPECT_EQ(num_tabs_incognito, 1);
      EXPECT_EQ(num_tabs_incognito2, 2);
      CloseBrowserSynchronously(incognito_browser2);
    }
  };

  {
    // Bookmark 4 pages, with the first and third one not being able to be
    // opened in incognito mode
    bookmark_model->AddURL(bbar, 0, u"Settings",
                           GURL(chrome::kChromeUISettingsURL));
    bookmark_model->AddURL(bbar, 1, u"Google", GURL("http://www.google.com"));
    bookmark_model->AddURL(bbar, 2, u"Extensions",
                           GURL(chrome::kChromeUIExtensionsURL));
    bookmark_model->AddURL(bbar, 3, u"Gmail", GURL("http://mail.google.com"));
    open_urls_and_test();
    open_urls_from_incognito_and_test();
    bookmark_model->RemoveAllUserBookmarks(FROM_HERE);
  }
  {
    // Bookmark 4 pages, with the second and fourth one not being able to be
    // opened in incognito mode
    bookmark_model->AddURL(bbar, 0, u"Google", GURL("http://www.google.com"));
    bookmark_model->AddURL(bbar, 1, u"Settings",
                           GURL(chrome::kChromeUISettingsURL));
    bookmark_model->AddURL(bbar, 2, u"Gmail", GURL("http://mail.google.com"));
    bookmark_model->AddURL(bbar, 3, u"Extensions",
                           GURL(chrome::kChromeUIExtensionsURL));
    open_urls_and_test();
    open_urls_from_incognito_and_test();
    bookmark_model->RemoveAllUserBookmarks(FROM_HERE);
  }
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

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  TestBookmarkTabHelperObserver bookmark_observer(tab_helper);

  // Go to a bookmarked url. Bookmark star should show.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bookmark_url));
  EXPECT_FALSE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents));
  EXPECT_TRUE(bookmark_observer.is_starred());
  // Now go to a non-bookmarked url which triggers an SSL warning. Bookmark
  // star should disappear.
  GURL error_url = https_server.GetURL("/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(web_contents));
  EXPECT_FALSE(bookmark_observer.is_starred());
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
        std::optional<ui::OSExchangeData::UrlInfo> url_info =
            drag_data->GetURLAndTitle(
                ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
        ASSERT_TRUE(url_info.has_value());
        EXPECT_EQ(page_url, url_info->url);
        EXPECT_EQ(page_title, url_info->title);
#if !BUILDFLAG(IS_WIN)
        // On Windows, GetDragImage() is a NOTREACHED() as the Windows
        // implementation of OSExchangeData just sets the drag image on the OS
        // API. https://crbug.com/893388
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

// A favicon update during drag shouldn't trigger the drag flow again. The test
// passes if the favicon update does not cause a crash. (see
// https://crbug.com/1364056)
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, FaviconChangeDuringBookmarkDrag) {
  BookmarkModel* model = WaitForBookmarkModel(browser()->profile());
  const std::u16string kPageTitle(u"foo");
  const GURL kPageUrl("http://www.google.com");
  const GURL kFaviconUrl("http://www.google.com/favicon.ico");
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* node = model->AddURL(root, 0, kPageTitle, kPageUrl);
  constexpr gfx::Point kExpectedPoint(100, 100);

  auto run_loop = std::make_unique<base::RunLoop>();

  chrome::DoBookmarkDragCallback cb = base::BindLambdaForTesting(
      [&run_loop, model, kPageUrl, kFaviconUrl](
          std::unique_ptr<ui::OSExchangeData> drag_data,
          gfx::NativeView native_view, ui::mojom::DragEventSource source,
          gfx::Point point, int operation) {
        // Simulate a favicon change during the drag operation.
        model->OnFaviconsChanged({kPageUrl}, kFaviconUrl);
        run_loop->Quit();
      });

  constexpr int kDragNodeIndex = 0;
  chrome::DragBookmarksForTest(
      browser()->profile(),
      {{node},
       kDragNodeIndex,
       browser()->tab_strip_model()->GetActiveWebContents(),
       ui::mojom::DragEventSource::kMouse,
       kExpectedPoint},
      std::move(cb));

  run_loop->Run();
}

// Provides coverage for the Bookmark Manager bookmark drag and drag image
// generation for dragging multiple bookmarks.
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, DragMultipleBookmarks) {
  BookmarkModel* model = WaitForBookmarkModel(browser()->profile());
  const std::u16string page_title = u"foo";
  const GURL page_url("http://www.google.com");
  const BookmarkNode* root = model->bookmark_bar_node();
  const BookmarkNode* node1 = model->AddURL(root, 0, page_title, page_url);
  const BookmarkNode* node2 = model->AddFolder(root, 0, page_title);
  const gfx::Point expected_point(100, 100);

  auto run_loop = std::make_unique<base::RunLoop>();

  chrome::DoBookmarkDragCallback cb = base::BindLambdaForTesting(
      [&run_loop, expected_point, page_title, page_url](
          std::unique_ptr<ui::OSExchangeData> drag_data,
          gfx::NativeView native_view, ui::mojom::DragEventSource source,
          gfx::Point point, int operation) {
#if BUILDFLAG(IS_MAC)
        // On the Mac, the clipboard can hold multiple items, each with
        // different representations. Therefore, when the "write multiple URLs"
        // call is made, a full-fledged array of objects and types are written
        // to the clipboard, providing rich interoperability with the rest of
        // the OS and other apps. Then, when `GetURLAndTitle` is called, it
        // looks at the clipboard, sees URL and title data, and returns true.
        std::optional<ui::OSExchangeData::UrlInfo> url_info =
            drag_data->GetURLAndTitle(
                ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES);
        ASSERT_TRUE(url_info.has_value());

        // The bookmarks are added in order, and the first is retrieved, so
        // expect the values from the first bookmark.
        EXPECT_EQ(page_title, url_info->title);
        EXPECT_EQ(page_url, url_info->url);
#else
        // On other platforms, because they don't have the concept of multiple
        // items on the clipboard, single URLs are added as a URL, but multiple
        // URLs are added as a data blob opaque to the outside world. Then, when
        // `GetURLAndTitle` is called, it's unable to extract any single URL,
        // and returns false.
        EXPECT_FALSE(drag_data
                         ->GetURLAndTitle(
                             ui::FilenameToURLPolicy::DO_NOT_CONVERT_FILENAMES)
                         .has_value());
#endif
#if !BUILDFLAG(IS_WIN)
        // On Windows, GetDragImage() is a NOTREACHED() as the Windows
        // implementation of OSExchangeData just sets the drag image on the OS
        // API. https://crbug.com/893388
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

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_EmitUmaForTimeMetrics) {
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

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, EmitUmaForTimeMetrics) {
  WaitForBookmarkModel(browser()->profile());

  // The total number of bookmarks is 7, but it gets rounded down due to
  // bucketing.
  ASSERT_THAT(
      histogram_tester()->GetAllSamples("Bookmarks.Count.OnProfileLoad3"),
      testing::ElementsAre(base::Bucket(/*min=*/6, /*count=*/1)));

  EXPECT_THAT(histogram_tester()->GetAllSamples(
                  "Bookmarks.Times.OnProfileLoad.TimeSinceAdded3"),
              testing::ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, PRE_EmitUmaForMostRecentlyUsed) {
  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  BookmarkNode* parent = const_cast<BookmarkNode*>(bookmark_model->AddFolder(
      bookmarks::GetParentForNewNodes(bookmark_model), 0, u"Test Folder"));
  parent->set_date_added(base::Time::Now() - base::Days(3));

  BookmarkNode* node = const_cast<BookmarkNode*>(bookmark_model->AddURL(
      parent, parent->children().size(), u"title1", GURL("http://a.com")));
  node->set_date_added(base::Time::Now() - base::Days(2));
  node->set_date_last_used(base::Time::Now() - base::Days(1));

  // This shouldn't count towards metrics because there's another node which
  // is more recently saved/used.
  node = const_cast<BookmarkNode*>(bookmark_model->AddURL(
      parent, parent->children().size(), u"title1", GURL("http://a.com")));
  node->set_date_added(base::Time::Now() - base::Days(3));
  node->set_date_last_used(base::Time::Now() - base::Days(3));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, EmitUmaForMostRecentlyUsed) {
  WaitForBookmarkModel(browser()->profile());

  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Times.OnProfileLoad.MostRecentlyUsedBookmarkInDays"),
      testing::ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));

  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Times.OnProfileLoad.MostRecentlySavedBookmarkInDays"),
      testing::ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Times.OnProfileLoad.MostRecentlyAddedFolderInDays"),
      testing::ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest,
                       EmitUmaForMostRecentlyUsed_NoBookmarks) {
  WaitForBookmarkModel(browser()->profile());

  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Times.OnProfileLoad.MostRecentlyUsedBookmarkInDays"),
      testing::ElementsAre());

  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Times.OnProfileLoad.MostRecentlySavedBookmarkInDays"),
      testing::ElementsAre());
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "Bookmarks.Times.OnProfileLoad.MostRecentlyAddedFolderInDays"),
      testing::ElementsAre());
}

// Test that the bookmark star state updates in response to same document
// navigations that change the URL
IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, SameDocumentNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  GURL bookmark_url = embedded_test_server()->GetURL("/title1.html");
  bookmarks::AddIfNotBookmarked(bookmark_model, bookmark_url, u"Bookmark");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  TestBookmarkTabHelperObserver bookmark_observer(tab_helper);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bookmark_url));
  EXPECT_TRUE(bookmark_observer.is_starred());

  GURL same_document_url = embedded_test_server()->GetURL("/title1.html#test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), same_document_url));
  EXPECT_FALSE(bookmark_observer.is_starred());
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest,
                       DifferentDocumentNavigationWithoutFinishing) {
  ASSERT_TRUE(embedded_test_server()->Start());

  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  GURL bookmark_url = embedded_test_server()->GetURL("/title1.html");
  bookmarks::AddIfNotBookmarked(bookmark_model, bookmark_url, u"Bookmark");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  TestBookmarkTabHelperObserver bookmark_observer(tab_helper);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bookmark_url));
  EXPECT_TRUE(bookmark_observer.is_starred());

  // Navigate to the second page and check that the bookmark is not starred even
  // when the navigation is not finished.
  GURL different_document_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationManager manager(web_contents, different_document_url);
  web_contents->GetController().LoadURL(
      different_document_url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
      std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());
  EXPECT_FALSE(manager.was_committed());
  EXPECT_EQ(web_contents->GetVisibleURL(), different_document_url);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), bookmark_url);
  EXPECT_FALSE(bookmark_observer.is_starred());
}

IN_PROC_BROWSER_TEST_F(BookmarkBrowsertest, NonCommitURLNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  BookmarkModel* bookmark_model = WaitForBookmarkModel(browser()->profile());
  GURL bookmark_url = embedded_test_server()->GetURL("/title1.html");
  bookmarks::AddIfNotBookmarked(bookmark_model, bookmark_url, u"Bookmark");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents);
  TestBookmarkTabHelperObserver bookmark_observer(tab_helper);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bookmark_url));
  EXPECT_TRUE(bookmark_observer.is_starred());

  const GURL non_commit_url = embedded_test_server()->GetURL("/page204.html");
  content::TestNavigationManager manager(web_contents, non_commit_url);
  web_contents->GetController().LoadURL(non_commit_url, content::Referrer(),
                                        ui::PAGE_TRANSITION_TYPED,
                                        std::string());
  EXPECT_TRUE(manager.WaitForRequestStart());
  EXPECT_FALSE(manager.was_committed());
  EXPECT_EQ(web_contents->GetVisibleURL(), non_commit_url);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), bookmark_url);
  EXPECT_FALSE(bookmark_observer.is_starred());

  // Since the navigation did not commit, the last committed URL becomes the
  // visible URL again, so the starred state should be restored.
  ASSERT_TRUE(manager.WaitForNavigationFinished());
  EXPECT_FALSE(manager.was_committed());
  EXPECT_TRUE(bookmark_observer.is_starred());
}

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
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
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

  BookmarkTabHelper* tab_helper =
      BookmarkTabHelper::FromWebContents(GetWebContents());
  TestBookmarkTabHelperObserver bookmark_observer(tab_helper);

  // Load a prerender page and prerendering should not notify to
  // URLStarredChanged listener.
  const content::FrameTreeNodeId host_id =
      prerender_test_helper().AddPrerender(bookmark_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_FALSE(bookmark_observer.is_starred());

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(bookmark_url);
  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(bookmark_observer.is_starred());
}
