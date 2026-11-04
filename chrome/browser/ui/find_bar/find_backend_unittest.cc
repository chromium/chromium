// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContents;
using content::WebContentsTester;

class FindBackendTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Give the main frame a live (test) renderer. Without one,
    // FindRequestManager has no frame to search and immediately reports a final
    // 0-match result for queries of 4+ characters, which would be counted
    // before the test's own simulated DidReceiveFindReply().
    NavigateAndCommit(GURL("about:blank"));
    FindBarState::ConfigureWebContents(web_contents());
  }

  void SimulateCompletedSearch(find_in_page::FindTabHelper* helper,
                               const std::u16string& text,
                               int matches) {
    helper->StartFinding(text, /*forward_direction=*/true,
                         /*case_sensitive=*/false, /*find_match=*/true);
    helper->DidReceiveFindReply(
        helper->current_find_request_id(), matches, gfx::Rect(),
        /*active_match_ordinal=*/matches > 0 ? 1 : 0, /*final_update=*/true);
  }
};

namespace {

std::u16string FindPrepopulateText(WebContents* contents) {
  return FindBarStateFactory::GetForBrowserContext(
             contents->GetBrowserContext())
      ->GetSearchPrepopulateText(contents);
}

}  // end namespace

// This test takes two WebContents objects, searches in both of them and
// tests the internal state for find_text and find_prepopulate_text.
TEST_F(FindBackendTest, InternalState) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());
  // Initial state for the WebContents is blank strings.
  EXPECT_EQ(std::u16string(), FindPrepopulateText(web_contents()));
  EXPECT_EQ(std::u16string(), find_tab_helper->find_text());

  // Get another WebContents object ready.
  std::unique_ptr<WebContents> contents2(
      WebContentsTester::CreateTestWebContents(profile(), nullptr));
  FindBarState::ConfigureWebContents(contents2.get());
  find_in_page::FindTabHelper* find_tab_helper2 =
      find_in_page::FindTabHelper::FromWebContents(contents2.get());

  // No search has still been issued, strings should be blank.
  EXPECT_EQ(std::u16string(), FindPrepopulateText(web_contents()));
  EXPECT_EQ(std::u16string(), find_tab_helper->find_text());
  EXPECT_EQ(std::u16string(), FindPrepopulateText(contents2.get()));
  EXPECT_EQ(std::u16string(), find_tab_helper2->find_text());

  std::u16string search_term1 = u" I had a 401K    ";
  std::u16string search_term2 = u" but the economy ";
  std::u16string search_term3 = u" eated it.       ";

  // Start searching in the first WebContents.
  find_tab_helper->StartFinding(search_term1, true /* forward_direction */,
                                false /* case_sensitive */,
                                true /* find_match */);

  // Pre-populate string should always match between the two, but find_text
  // should not.
  EXPECT_EQ(search_term1, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term1, find_tab_helper->find_text());
  EXPECT_EQ(search_term1, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(std::u16string(), find_tab_helper2->find_text());

  // Now search in the other WebContents.
  find_tab_helper2->StartFinding(search_term2, true /* forward_direction */,
                                 false /* case_sensitive */,
                                 true /* find_match */);

  // Again, pre-populate string should always match between the two, but
  // find_text should not.
  EXPECT_EQ(search_term2, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term1, find_tab_helper->find_text());
  EXPECT_EQ(search_term2, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(search_term2, find_tab_helper2->find_text());

  // Search again in the first WebContents
  find_tab_helper->StartFinding(search_term3, true /* forward_direction */,
                                false /* case_sensitive */,
                                true /* find_match */);

  // The fallback search term for the first WebContents will be the original
  // search.
  EXPECT_EQ(search_term3, FindPrepopulateText(web_contents()));
  EXPECT_EQ(search_term3, find_tab_helper->find_text());
  EXPECT_EQ(search_term3, FindPrepopulateText(contents2.get()));
  EXPECT_EQ(search_term2, find_tab_helper2->find_text());
}

TEST_F(FindBackendTest, PolicyRestrictions) {
  data_controls::SetDataControls(profile()->GetPrefs(), {R"({
                    "name": "block_google_to_bing",
                    "rule_id": "1234",
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "destinations": {
                      "urls": ["bing.com"]
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  // Set up the source WebContents (google.com)
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://google.com"));

  // Set up allowed destination WebContents (yahoo.com)
  std::unique_ptr<WebContents> contents_yahoo(
      WebContentsTester::CreateTestWebContents(profile(), nullptr));
  FindBarState::ConfigureWebContents(contents_yahoo.get());
  content::WebContentsTester::For(contents_yahoo.get())
      ->NavigateAndCommit(GURL("https://yahoo.com"));

  // Set up blocked destination WebContents (bing.com)
  std::unique_ptr<WebContents> contents_bing(
      WebContentsTester::CreateTestWebContents(profile(), nullptr));
  FindBarState::ConfigureWebContents(contents_bing.get());
  content::WebContentsTester::For(contents_bing.get())
      ->NavigateAndCommit(GURL("https://bing.com"));

  // Search in Google WebContents
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());
  find_tab_helper->StartFinding(u"sensitive", true /* forward_direction */,
                                false /* case_sensitive */,
                                true /* find_match */);

  // Check prepopulating in Google itself is allowed.
  EXPECT_EQ(u"sensitive", FindPrepopulateText(web_contents()));

  // Check prepopulating in Yahoo (allowed destination) is allowed.
  EXPECT_EQ(u"sensitive", FindPrepopulateText(contents_yahoo.get()));

  // Check prepopulating in Bing (blocked destination) is blocked.
  EXPECT_EQ(std::u16string(), FindPrepopulateText(contents_bing.get()));
}

// FindInPage.SessionOutcome is only recorded on Desktop.
#if !BUILDFLAG(IS_ANDROID)
// Ending a session without any completed active search logs nothing.
TEST_F(FindBackendTest, SessionOutcome_NoActiveSearchCompleted) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  // 1. Ending without any search logs nothing.
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectTotalCount("FindInPage.SessionOutcome", 0);

  // 2. find_match = false logs nothing.
  find_tab_helper->StartFinding(u"cat", /*forward_direction=*/true,
                                /*case_sensitive=*/false, /*find_match=*/false);
  find_tab_helper->DidReceiveFindReply(
      find_tab_helper->current_find_request_id(), 3, gfx::Rect(),
      /*active_match_ordinal=*/0, /*final_update=*/true);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectTotalCount("FindInPage.SessionOutcome", 0);

  // 3. Passive selection pre-fill (find_match = true, scroll_to_match = false)
  // logs nothing.
  find_tab_helper->StartFinding(u"cat", /*forward_direction=*/true,
                                /*case_sensitive=*/false, /*find_match=*/true,
                                /*scroll_to_match=*/false);
  find_tab_helper->DidReceiveFindReply(
      find_tab_helper->current_find_request_id(), 3, gfx::Rect(),
      /*active_match_ordinal=*/1, /*final_update=*/true);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectTotalCount("FindInPage.SessionOutcome", 0);

  // 4. Actively searching on that same text (find_match = true,
  // scroll_to_match = true) counts and logs the outcome.
  find_tab_helper->StartFinding(u"cat", /*forward_direction=*/true,
                                /*case_sensitive=*/false, /*find_match=*/true,
                                /*scroll_to_match=*/true);
  find_tab_helper->DidReceiveFindReply(
      find_tab_helper->current_find_request_id(), 3, gfx::Rect(),
      /*active_match_ordinal=*/1, /*final_update=*/true);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedWithoutSeeingZeroMatch, 1);
}

// Session that matches without ever hitting zero matches.
TEST_F(FindBackendTest, SessionOutcome_MatchedWithoutSeeingZeroMatch) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"cat", 3);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedWithoutSeeingZeroMatch, 1);
}

