// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_provider.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_provider_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using base::ASCIIToUTF16;
using ExpectedURLs = std::vector<ExpectedURLAndAllowedToBeDefault>;

namespace {

struct TestShortcutData shortcut_test_db[] = {
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E0", "goog", "www.google.com",
     "http://www.google.com/", "Google", "0,1,4,0", "Google", "0,3,4,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E1", "slash", "slashdot.org",
     "http://slashdot.org/", "slashdot.org", "0,3,5,1",
     "Slashdot - News for nerds, stuff that matters", "0,2,5,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 0, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E2", "news", "slashdot.org",
     "http://slashdot.org/", "slashdot.org", "0,1",
     "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 0, 5},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E3", "news", "sports.yahoo.com",
     "http://sports.yahoo.com/", "sports.yahoo.com", "0,1",
     "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
     "0,0,23,2,27,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 2, 5},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E4", "news weather",
     "www.cnn.com/index.html", "http://www.cnn.com/index.html",
     "www.cnn.com/index.html", "0,1",
     "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
     "0,0,19,2,23,0,38,2,45,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 1, 10},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E5", "nhl scores", "sports.yahoo.com",
     "http://sports.yahoo.com/", "sports.yahoo.com", "0,1",
     "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
     "0,0,29,2,35,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_BODY, "", 1, 10},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E6", "nhl scores",
     "www.nhl.com/scores/index.html", "http://www.nhl.com/scores/index.html",
     "www.nhl.com/scores/index.html", "0,1,4,3,7,1",
     "January 13, 2010 - NHL.com - Scores", "0,0,19,2,22,0,29,2,35,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 5, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E7", "just", "www.testsite.com/a.html",
     "http://www.testsite.com/a.html", "www.testsite.com/a.html", "0,1",
     "Test - site - just a test", "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 5, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E8", "just", "www.testsite.com/b.html",
     "http://www.testsite.com/b.html", "www.testsite.com/b.html", "0,1",
     "Test - site - just a test", "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 5, 2},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880E9", "just", "www.testsite.com/c.html",
     "http://www.testsite.com/c.html", "www.testsite.com/c.html", "0,1",
     "Test - site - just a test", "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 8, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EA", "just a",
     "www.testsite.com/d.html", "http://www.testsite.com/d.html",
     "www.testsite.com/d.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EB", "just a t",
     "www.testsite.com/e.html", "http://www.testsite.com/e.html",
     "www.testsite.com/e.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EC", "just a te",
     "www.testsite.com/f.html", "http://www.testsite.com/f.html",
     "www.testsite.com/f.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880ED", "ago",
     "www.daysagotest.com/a.html", "http://www.daysagotest.com/a.html",
     "www.daysagotest.com/a.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EE", "ago",
     "www.daysagotest.com/b.html", "http://www.daysagotest.com/b.html",
     "www.daysagotest.com/b.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 2, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880EF", "ago",
     "www.daysagotest.com/c.html", "http://www.daysagotest.com/c.html",
     "www.daysagotest.com/c.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 3, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F0", "ago",
     "www.daysagotest.com/d.html", "http://www.daysagotest.com/d.html",
     "www.daysagotest.com/d.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 4, 1},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F2", "abcdef.com", "http://abcdef.com",
     "http://abcdef.com/", "Abcdef", "0,1,4,0", "Abcdef", "0,3,4,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F3", "query", "query",
     "https://www.google.com/search?q=query", "query", "0,0", "Google Search",
     "0,4", ui::PAGE_TRANSITION_GENERATED,
     AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F4", "word", "www.word",
     "https://www.google.com/search?q=www.word", "www.word", "0,0",
     "Google Search", "0,4", ui::PAGE_TRANSITION_GENERATED,
     AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F5", "about:o", "chrome://omnibox",
     "chrome://omnibox/", "about:omnibox", "0,3,10,1", "", "",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::NAVSUGGEST, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F6", "www/real sp",
     "http://www/real space/long-url-with-space.html",
     "http://www/real%20space/long-url-with-space.html",
     "www/real space/long-url-with-space.html", "0,3,11,1",
     "Page With Space; Input with Space", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F7", "duplicate",
     "http://duplicate.com", "http://duplicate.com/", "Duplicate", "0,1",
     "Duplicate", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F8", "dupl", "http://duplicate.com",
     "http://duplicate.com/", "Duplicate", "0,1", "Duplicate", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880F9", "notrailing.com/",
     "http://notrailing.com", "http://notrailing.com/", "No Trailing Slash",
     "0,1", "No Trailing Slash on fill_into_edit", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FA", "http:///foo.com",
     "http://foo.com", "http://foo.com/", "Foo - Typo in Input", "0,1",
     "Foo - Typo in Input Corrected in fill_into_edit", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FB", "trailing1 ",
     "http://trailing1.com", "http://trailing1.com/",
     "Trailing1 - Space in Shortcut", "0,1", "Trailing1 - Space in Shortcut",
     "0,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "",
     1, 100},
    {"BD85DBA2-8C29-49F9-84AE-48E1E90880FC", "about:trailing2 ",
     "chrome://trailing2blah", "chrome://trailing2blah/",
     "Trailing2 - Space in Shortcut", "0,1", "Trailing2 - Space in Shortcut",
     "0,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "",
     1, 100},
};

}  // namespace

