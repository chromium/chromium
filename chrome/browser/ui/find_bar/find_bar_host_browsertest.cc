// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/find_result_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

using base::ASCIIToUTF16;
using base::WideToUTF16;
using content::NavigationController;
using content::WebContents;

namespace {

const char kAnchorPage[] = "anchor.html";
const char kAnchor[] = "#chapter2";
const char kFramePage[] = "frames.html";
const char kFrameData[] = "framedata_general.html";
const char kUserSelectPage[] = "user-select.html";
const char kCrashPage[] = "crash_1341577.html";
const char kTooFewMatchesPage[] = "bug_1155639.html";
const char kLongTextareaPage[] = "large_textarea.html";
const char kPrematureEnd[] = "premature_end.html";
const char kMoveIfOver[] = "move_if_obscuring.html";
const char kBitstackCrash[] = "crash_14491.html";
const char kSelectChangesOrdinal[] = "select_changes_ordinal.html";
const char kStartAfterSelection[] = "start_after_selection.html";
const char kSimple[] = "simple.html";
const char kLinkPage[] = "link.html";

const bool kBack = false;
const bool kFwd = true;

const bool kIgnoreCase = false;
const bool kCaseSensitive = true;

const int kMoveIterations = 30;

}  // namespace

class FindInPageControllerTest : public InProcessBrowserTest {
 public:
  FindInPageControllerTest() { DisableFindBarAnimationsDuringTesting(true); }

 protected:
  void SetUpOnMainThread() override {
    views::test::WaitForAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->pinned_toolbar_actions_container());
  }

  bool GetFindBarWindowInfoForBrowser(
      Browser* browser, gfx::Point* position, bool* fully_visible) {
    const FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetFindBarWindowInfo(position, fully_visible);
  }

  bool GetFindBarWindowInfo(gfx::Point* position, bool* fully_visible) {
    return GetFindBarWindowInfoForBrowser(browser(), position, fully_visible);
  }

  std::u16string GetFindBarTextForBrowser(Browser* browser) {
    FindBar* find_bar = browser->GetFindBarController()->find_bar();
    return find_bar->GetFindText();
  }

  std::u16string GetFindBarText() {
    return GetFindBarTextForBrowser(browser());
  }

  std::u16string GetFindBarMatchCountTextForBrowser(Browser* browser) {
    const FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetMatchCountText();
  }

  std::u16string GetMatchCountText() {
    return GetFindBarMatchCountTextForBrowser(browser());
  }

  int GetFindBarWidthForBrowser(Browser* browser) {
    const FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetContentsWidth();
  }

  size_t GetFindBarAudibleAlertsForBrowser(Browser* browser) {
    const FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetAudibleAlertCount();
  }

  void EnsureFindBoxOpenForBrowser(Browser* browser) {
    chrome::Find(browser);
    gfx::Point position;
    bool fully_visible = false;
    EXPECT_TRUE(GetFindBarWindowInfoForBrowser(
                    browser, &position, &fully_visible));
    EXPECT_TRUE(fully_visible);
  }

  void EnsureFindBoxOpen() {
    EnsureFindBoxOpenForBrowser(browser());
  }

  int FindNext(WebContents* web_contents, int* ordinal) {
    browser()->GetFindBarController()->Show(true /*find_next*/);
    ui_test_utils::FindResultWaiter observer(web_contents);
    observer.Wait();
    if (ordinal) {
      *ordinal = observer.active_match_ordinal();
    }
    return observer.number_of_matches();
  }

  int FindInPage16(WebContents* web_contents,
                   const std::u16string& search_str,
                   bool forward,
                   bool case_sensitive,
                   int* ordinal) {
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    browser->GetFindBarController()->find_bar()->SetFindTextAndSelectedRange(
        search_str, gfx::Range());
    return ui_test_utils::FindInPage(web_contents, search_str, forward,
                                     case_sensitive, ordinal, nullptr);
  }

  int FindInPageASCII(WebContents* web_contents,
                      const std::string& search_str,
                      bool forward,
                      bool case_sensitive,
                      int* ordinal) {
    return FindInPage16(web_contents, ASCIIToUTF16(search_str), forward,
                        case_sensitive, ordinal);
  }

  // Calls FindInPageASCII till the find box's x position != |start_x_position|.
  // Return |start_x_position| if the find box has not moved after iterating
  // through all matches of |search_str|.
  int FindInPageTillBoxMoves(WebContents* web_contents,
                             int start_x_position,
                             const std::string& search_str,
                             int expected_matches) {
    // Search for |search_str| which the Find box is obscuring.
    for (int index = 0; index < expected_matches; ++index) {
      int ordinal = 0;
      EXPECT_EQ(expected_matches, FindInPageASCII(web_contents, search_str,
                                                  kFwd, kIgnoreCase, &ordinal));

      // Check the position.
      bool fully_visible;
      gfx::Point position;
      EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
      EXPECT_TRUE(fully_visible);

      // If the Find box has moved then we are done.
      if (position.x() != start_x_position)
        return position.x();
    }
    return start_x_position;
  }

  GURL GetURL(const std::string& filename) {
    return ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("find_in_page"),
        base::FilePath().AppendASCII(filename));
  }
};