// Session that hits zero matches on a typo ("chromez"), then recovers by
// backspacing to "chrome" and ending the session.
TEST_F(FindBackendTest,
       SessionOutcome_MatchedAfterSeeingZeroMatch_ViaBackspace) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"chromez", 0);
  SimulateCompletedSearch(find_tab_helper, u"chrome", 5);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedAfterSeeingZeroMatch, 1);
}

// Session that hits zero matches, then recovers by searching a new term.
TEST_F(FindBackendTest,
       SessionOutcome_MatchedAfterSeeingZeroMatch_ViaNewQuery) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"unicorn", 0);
  SimulateCompletedSearch(find_tab_helper, u"horse", 2);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedAfterSeeingZeroMatch, 1);
}

// Session that ends with zero matches.
TEST_F(FindBackendTest, SessionOutcome_EndedWithZeroMatches) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"xyz", 0);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kEndedWithZeroMatches, 1);
}

// Session that hits zero matches, then backspaces through intermediate matches
// to an empty search box.
TEST_F(FindBackendTest,
       SessionOutcome_EndedWithEmptyQueryAfterSeeingZeroMatch) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"unicorn", 0);
  SimulateCompletedSearch(find_tab_helper, u"unicor", 0);
  SimulateCompletedSearch(find_tab_helper, u"u", 10);
  find_tab_helper->StartFinding(u"", /*forward_direction=*/true,
                                /*case_sensitive=*/false,
                                /*find_match=*/true);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::
          kEndedWithEmptyQueryAfterSeeingZeroMatch,
      1);
}