// ClassifyTest ---------------------------------------------------------------

// Helper class to make running tests of ClassifyAllMatchesInString() more
// convenient.
class ClassifyTest {
 public:
  ClassifyTest(const base::string16& text,
               const bool text_is_query,
               ACMatchClassifications matches);
  ~ClassifyTest();

  ACMatchClassifications RunTest(const base::string16& find_text);

 private:
  const base::string16 text_;
  const bool text_is_query_;
  const ACMatchClassifications matches_;
};

ClassifyTest::ClassifyTest(const base::string16& text,
                           const bool text_is_query,
                           ACMatchClassifications matches)
    : text_(text), text_is_query_(text_is_query), matches_(matches) {}

ClassifyTest::~ClassifyTest() {}

ACMatchClassifications ClassifyTest::RunTest(const base::string16& find_text) {
  return ShortcutsProvider::ClassifyAllMatchesInString(
      find_text, ShortcutsProvider::CreateWordMapForString(find_text), text_,
      text_is_query_, matches_);
}

// ShortcutsProviderTest ------------------------------------------------------

class ShortcutsProviderTest : public testing::Test {
 public:
  ShortcutsProviderTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  // Passthrough to the private function in provider_.
  int CalculateScore(const std::string& terms,
                     const ShortcutsDatabase::Shortcut& shortcut,
                     int max_relevance);

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<ShortcutsProvider> provider_;
};

ShortcutsProviderTest::ShortcutsProviderTest() {}

void ShortcutsProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  ASSERT_TRUE(client_->GetShortcutsBackend());
  provider_ = base::MakeRefCounted<ShortcutsProvider>(client_.get());
  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       shortcut_test_db,
                                       arraysize(shortcut_test_db));
}

void ShortcutsProviderTest::TearDown() {
  provider_ = nullptr;
  client_.reset();
  scoped_task_environment_.RunUntilIdle();
}

int ShortcutsProviderTest::CalculateScore(
    const std::string& terms,
    const ShortcutsDatabase::Shortcut& shortcut,
    int max_relevance) {
  return provider_->CalculateScore(ASCIIToUTF16(terms), shortcut,
                                   max_relevance);
}

// Actual tests ---------------------------------------------------------------