// This test loads a page with frames and starts FindInPage requests.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageFrames) {
  // First we navigate to our frames page.
  GURL url = GetURL(kFramePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Try incremental search (mimicking user typing in).
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));

  EXPECT_EQ(18, FindInPageASCII(web_contents, "g",
                                kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(11, FindInPageASCII(web_contents, "go",
                                kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(4, FindInPageASCII(web_contents, "goo",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(3, FindInPageASCII(web_contents, "goog",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(2, FindInPageASCII(web_contents, "googl",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(1, FindInPageASCII(web_contents, "google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));

  EXPECT_EQ(
      0, FindInPageASCII(web_contents, "google!", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
  EXPECT_EQ(1u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Extend the search string one more.
  EXPECT_EQ(0, FindInPageASCII(web_contents, "google!!", kFwd, kIgnoreCase,
                               &ordinal));
  EXPECT_EQ(0, ordinal);
  EXPECT_EQ(2u, GetFindBarAudibleAlertsForBrowser(browser()));

  // "Backspace" one, make sure there's no audible alert while backspacing.
  EXPECT_EQ(0, FindInPageASCII(web_contents, "google!",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
  EXPECT_EQ(2u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Negative test (no matches should be found).
  EXPECT_EQ(0, FindInPageASCII(web_contents, "Non-existing string",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);

  // 'horse' only exists in the three right frames.
  EXPECT_EQ(3, FindInPageASCII(web_contents, "horse",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // 'cat' only exists in the first frame.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "cat",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try searching again, should still come up with 1 match.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "cat",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try searching backwards, ignoring case, should still come up with 1 match.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "CAT",
                               kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try case sensitive, should NOT find it.
  EXPECT_EQ(0, FindInPageASCII(web_contents, "CAT",
                               kFwd, kCaseSensitive, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Try again case sensitive, but this time with right case.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "dog",
                               kFwd, kCaseSensitive, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Try non-Latin characters ('Hreggvidur' with 'eth' for 'd' in left frame).
  EXPECT_EQ(1, FindInPage16(web_contents, u"Hreggvi\u00F0ur", kFwd, kIgnoreCase,
                            &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(1, FindInPage16(web_contents, u"Hreggvi\u00F0ur", kFwd,
                            kCaseSensitive, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPage16(web_contents, u"hreggvi\u00F0ur", kFwd,
                            kCaseSensitive, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Verify search for text within various forms and text areas.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageFormsTextAreas) {
  std::vector<GURL> urls;
  urls.push_back(GetURL("textintextarea.html"));
  urls.push_back(GetURL("smalltextarea.html"));
  urls.push_back(GetURL("populatedform.html"));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  for (size_t i = 0; i < urls.size(); ++i) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), urls[i]));
    EXPECT_EQ(1,
              FindInPageASCII(web_contents, "cat", kFwd, kIgnoreCase, nullptr));
    EXPECT_EQ(0,
              FindInPageASCII(web_contents, "bat", kFwd, kIgnoreCase, nullptr));
  }
}

// This test removes a frame after a search comes up empty, then navigates to
// a different page and verifies this doesn't cause any extraneous dings.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, NoAudibleAlertOnFrameChange) {
  // First we navigate to our frames page.
  GURL url = GetURL(kFramePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Search for a non-existant string.
  EXPECT_EQ(
      0, FindInPageASCII(web_contents, "google!", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
  EXPECT_EQ(1u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Remove the first frame of the page.
  constexpr char kRemoveFrameScript[] =
      "frame = document.getElementsByTagName(\"FRAME\")[0];\n"
      "frame.parentElement.removeChild(frame);\n";
  ASSERT_TRUE(content::ExecJs(web_contents, kRemoveFrameScript));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("specialchar.html")));

  EXPECT_EQ(1u, GetFindBarAudibleAlertsForBrowser(browser()));
}

// This test navigates to a different page after a successful search and
// verifies this doesn't cause any extraneous dings.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, NoAudibleAlertOnNavigation) {
  // First we navigate to our frames page.
  GURL url = GetURL(kFramePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Search for a string in the page.
  EXPECT_EQ(
      1, FindInPageASCII(web_contents, "google", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Navigate to a different page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("specialchar.html")));

  // Ensure that there was no audible alert.
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));
}

// See http://crbug.com/1131780
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       AudibleAlertsWithPrepopulatedFind) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL(kSimple)));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(0u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Do an initial find so the text will be prepopulated for the next one.
  // This will produce an audible alert.
  EXPECT_EQ(
      0, FindInPageASCII(web_contents, "zzz", kFwd, kIgnoreCase, nullptr));
  EXPECT_EQ(1u, GetFindBarAudibleAlertsForBrowser(browser()));
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Now show the findbar (prepopulated) and ensure there's no alert.
  browser()->GetFindBarController()->Show(false /*find_next*/);
  EXPECT_EQ(u"zzz", GetFindBarText());
  ui_test_utils::FindResultWaiter observer1(web_contents);
  observer1.Wait();
  EXPECT_EQ(0, observer1.number_of_matches());
  EXPECT_EQ(1u, GetFindBarAudibleAlertsForBrowser(browser()));

  // Now do a find-next and ensure there *is* an alert
  browser()->GetFindBarController()->Show(true /*find_next*/);
  EXPECT_EQ(u"zzz", GetFindBarText());
  ui_test_utils::FindResultWaiter observer2(web_contents);
  observer2.Wait();
  EXPECT_EQ(0, observer2.number_of_matches());
  EXPECT_EQ(2u, GetFindBarAudibleAlertsForBrowser(browser()));
}

// Verify search selection coordinates. The data file used is set-up such that
// the text occurs on the same line, and we verify their positions by verifying
// their relative positions.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageSpecialURLs) {
  const std::wstring search_string(L"\u5728\u897f\u660c\u536b\u661f\u53d1");
  gfx::Rect first, second, first_reverse;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("specialchar.html")));
  ui_test_utils::FindInPage(web_contents, WideToUTF16(search_string), kFwd,
                            kIgnoreCase, nullptr, &first);
  ui_test_utils::FindInPage(web_contents, WideToUTF16(search_string), kFwd,
                            kIgnoreCase, nullptr, &second);

  // We have search occurrence in the same row, so top-bottom coordinates should
  // be the same even for second search.
  ASSERT_EQ(first.y(), second.y());
  ASSERT_EQ(first.bottom(), second.bottom());
  ASSERT_LT(first.x(), second.x());
  ASSERT_LT(first.right(), second.right());

  ui_test_utils::FindInPage(web_contents, WideToUTF16(search_string), kBack,
                            kIgnoreCase, nullptr, &first_reverse);
  // We find next and we go back so find coordinates should be the same as
  // previous ones.
  ASSERT_EQ(first, first_reverse);
}

// This tests the following bug that used to exist:
// 1) Do a find that has 0 results
// 2) Navigate to a new page (on the same domain) that contains the search text.
// 3) Open the find bar. It will be prepopulated with the previous search text
// and should show the number of matches for that text. The bug caused it to
// show 0 matches instead.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, StaleCountAfterNoResults) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("simple.html")));
  EXPECT_EQ(0, FindInPageASCII(web_contents, "link", kFwd, kIgnoreCase,
                               nullptr));
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("link.html")));
  browser()->GetFindBarController()->Show();
  ui_test_utils::FindResultWaiter observer(web_contents);
  observer.Wait();
  EXPECT_EQ(1, observer.number_of_matches());
  EXPECT_EQ(0, observer.active_match_ordinal());
}

// Verifies that comments and meta data are not searchable.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       CommentsAndMetaDataNotSearchable) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("specialchar.html")));

  const std::wstring search_string =
      L"\u4e2d\u65b0\u793e\u8bb0\u8005\u5b8b\u5409\u6cb3\u6444\u4e2d\u65b0\u7f51";
  EXPECT_EQ(0,
            ui_test_utils::FindInPage(web_contents, WideToUTF16(search_string),
                                      kFwd, kIgnoreCase, nullptr, nullptr));
}

// Verifies that span are searchable.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, SpanSearchable) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("FindRandomTests.html")));

  std::string search_string = "has light blue eyes and my father has dark";
  EXPECT_EQ(1,
            ui_test_utils::FindInPage(web_contents, ASCIIToUTF16(search_string),
                                      kFwd, kIgnoreCase, nullptr, nullptr));
}

// Find in a very large page.
// TODO(crbug.com/40129326): Test is flaky on Mac debug builds.
#if BUILDFLAG(IS_MAC) && !defined(NDEBUG)
#define MAYBE_LargePage DISABLED_LargePage
#elif BUILDFLAG(IS_LINUX) && (!defined(NDEBUG) || defined(ADDRESS_SANITIZER))
// TODO(crbug.com/40751034): Test is flaky on Linux debug builds.
// TODO(crbug.com/40760850): Test is flaky on Linux ASAN builds.
#define MAYBE_LargePage DISABLED_LargePage
#else
#define MAYBE_LargePage LargePage
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_LargePage) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("largepage.html")));

  EXPECT_EQ(373, FindInPageASCII(web_contents, "daughter of Prince", kFwd,
                                 kIgnoreCase, nullptr));
}

// Find a very long string in a large page.
// TODO(crbug.com/40700976): Test is flaky on Mac debug builds and Linux asan.
#if (BUILDFLAG(IS_MAC) && !defined(NDEBUG)) || defined(ADDRESS_SANITIZER)
#define MAYBE_FindLongString DISABLED_FindLongString
#elif BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
// TODO(crbug.com/40751034): Test is flaky on Linux debug builds.
#define MAYBE_FindLongString DISABLED_FindLongString
#else
#define MAYBE_FindLongString FindLongString
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_FindLongString) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("largepage.html")));

  base::FilePath path = ui_test_utils::GetTestFilePath(
      base::FilePath().AppendASCII("find_in_page"),
      base::FilePath().AppendASCII("LongFind.txt"));
  std::string query;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(path, &query);
  }
  EXPECT_EQ(1, FindInPage16(web_contents, base::UTF8ToUTF16(query), kFwd,
                            kIgnoreCase, nullptr));
}

// Find a big font string in a page.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, BigString) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("BigText.html")));
  EXPECT_EQ(1, FindInPageASCII(web_contents, "SomeLargeString", kFwd,
                               kIgnoreCase, nullptr));
}

// Search Back and Forward on a single occurrence.
// TODO(crbug.com/40714133): Test is flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
#define MAYBE_SingleOccurrence DISABLED_SingleOccurrence
#else
#define MAYBE_SingleOccurrence SingleOccurrence
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_SingleOccurrence) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("FindRandomTests.html")));

  gfx::Rect first_rect;
  EXPECT_EQ(1, ui_test_utils::FindInPage(web_contents, u"2010 Pro Bowl", kFwd,
                                         kIgnoreCase, nullptr, &first_rect));

  gfx::Rect second_rect;
  EXPECT_EQ(1, ui_test_utils::FindInPage(web_contents, u"2010 Pro Bowl", kFwd,
                                         kIgnoreCase, nullptr, &second_rect));

  // Doing a fake find so we have no previous search.
  ui_test_utils::FindInPage(web_contents, u"ghgfjgfh201232rere", kFwd,
                            kIgnoreCase, nullptr, nullptr);

  ASSERT_EQ(first_rect, second_rect);

  EXPECT_EQ(1, ui_test_utils::FindInPage(web_contents, u"2010 Pro Bowl", kFwd,
                                         kIgnoreCase, nullptr, &first_rect));
  EXPECT_EQ(1, ui_test_utils::FindInPage(web_contents, u"2010 Pro Bowl", kBack,
                                         kIgnoreCase, nullptr, &second_rect));
  ASSERT_EQ(first_rect, second_rect);
}