// Session that hits zero matches on a typo, recovers via backspace, and then
// clears the search box before closing.
TEST_F(
    FindBackendTest,
    SessionOutcome_EndedWithEmptyQueryAfterSeeingZeroMatch_AfterTypoRecovery) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"chromez", 0);
  SimulateCompletedSearch(find_tab_helper, u"chrome", 5);
  find_tab_helper->StartFinding(u"", /*forward_direction=*/true,
                                /*case_sensitive=*/false,
                                /*find_match=*/true);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::
          kEndedWithEmptyQueryAfterSeeingZeroMatch,
      1);
}

// Session that clears the search box to empty without ever hitting zero
// matches.
TEST_F(FindBackendTest,
       SessionOutcome_EndedWithEmptyQueryWithoutSeeingZeroMatch) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"chrome", 5);
  SimulateCompletedSearch(find_tab_helper, u"c", 20);
  find_tab_helper->StartFinding(u"", /*forward_direction=*/true,
                                /*case_sensitive=*/false,
                                /*find_match=*/true);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::
          kEndedWithEmptyQueryWithoutSeeingZeroMatch,
      1);
}

// Programmatic clear (e.g. macOS pasteboard sync or tab change) calling
// StopFinding(kClear) directly does not mark ended_with_empty_query.
TEST_F(FindBackendTest, SessionOutcome_ProgrammaticClearDoesNotMarkEmptyQuery) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"chrome", 5);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kClear);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedWithoutSeeingZeroMatch, 1);
}

// A repeat zero match search on teardown does not overwrite the match outcome.
TEST_F(FindBackendTest,
       SessionOutcome_RepeatZeroMatchOnTeardownDoesNotOverwriteMatch) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"cat", 3);
  SimulateCompletedSearch(find_tab_helper, u"cat", 0);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedWithoutSeeingZeroMatch, 1);
}

// Zero matches encountered only during a backspace does not count as
// seeing a zero-match.
TEST_F(FindBackendTest, SessionOutcome_ZeroMatchOnBackspaceIgnored) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"cat", 1);
  SimulateCompletedSearch(find_tab_helper, u"ca", 0);
  SimulateCompletedSearch(find_tab_helper, u"c", 2);
  find_tab_helper->StopFinding(find_in_page::SelectionAction::kKeep);
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedWithoutSeeingZeroMatch, 1);
}

// Destroying WebContents while a find session is still active properly records
// the session outcome.
TEST_F(FindBackendTest, SessionOutcome_RecordedOnWebContentsDestruction) {
  base::HistogramTester histogram_tester;
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents());

  SimulateCompletedSearch(find_tab_helper, u"cat", 4);
  DeleteContents();
  histogram_tester.ExpectUniqueSample(
      "FindInPage.SessionOutcome",
      find_in_page::FindSessionOutcome::kMatchedWithoutSeeingZeroMatch, 1);
}

#endif  // !BUILDFLAG(IS_ANDROID)