TEST_F(ShortcutsProviderTest, SimpleSingleMatch) {
  base::string16 text(ASCIIToUTF16("go"));
  std::string expected_url("http://www.google.com/");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16("ogle.com"));

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           ASCIIToUTF16("ogle.com"));

  // A pair of analogous tests where the shortcut ends at the end of
  // |fill_into_edit|.  This exercises the inline autocompletion and default
  // match code.
  text = ASCIIToUTF16("abcdef.com");
  expected_url = "http://abcdef.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           base::string16());
  // With prevent inline autocomplete, the suggestion should be the same
  // (because there is no completion).
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           base::string16());

  // Another test, simply for a query match type, not a navigation URL match
  // type.
  text = ASCIIToUTF16("que");
  expected_url = "https://www.google.com/search?q=query";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16("ry"));

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           ASCIIToUTF16("ry"));

  // A pair of analogous tests where the shortcut ends at the end of
  // |fill_into_edit|.  This exercises the inline autocompletion and default
  // match code.
  text = ASCIIToUTF16("query");
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           base::string16());
  // With prevent inline autocomplete, the suggestion should be the same
  // (because there is no completion).
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           base::string16());

  // Now the shortcut ends at the end of |fill_into_edit| but has a
  // non-droppable prefix.  ("www.", for instance, is not droppable for
  // queries.)
  text = ASCIIToUTF16("word");
  expected_url = "https://www.google.com/search?q=www.word";
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           base::string16());
}

// These tests are like those in SimpleSingleMatch but more complex,
// involving URLs that need to be fixed up to match properly.
TEST_F(ShortcutsProviderTest, TrickySingleMatch) {
  // Test that about: URLs are fixed up/transformed to chrome:// URLs.
  base::string16 text(ASCIIToUTF16("about:o"));
  std::string expected_url("chrome://omnibox/");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16("mnibox"));

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           ASCIIToUTF16("mnibox"));

  // Test that an input with a space can match URLs with a (escaped) space.
  // This would fail if we didn't try to lookup the un-fixed-up string.
  text = ASCIIToUTF16("www/real sp");
  expected_url = "http://www/real%20space/long-url-with-space.html";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16("ace/long-url-with-space.html"));

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           ASCIIToUTF16("ace/long-url-with-space.html"));

  // Test when the user input has a typo that can be fixed up for matching
  // fill_into_edit.  This should still be allowed to be default.
  text = ASCIIToUTF16("http:///foo.com");
  expected_url = "http://foo.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           base::string16());

  // A foursome of tests to verify that trailing spaces prevent the shortcut
  // from being allowed to be the default match.  For each of two tests, we
  // first verify that the match is allowed to be default without the trailing
  // space but is not allowed to be default with the trailing space.  In both
  // of these with-trailing-space cases, we actually get an
  // inline_autocompletion, though it's never used because the match is
  // prohibited from being default.
  text = ASCIIToUTF16("trailing1");
  expected_url = "http://trailing1.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16(".com"));
  text = ASCIIToUTF16("trailing1 ");
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16(".com"));
  text = ASCIIToUTF16("about:trailing2");
  expected_url = "chrome://trailing2blah/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16("blah"));
  text = ASCIIToUTF16("about:trailing2 ");
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           ASCIIToUTF16("blah"));
}

TEST_F(ShortcutsProviderTest, MultiMatch) {
  base::string16 text(ASCIIToUTF16("NEWS"));
  ExpectedURLs expected_urls;
  // Scores high because of completion length.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://slashdot.org/", false));
  // Scores high because of visit count.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://sports.yahoo.com/", false));
  // Scores high because of visit count but less match span,
  // which is more important.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.cnn.com/index.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://slashdot.org/", base::string16());
}

TEST_F(ShortcutsProviderTest, RemoveDuplicates) {
  base::string16 text(ASCIIToUTF16("dupl"));
  ExpectedURLs expected_urls;
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://duplicate.com/", true));
  // Make sure the URL only appears once in the output list.
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://duplicate.com/", ASCIIToUTF16("icate.com"));
}

TEST_F(ShortcutsProviderTest, TypedCountMatches) {
  base::string16 text(ASCIIToUTF16("just"));
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/b.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/a.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/c.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.testsite.com/b.html", base::string16());
}