// Find the whole text file page and find count should be 1.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindWholeFileContent) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::FilePath path = ui_test_utils::GetTestFilePath(
      base::FilePath().AppendASCII("find_in_page"),
      base::FilePath().AppendASCII("find_test.txt"));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(path)));

  std::string query;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(path, &query);
  }
  EXPECT_EQ(1, FindInPage16(web_contents, base::UTF8ToUTF16(query), false,
                            false, nullptr));
}

// This test loads a single-frame page and makes sure the ordinal returned makes
// sense as we FindNext over all the items.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageOrdinal) {
  // First we navigate to our page.
  GURL url = GetURL(kFrameData);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for 'o', which should make the first item active and return
  // '1 in 3' (1st ordinal of a total of 3 matches).
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int ordinal = 0;
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Go back one match.
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  // This should wrap to the top.
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  // This should go back to the end.
  EXPECT_EQ(3, FindInPageASCII(web_contents, "o",
                               kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// This tests that the ordinal is correctly adjusted after a selection
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       SelectChangesOrdinal_Issue20883) {
  // First we navigate to our test content.
  GURL url = GetURL(kSelectChangesOrdinal);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for a text that exists within a link on the page.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);

  int ordinal = 0;
  EXPECT_EQ(4, FindInPageASCII(web_contents, "google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Move the selection to link 1, after searching.
  ASSERT_TRUE(content::ExecJs(web_contents, "selectLink1();"));

  // Do a find-next after the selection.  This should move forward
  // from there to the 3rd instance of 'google'.
  EXPECT_EQ(4, FindInPageASCII(web_contents, "google",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);

  // End the find session.
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
}

// This tests that we start searching after selected text.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       StartSearchAfterSelection) {
  // First we navigate to our test content.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL(kStartAfterSelection)));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  int ordinal = 0;

  // Move the selection to the text span.
  ASSERT_TRUE(content::ExecJs(web_contents, "selectSpan();"));

  // Do a find-next after the selection. This should select the 2nd occurrence
  // of the word 'find'.
  EXPECT_EQ(4, FindInPageASCII(web_contents, "fi",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);

  // Refine the search, current active match should not change.
  EXPECT_EQ(4, FindInPageASCII(web_contents, "find",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);

  // Refine the search to 'findMe'. The first new match is before the current
  // active match, the second one is after it. This verifies that refining a
  // search doesn't reset it.
  EXPECT_EQ(2, FindInPageASCII(web_contents, "findMe",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
}

// This test loads a page with frames and makes sure the ordinal returned makes
// sense.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPageMultiFramesOrdinal) {
  // First we navigate to our page.
  GURL url = GetURL(kFramePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for 'a', which should make the first item active and return
  // '1 in 7' (1st ordinal of a total of 7 matches).
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int ordinal = 0;
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(4, ordinal);
  // Go back one, which should go back one frame.
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(4, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(5, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(6, ordinal);
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(7, ordinal);
  // Now we should wrap back to frame 1.
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  // Now we should wrap back to frame last frame.
  EXPECT_EQ(7,
            FindInPageASCII(web_contents, "a", kBack, kIgnoreCase, &ordinal));
  EXPECT_EQ(7, ordinal);
}

// We could get ordinals out of whack when restarting search in subframes.
// See http://crbug.com/5132.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPage_Issue5132) {
  // First we navigate to our page.
  GURL url = GetURL(kFramePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for 'goa' three times (6 matches on page).
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(6, FindInPageASCII(web_contents, "goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(6, FindInPageASCII(web_contents, "goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(2, ordinal);
  EXPECT_EQ(6, FindInPageASCII(web_contents, "goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
  // Add space to search (should result in no matches).
  EXPECT_EQ(0, FindInPageASCII(web_contents, "goa ",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
  // Remove the space, should be back to '3 out of 6')
  EXPECT_EQ(6, FindInPageASCII(web_contents, "goa",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(3, ordinal);
}

// This tests that the ordinal and match count is cleared after a navigation,
// as reported in issue http://crbug.com/126468.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, NavigateClearsOrdinal) {
  // First we navigate to our test content.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open the Find box. In most tests we can just search without opening the
  // box first, but in this case we are testing functionality triggered by
  // `NavigationEntryCommitted()` in the FindBarController and it isn't setup
  // unless the box is open.
  EnsureFindBoxOpen();

  // Search for a text that exists within a link on the page.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  int ordinal = 0;
  EXPECT_EQ(8, FindInPageASCII(web_contents, "e",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Then navigate away (to any page).
  url = GetURL(kLinkPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Open the Find box again.
  EnsureFindBoxOpen();

  EXPECT_EQ(u"e", GetFindBarText());
  EXPECT_TRUE(GetMatchCountText().empty());
}

// Load a page with no selectable text and make sure we don't crash.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindUnselectableText) {
  // First we navigate to our page.
  GURL url = GetURL(kUserSelectPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents, "text",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// Try to reproduce the crash seen in issue 1341577.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindCrash_Issue1341577) {
  // First we navigate to our page.
  GURL url = GetURL(kCrashPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // This would crash the tab. These must be the first two find requests issued
  // against the frame, otherwise an active frame pointer is set and it wont
  // produce the crash.
  // We used to check the return value and |ordinal|. With ICU 4.2, FiP does
  // not find a stand-alone dependent vowel sign of Indic scripts. So, the
  // exptected values are all 0. To make this test pass regardless of
  // ICU version, we just call FiP and see if there's any crash.
  // TODO(jungshik): According to a native Malayalam speaker, it's ok not
  // to find U+0D4C. Still need to investigate further this issue.
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const std::u16string search_str = u"\u0D4C";
  FindInPage16(web_contents, search_str, kFwd, kIgnoreCase, &ordinal);
  FindInPage16(web_contents, search_str, kFwd, kIgnoreCase, &ordinal);

  // This should work fine.
  EXPECT_EQ(1, FindInPage16(web_contents, u"\u0D24\u0D46", kFwd, kIgnoreCase,
                            &ordinal));
  EXPECT_EQ(1, ordinal);
  EXPECT_EQ(0, FindInPageASCII(web_contents, "nostring",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Try to reproduce the crash seen in http://crbug.com/14491, where an assert
// hits in the BitStack size comparison in WebKit.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindCrash_Issue14491) {
  // First we navigate to our page.
  GURL url = GetURL(kBitstackCrash);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // This used to crash the tab.
  int ordinal = 0;
  EXPECT_EQ(0, FindInPageASCII(browser()->tab_strip_model()->
                                   GetActiveWebContents(),
                               "s", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Test to make sure Find does the right thing when restarting from a timeout.
// We used to have a problem where we'd stop finding matches when all of the
// following conditions were true:
// 1) The page has a lot of text to search.
// 2) The page contains more than one match.
// 3) It takes longer than the time-slice given to each Find operation (100
//    ms) to find one or more of those matches (so Find times out and has to try
//    again from where it left off).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindRestarts_Issue1155639) {
  // First we navigate to our page.
  GURL url = GetURL(kTooFewMatchesPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // This string appears 5 times at the bottom of a long page. If Find restarts
  // properly after a timeout, it will find 5 matches, not just 1.
  int ordinal = 0;
  EXPECT_EQ(5, FindInPageASCII(browser()->tab_strip_model()->
                                   GetActiveWebContents(),
                               "008.xml", kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// Make sure we don't get into an infinite loop when text box contains very
// large amount of text.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindRestarts_Issue70505) {
  // First we navigate to our page.
  GURL url = GetURL(kLongTextareaPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // If this test hangs on the FindInPage call, then it might be a regression
  // such as the one found in issue http://crbug.com/70505.
  int ordinal = 0;
  FindInPageASCII(browser()->tab_strip_model()->GetActiveWebContents(),
                  "a", kFwd, kIgnoreCase, &ordinal);
  EXPECT_EQ(1, ordinal);
  // TODO(finnur): We cannot reliably get the matchcount for this Find call
  // until we fix issue http://crbug.com/71176.
}

// This tests bug 11761: FindInPage terminates search prematurely.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindInPagePrematureEnd) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kPrematureEnd);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Search for a text that exists within a link on the page.
  int ordinal = 0;
  EXPECT_EQ(2, FindInPageASCII(web_contents, "html ",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// Verify that the find bar is hidden on reload and navigation.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       HideFindBarOnNavigateAndReload) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kSimple);
  GURL url2 = GetURL(kFramePage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Reload and make sure the find window goes away.
  content::LoadStopObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);

  // Open the find bar again.
  chrome::Find(browser());

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Navigate and make sure the find window goes away.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);
}

IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindStayVisibleOnAnchorLoad) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kAnchorPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Navigate to the same page (but add an anchor/ref/fragment/whatever the kids
  // are calling it these days).
  GURL url_with_anchor = url.Resolve(kAnchor);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_anchor));

  // Make sure it is still open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);
}

// Make sure Find box disappears when History/Downloads page is opened, and
// when a New Tab is opened.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       FindDisappearOnNewTabAndHistory) {
  // First we navigate to our special focus tracking page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());

  gfx::Point position;
  bool fully_visible = false;

  // Make sure it is open.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Open another tab (tab B).
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Make sure Find box is closed.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);

  // Close tab B.
  chrome::CloseTab(browser());

  // Make sure Find window appears again.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  chrome::ShowHistory(browser());

  // Make sure Find box is closed.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_FALSE(fully_visible);
}

// Make sure Find box moves out of the way if it is obscuring the active match.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FindMovesWhenObscuring) {
  GURL url = GetURL(kMoveIfOver);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());

  // This is needed on GTK because the reposition operation is asynchronous.
  base::RunLoop().RunUntilIdle();

  gfx::Point start_position;
  gfx::Point position;
  bool fully_visible = false;
  int ordinal = 0;

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Make a query so the Lens entrypoint disappears. If not, the start position,
  // will be off since the Lens entrypoint will disappear on the first query,
  // and the start_position vs position will be comparing the position of the
  // find bar with vs without the Lens entrypoint.
  // Make sure it is open.
  FindInPageASCII(web_contents, "a", kFwd, kIgnoreCase, &ordinal);

  // Make sure it is open and store starting position.
  EXPECT_TRUE(GetFindBarWindowInfo(&start_position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  int moved_x_coord = FindInPageTillBoxMoves(web_contents, start_position.x(),
                                             "Chromium", kMoveIterations);
  // The find box should have moved.
  EXPECT_TRUE(moved_x_coord != start_position.x());

  // Search for something guaranteed not to be obscured by the Find box.
  EXPECT_EQ(1, FindInPageASCII(web_contents, "Done",
                               kFwd, kIgnoreCase, &ordinal));
  // Check the position.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Make sure Find box has moved back to its original location.
  EXPECT_EQ(position.x(), start_position.x());

  // Move the find box again.
  moved_x_coord = FindInPageTillBoxMoves(web_contents, start_position.x(),
                                         "Chromium", kMoveIterations);
  EXPECT_TRUE(moved_x_coord != start_position.x());

  // Search for an invalid string.
  EXPECT_EQ(0, FindInPageASCII(web_contents, "WeirdSearchString",
                               kFwd, kIgnoreCase, &ordinal));

  // Check the position.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, &fully_visible));
  EXPECT_TRUE(fully_visible);

  // Make sure Find box has moved back to its original location.
  EXPECT_EQ(position.x(), start_position.x());
}

// Make sure F3 in a new tab works if Find has previous string to search for.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       FindNextInNewTabUsesPrepopulate) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for 'no_match'. No matches should be found.
  int ordinal = 0;
  auto* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(0, FindInPageASCII(tab_strip->GetActiveWebContents(), "no_match",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(0, ordinal);

  // Open another tab (tab B).
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Simulate what happens when you press F3 for FindNext. We should get a
  // response here (a hang means search was aborted).
  EXPECT_EQ(0, FindNext(tab_strip->GetActiveWebContents(), &ordinal));
  EXPECT_EQ(0, ordinal);

  // Open another tab (tab C).
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Simulate what happens when you press F3 for FindNext. We should get a
  // response here (a hang means search was aborted).
  EXPECT_EQ(0, FindNext(tab_strip->GetActiveWebContents(), &ordinal));
  EXPECT_EQ(0, ordinal);
}

// Make sure Find box does not become UI-inactive when no text is in the box as
// we switch to a tab contents with an empty find string. See issue 13570.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, StayActive) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  chrome::Find(browser());

  // Simulate a user clearing the search string. Ideally, we should be
  // simulating keypresses here for searching for something and pressing
  // backspace, but that's been proven flaky in the past, so we go straight to
  // web_contents.
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());
  // Stop the (non-existing) find operation, and clear the selection (which
  // signals the UI is still active).
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kClear);
  // Make sure the Find UI flag hasn't been cleared, it must be so that the UI
  // still responds to browser window resizing.
  ASSERT_TRUE(find_tab_helper->find_ui_active());
}

// Make sure F3 works after you FindNext a couple of times and end the Find
// session. See issue http://crbug.com/28306.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, RestartSearchFromF3) {
  // First we navigate to a simple page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for 'page'. Should have 1 match.
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents, "page",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(1, ordinal);

  // Simulate what happens when you press F3 for FindNext. Still should show
  // one match. This cleared the pre-populate string at one point (see bug).
  EXPECT_EQ(1, FindNext(web_contents, &ordinal));
  EXPECT_EQ(1, ordinal);

  // End the Find session, thereby making the next F3 start afresh.
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Simulate F3 while Find box is closed. Should have 1 match.
  EXPECT_EQ(1, FindNext(web_contents, &ordinal));
  EXPECT_EQ(1, ordinal);
}

// When re-opening the find bar with F3, the find bar should be re-populated
// with the last search from the same tab rather than the last overall search.
// The only exception is if there is a global pasteboard (for example on Mac).
// http://crbug.com/30006
#if BUILDFLAG(IS_MAC)
#define MAYBE_PreferPreviousSearch DISABLED_PreferPreviousSearch
#else
#define MAYBE_PreferPreviousSearch PreferPreviousSearch
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, MAYBE_PreferPreviousSearch) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Find "Default".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents_1, "text",
                               kFwd, kIgnoreCase, &ordinal));

  // Create a second tab.
  // For some reason we can't use AddSelectedTabWithURL here on ChromeOS. It
  // could be some delicate assumption about the tab starting off unselected or
  // something relating to user gesture.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Find "given".
  FindInPageASCII(web_contents_2, "given", kFwd, kIgnoreCase, &ordinal);

  // Switch back to first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
  // Simulate F3.
  FindNext(web_contents_1, &ordinal);
  FindBar* find_bar = browser()->GetFindBarController()->find_bar();
  if (find_bar->HasGlobalFindPasteboard()) {
    EXPECT_EQ(find_in_page::FindTabHelper::FromWebContents(web_contents_1)
                  ->find_text(),
              u"given");
  } else {
    EXPECT_EQ(find_in_page::FindTabHelper::FromWebContents(web_contents_1)
                  ->find_text(),
              u"text");
  }
}

// This tests that whenever you close and reopen the Find bar, it should show
// the last search entered in that tab. http://crbug.com/40121.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PrepopulateSameTab) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for the word "page".
  int ordinal = 0;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents, "page",
                               kFwd, kIgnoreCase, &ordinal));

  // Open the Find box.
  EnsureFindBoxOpen();

  EXPECT_EQ(u"page", GetFindBarText());
  EXPECT_EQ(u"1/1", GetMatchCountText());

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Open the Find box again.
  EnsureFindBoxOpen();

  // After the Find box has been reopened, it should have been prepopulated with
  // the word "page" again.
  EXPECT_EQ(u"page", GetFindBarText());
  EXPECT_EQ(u"1/1", GetMatchCountText());
}

// This tests that whenever you open Find in a new tab it should prepopulate
// with a previous search term (in any tab), if a search has not been issued in
// this tab before.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PrepopulateInNewTab) {
  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for the word "page".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents_1, "page",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(u"1/1", GetMatchCountText());

  // Now create a second tab and load the same page.
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Open the Find box.
  EnsureFindBoxOpen();

  // The new tab should have "page" prepopulated, since that was the last search
  // in the first tab.
  EXPECT_EQ(u"page", GetFindBarText());
  // But it should not seem like a search has been issued.
  EXPECT_EQ(std::u16string(), GetMatchCountText());
}

// This makes sure that we can search for A in tabA, then for B in tabB and
// when we come back to tabA we should still see A (because that was the last
// search in that tab).
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, PrepopulatePreserveLast) {
  FindBar* find_bar = browser()->GetFindBarController()->find_bar();
  if (find_bar->HasGlobalFindPasteboard())
    return;

  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for the word "page".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents_1, "page",
                               kFwd, kIgnoreCase, &ordinal));

  // Open the Find box.
  EnsureFindBoxOpen();

  EXPECT_EQ(u"page", GetFindBarText());

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Now create a second tab and load the same page.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Search for the word "text".
  FindInPageASCII(web_contents_2, "text", kFwd, kIgnoreCase, &ordinal);

  // Go back to the first tab and make sure we have NOT switched the prepopulate
  // text to "text".
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Open the Find box.
  EnsureFindBoxOpen();

  // After the Find box has been reopened, it should have been prepopulated with
  // the word "page" again, since that was the last search in that tab.
  EXPECT_EQ(u"page", GetFindBarText());

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Re-open the Find box.
  // This is a special case: previous search in WebContents used to get cleared
  // if you opened and closed the FindBox, which would cause the global
  // prepopulate value to show instead of last search in this tab.
  EnsureFindBoxOpen();

  // After the Find box has been reopened, it should have been prepopulated with
  // the word "page" again, since that was the last search in that tab.
  EXPECT_EQ(u"page", GetFindBarText());
}

// This tests that search terms entered into an incognito find bar are not used
// as prepopulate terms for non-incognito windows.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, NoIncognitoPrepopulate) {
  FindBar* find_bar = browser()->GetFindBarController()->find_bar();
  if (find_bar->HasGlobalFindPasteboard())
    return;

  // First we navigate to the "simple" test page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Search for the word "page" in the normal browser tab.
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents_1, "page",
                               kFwd, kIgnoreCase, &ordinal));

  // Open the Find box.
  EnsureFindBoxOpenForBrowser(browser());
  EXPECT_EQ(u"page", GetFindBarTextForBrowser(browser()));

  // Close the Find box.
  browser()->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Open a new incognito window and navigate to the same page.
  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Browser* incognito_browser =
      Browser::Create(Browser::CreateParams(incognito_profile, true));
  chrome::AddSelectedTabWithURL(incognito_browser, url,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  EXPECT_TRUE(content::WaitForLoadStop(
      incognito_browser->tab_strip_model()->GetActiveWebContents()));
  incognito_browser->window()->Show();

  // Open the find box and make sure that it is prepopulated with "page".
  EnsureFindBoxOpenForBrowser(incognito_browser);
  EXPECT_EQ(u"page", GetFindBarTextForBrowser(incognito_browser));

  // Search for the word "text" in the incognito tab.
  WebContents* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(incognito_tab, "text",
                               kFwd, kIgnoreCase, &ordinal));
  EXPECT_EQ(u"text", GetFindBarTextForBrowser(incognito_browser));

  // Close the Find box.
  incognito_browser->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);

  // Now open a new tab in the original (non-incognito) browser.
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(web_contents_1, web_contents_2);

  // Open the Find box and make sure it is prepopulated with the search term
  // from the original browser, not the search term from the incognito window.
  EnsureFindBoxOpenForBrowser(browser());
  EXPECT_EQ(u"page", GetFindBarTextForBrowser(browser()));
}

// This makes sure that dismissing the find bar with kActivateSelection works.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, ActivateLinkNavigatesPage) {
  // First we navigate to our test content.
  GURL url = GetURL(kLinkPage);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);

  int ordinal = 0;
  FindInPageASCII(web_contents, "link", kFwd, kIgnoreCase, &ordinal);
  EXPECT_EQ(ordinal, 1);

  // End the find session, click on the link.
  content::LoadStopObserver observer(web_contents);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kActivate);
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, FitWindow) {
  Browser::CreateParams params(Browser::TYPE_POPUP, browser()->profile(), true);
  params.initial_bounds = gfx::Rect(0, 0, 100, 500);
  Browser* popup = Browser::Create(params);
  chrome::AddSelectedTabWithURL(popup, GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_LINK);
  // Wait for the page to finish loading.
  EXPECT_TRUE(content::WaitForLoadStop(
      popup->tab_strip_model()->GetActiveWebContents()));
  popup->window()->Show();

  EnsureFindBoxOpenForBrowser(popup);

  ASSERT_LE(GetFindBarWidthForBrowser(popup),
            popup->window()->GetBounds().width());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40230732): Flakily crashes on Lacros.
#define MAYBE_FindMovesOnTabClose_Issue1343052 \
  DISABLED_FindMovesOnTabClose_Issue1343052
#else
#define MAYBE_FindMovesOnTabClose_Issue1343052 FindMovesOnTabClose_Issue1343052
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       MAYBE_FindMovesOnTabClose_Issue1343052) {
  EnsureFindBoxOpen();
  content::RunAllPendingInMessageLoop();  // Needed on Linux.

  gfx::Point position;
  EXPECT_TRUE(GetFindBarWindowInfo(&position, nullptr));

  // Open another tab.
  GURL url = GetURL(kSimple);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close it.
  chrome::CloseTab(browser());

  // See if the Find window has moved.
  gfx::Point position2;
  EXPECT_TRUE(GetFindBarWindowInfo(&position2, nullptr));
  EXPECT_EQ(position, position2);

  // Toggle the bookmark bar state. Note that this starts an animation, and
  // there isn't a good way other than looping and polling to see when it's
  // done. So instead we change the state and open a new tab, since the new tab
  // animation doesn't happen on tab change.
  chrome::ToggleBookmarkBar(browser());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EnsureFindBoxOpen();
  content::RunAllPendingInMessageLoop();  // Needed on Linux.
  EXPECT_TRUE(GetFindBarWindowInfo(&position, nullptr));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  chrome::CloseTab(browser());
  EXPECT_TRUE(GetFindBarWindowInfo(&position2, nullptr));
  EXPECT_EQ(position, position2);
}

// Verify that if there's a global pasteboard (for example on Mac) then doing
// a search on one tab will clear the matches label on the other tabs.
#if BUILDFLAG(IS_MAC)
// TODO(http://crbug.com/843878): Remove the interactive UI test
// FindBarPlatformHelperMacInteractiveUITest.GlobalPasteBoardClearMatches
// once http://crbug.com/843878 is fixed.
#define MAYBE_GlobalPasteBoardClearMatches DISABLED_GlobalPasteBoardClearMatches
#else
#define MAYBE_GlobalPasteBoardClearMatches GlobalPasteBoardClearMatches
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       MAYBE_GlobalPasteBoardClearMatches) {
  FindBar* find_bar = browser()->GetFindBarController()->find_bar();
  if (!find_bar->HasGlobalFindPasteboard())
    return;

  // First we navigate to any page.
  GURL url = GetURL(kSimple);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Change the match count on the first tab to "1/1".
  int ordinal = 0;
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(1, FindInPageASCII(web_contents_1, "page",
                               kFwd, kIgnoreCase, &ordinal));
  EnsureFindBoxOpen();
  EXPECT_EQ(u"1/1", GetMatchCountText());

  // Next, do a search in a second tab.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WebContents* web_contents_2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  FindInPageASCII(web_contents_2, "text", kFwd, kIgnoreCase, &ordinal);
  EXPECT_EQ(u"1/1", GetMatchCountText());

  // Go back to the first tab and verify that the match text is cleared.
  // text to "text".
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(GetMatchCountText().empty());
}