TEST_F(ShortcutsProviderTest, FragmentLengthMatches) {
  base::string16 text(ASCIIToUTF16("just a"));
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/d.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/e.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.testsite.com/f.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.testsite.com/d.html", base::string16());
}

TEST_F(ShortcutsProviderTest, DaysAgoMatches) {
  base::string16 text(ASCIIToUTF16("ago"));
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/a.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/b.html", false));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/c.html", false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.daysagotest.com/a.html",
                           base::string16());
}

TEST_F(ShortcutsProviderTest, ClassifyAllMatchesInString) {
  ACMatchClassifications matches =
      AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test(ASCIIToUTF16("A man, a plan, a canal Panama"),
                             /*text_is_query=*/false, matches);

  ACMatchClassifications spans_a = classify_test.RunTest(ASCIIToUTF16("man"));
  // ACMatch spans should be: '--MMM------------------------'
  EXPECT_EQ("0,0,2,2,5,0", AutocompleteMatch::ClassificationsToString(spans_a));

  ACMatchClassifications spans_b = classify_test.RunTest(ASCIIToUTF16("man p"));
  // ACMatch spans should be: '--MMM----M-------------M-----'
  EXPECT_EQ("0,0,2,2,5,0,9,2,10,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans_b));

  ACMatchClassifications spans_c =
      classify_test.RunTest(ASCIIToUTF16("man plan panama"));
  // ACMatch spans should be:'--MMM----MMMM----------MMMMMM'
  EXPECT_EQ("0,0,2,2,5,0,9,2,13,0,23,2",
            AutocompleteMatch::ClassificationsToString(spans_c));

  ClassifyTest classify_test2(
      ASCIIToUTF16("Yahoo! Sports - Sports News, "
                   "Scores, Rumors, Fantasy Games, and more"),
      /*text_is_query=*/false, matches);

  ACMatchClassifications spans_d = classify_test2.RunTest(ASCIIToUTF16("ne"));
  // ACMatch spans should match first two letters of the "news".
  EXPECT_EQ("0,0,23,2,25,0",
            AutocompleteMatch::ClassificationsToString(spans_d));

  ACMatchClassifications spans_e =
      classify_test2.RunTest(ASCIIToUTF16("news r"));
  EXPECT_EQ(
      "0,0,10,2,11,0,19,2,20,0,23,2,27,0,32,2,33,0,37,2,38,0,41,2,42,0,"
      "66,2,67,0",
      AutocompleteMatch::ClassificationsToString(spans_e));

  matches = AutocompleteMatch::ClassificationsFromString("0,1");
  ClassifyTest classify_test3(ASCIIToUTF16("livescore.goal.com"),
                              /*text_is_query=*/false, matches);

  ACMatchClassifications spans_f = classify_test3.RunTest(ASCIIToUTF16("go"));
  // ACMatch spans should match first two letters of the "goal".
  EXPECT_EQ("0,1,10,3,12,1",
            AutocompleteMatch::ClassificationsToString(spans_f));

  matches = AutocompleteMatch::ClassificationsFromString("0,0,13,1");
  ClassifyTest classify_test4(ASCIIToUTF16("Email login: mail.somecorp.com"),
                              /*text_is_query=*/false, matches);

  ACMatchClassifications spans_g = classify_test4.RunTest(ASCIIToUTF16("ail"));
  EXPECT_EQ("0,0,2,2,5,0,13,1,14,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans_g));

  ACMatchClassifications spans_h =
      classify_test4.RunTest(ASCIIToUTF16("lo log"));
  EXPECT_EQ("0,0,6,2,9,0,13,1",
            AutocompleteMatch::ClassificationsToString(spans_h));

  ACMatchClassifications spans_i =
      classify_test4.RunTest(ASCIIToUTF16("ail em"));
  // 'Email' and 'ail' should be matched.
  EXPECT_EQ("0,2,5,0,13,1,14,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans_i));

  // Some web sites do not have a description.  If the string being searched is
  // empty, the classifications must also be empty: http://crbug.com/148647
  // Extra parens in the next line hack around C++03's "most vexing parse".
  class ClassifyTest classify_test5((base::string16()), /*text_is_query=*/false,
                                    ACMatchClassifications());
  ACMatchClassifications spans_j = classify_test5.RunTest(ASCIIToUTF16("man"));
  ASSERT_EQ(0U, spans_j.size());

  // Matches which end at beginning of classification merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,4,9,0");
  ClassifyTest classify_test6(ASCIIToUTF16("html password example"),
                              /*text_is_query=*/false, matches);

  // Extra space in the next string avoids having the string be a prefix of the
  // text above, which would allow for two different valid classification sets,
  // one of which uses two spans (the first of which would mark all of "html
  // pass" as a match) and one which uses four (which marks the individual words
  // as matches but not the space between them).  This way only the latter is
  // valid.
  ACMatchClassifications spans_k =
      classify_test6.RunTest(ASCIIToUTF16("html  pass"));
  EXPECT_EQ("0,6,4,4,5,6,9,0",
            AutocompleteMatch::ClassificationsToString(spans_k));

  // Multiple matches with both beginning and end at beginning of
  // classifications merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,1,11,0");
  ClassifyTest classify_test7(ASCIIToUTF16("http://a.co is great"),
                              /*text_is_query=*/false, matches);

  ACMatchClassifications spans_l =
      classify_test7.RunTest(ASCIIToUTF16("ht co"));
  EXPECT_EQ("0,3,2,1,9,3,11,0",
            AutocompleteMatch::ClassificationsToString(spans_l));

  // Queries should be classify the same way as google seach autocomplete
  // suggestions.
  matches = AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test8(ASCIIToUTF16("panama canal"),
                              /*text_is_query=*/true, matches);

  ACMatchClassifications spans_m = classify_test8.RunTest(ASCIIToUTF16("pan"));
  // ACMatch spans should be: "---MMMMMMMMM";
  EXPECT_EQ("0,0,3,2", AutocompleteMatch::ClassificationsToString(spans_m));
  ACMatchClassifications spans_n =
      classify_test8.RunTest(ASCIIToUTF16("canal"));
  // ACMatch spans should be: "MMMMMM-----";
  EXPECT_EQ("0,2,7,0", AutocompleteMatch::ClassificationsToString(spans_n));
}

TEST_F(ShortcutsProviderTest, CalculateScore) {
  ShortcutsDatabase::Shortcut shortcut(
      std::string(), ASCIIToUTF16("test"),
      ShortcutsDatabase::Shortcut::MatchCore(
          ASCIIToUTF16("www.test.com"), GURL("http://www.test.com"),
          ASCIIToUTF16("www.test.com"), "0,1,4,3,8,1", ASCIIToUTF16("A test"),
          "0,0,2,2", ui::PAGE_TRANSITION_TYPED,
          AutocompleteMatchType::HISTORY_URL, base::string16()),
      base::Time::Now(), 1);

  // Maximal score.
  const int max_relevance =
      ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance;
  const int kMaxScore = CalculateScore("test", shortcut, max_relevance);

  // Score decreases as percent of the match is decreased.
  int score_three_quarters = CalculateScore("tes", shortcut, max_relevance);
  EXPECT_LT(score_three_quarters, kMaxScore);
  int score_one_half = CalculateScore("te", shortcut, max_relevance);
  EXPECT_LT(score_one_half, score_three_quarters);
  int score_one_quarter = CalculateScore("t", shortcut, max_relevance);
  EXPECT_LT(score_one_quarter, score_one_half);

  // Should decay with time - one week.
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(7);
  int score_week_old = CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_week_old, kMaxScore);

  // Should decay more in two weeks.
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_two_weeks_old = CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_two_weeks_old, score_week_old);

  // But not if it was activly clicked on. 2 hits slow decaying power.
  shortcut.number_of_hits = 2;
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_popular_two_weeks_old =
      CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_two_weeks_old, score_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_popular_two_weeks_old, kMaxScore);

  // 3 hits slow decaying power even more.
  shortcut.number_of_hits = 3;
  shortcut.last_access_time = base::Time::Now() - base::TimeDelta::FromDays(14);
  int score_more_popular_two_weeks_old =
      CalculateScore("test", shortcut, max_relevance);
  EXPECT_LT(score_two_weeks_old, score_more_popular_two_weeks_old);
  EXPECT_LT(score_popular_two_weeks_old, score_more_popular_two_weeks_old);
  // But still decayed.
  EXPECT_LT(score_more_popular_two_weeks_old, kMaxScore);
}