// Verify that Incognito window doesn't propagate find string to other widows.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, GlobalPasteboardIncognito) {
  Browser* browser_incognito = CreateIncognitoBrowser();
  WebContents* web_contents_1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  FindInPageASCII(web_contents_1, "page", kFwd, kIgnoreCase, nullptr);
  EXPECT_EQ(u"page", GetFindBarText());
  WebContents* web_contents_2 =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  FindInPageASCII(web_contents_2, "Incognito", kFwd, kIgnoreCase, nullptr);
  EXPECT_EQ(u"Incognito", GetFindBarTextForBrowser(browser_incognito));
  EXPECT_EQ(u"page", GetFindBarText());
}

// Find text in regular window, find different text in incognito, send
// IDC_FIND_NEXT to incognito. It should search for the second phrase.
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest, IncognitoFindNextSecret) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // On Mac this updates the find pboard.
  FindInPageASCII(web_contents, "bar", kFwd, kIgnoreCase, nullptr);

  Browser* browser_incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_incognito,
                                           GURL("data:text/plain,barfoofoo")));
  WebContents* web_contents_incognito =
        browser_incognito->tab_strip_model()->GetActiveWebContents();
  FindInPageASCII(web_contents_incognito, "foo", true, kIgnoreCase, nullptr);
  EXPECT_EQ(u"foo", GetFindBarTextForBrowser(browser_incognito));
  EXPECT_EQ(u"1/2", GetFindBarMatchCountTextForBrowser(browser_incognito));

  // Close the find bar.
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents_incognito);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kActivate);

  // Cmd + G triggers IDC_FIND_NEXT command. Thus we test FindInPage()
  // method from browser_commands.cc. FindInPage16() bypasses it.
  EXPECT_TRUE(chrome::ExecuteCommand(browser_incognito, IDC_FIND_NEXT));
  ui_test_utils::FindResultWaiter(web_contents_incognito).Wait();
  EXPECT_EQ(u"foo", GetFindBarTextForBrowser(browser_incognito));
  EXPECT_EQ(u"2/2", GetFindBarMatchCountTextForBrowser(browser_incognito));
}

// Find text in regular window, send IDC_FIND_NEXT to incognito. It should
// search for the first phrase.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IncognitoFindNextShared DISABLED_IncognitoFindNextShared
#else
#define MAYBE_IncognitoFindNextShared IncognitoFindNextShared
#endif
IN_PROC_BROWSER_TEST_F(FindInPageControllerTest,
                       MAYBE_IncognitoFindNextShared) {
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // On Mac this updates the find pboard.
  FindInPageASCII(web_contents, "bar", kFwd, kIgnoreCase, nullptr);

  Browser* browser_incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_incognito,
                                           GURL("data:text/plain,bar")));

  EXPECT_TRUE(chrome::ExecuteCommand(browser_incognito, IDC_FIND_NEXT));
  WebContents* web_contents_incognito =
      browser_incognito->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::FindResultWaiter(web_contents_incognito).Wait();
  EXPECT_EQ(u"bar", GetFindBarTextForBrowser(browser_incognito));
}