TEST_F(ShortcutsProviderTest, DeleteMatch) {
  TestShortcutData shortcuts_to_test_delete[] = {
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F1", "delete", "www.deletetest.com/1",
       "http://www.deletetest.com/1", "http://www.deletetest.com/1", "0,2",
       "Erase this shortcut!", "0,0", ui::PAGE_TRANSITION_TYPED,
       AutocompleteMatchType::HISTORY_URL, "", 1, 1},
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F2", "erase", "www.deletetest.com/1",
       "http://www.deletetest.com/1", "http://www.deletetest.com/1", "0,2",
       "Erase this shortcut!", "0,0", ui::PAGE_TRANSITION_TYPED,
       AutocompleteMatchType::HISTORY_TITLE, "", 1, 1},
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F3", "keep", "www.deletetest.com/1/2",
       "http://www.deletetest.com/1/2", "http://www.deletetest.com/1/2", "0,2",
       "Keep this shortcut!", "0,0", ui::PAGE_TRANSITION_TYPED,
       AutocompleteMatchType::HISTORY_TITLE, "", 1, 1},
      {"BD85DBA2-8C29-49F9-84AE-48E1E90881F4", "delete", "www.deletetest.com/2",
       "http://www.deletetest.com/2", "http://www.deletetest.com/2", "0,2",
       "Erase this shortcut!", "0,0", ui::PAGE_TRANSITION_TYPED,
       AutocompleteMatchType::HISTORY_URL, "", 1, 1},
  };

  scoped_refptr<ShortcutsBackend> backend = client_->GetShortcutsBackend();
  size_t original_shortcuts_count = backend->shortcuts_map().size();

  PopulateShortcutsBackendWithTestData(backend, shortcuts_to_test_delete,
                                       arraysize(shortcuts_to_test_delete));

  EXPECT_EQ(original_shortcuts_count + 4, backend->shortcuts_map().size());
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(ASCIIToUTF16("delete")));
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(ASCIIToUTF16("erase")));

  AutocompleteMatch match(provider_.get(), 1200, true,
                          AutocompleteMatchType::HISTORY_TITLE);

  match.destination_url = GURL(shortcuts_to_test_delete[0].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[0].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[0].description);

  provider_->DeleteMatch(match);

  // shortcuts_to_test_delete[0] and shortcuts_to_test_delete[1] should be
  // deleted, but not shortcuts_to_test_delete[2] or
  // shortcuts_to_test_delete[3], which have different URLs.
  EXPECT_EQ(original_shortcuts_count + 2, backend->shortcuts_map().size());
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(ASCIIToUTF16("delete")));
  EXPECT_TRUE(backend->shortcuts_map().end() ==
              backend->shortcuts_map().find(ASCIIToUTF16("erase")));

  match.destination_url = GURL(shortcuts_to_test_delete[3].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[3].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[3].description);

  provider_->DeleteMatch(match);
  EXPECT_EQ(original_shortcuts_count + 1, backend->shortcuts_map().size());
  EXPECT_TRUE(backend->shortcuts_map().end() ==
              backend->shortcuts_map().find(ASCIIToUTF16("delete")));
}

TEST_F(ShortcutsProviderTest, DoesNotProvideOnFocus) {
  AutocompleteInput input(ASCIIToUTF16("about:o"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_from_omnibox_focus(true);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}
