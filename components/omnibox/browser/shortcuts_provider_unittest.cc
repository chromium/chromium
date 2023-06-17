// Copyright 2012 The Chromium Authors
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
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/url_database.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_provider_test_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"

using base::ASCIIToUTF16;
using ExpectedURLs = std::vector<ExpectedURLAndAllowedToBeDefault>;

namespace {

// Returns up to 99,999 incrementing GUIDs of the format
// "BD85DBA2-8C29-49F9-84AE-48E1E_____E0".
std::string GetGuid() {
  static int currentGuid = 0;
  currentGuid++;
  DCHECK_LE(currentGuid, 99999);
  return base::StringPrintf("BD85DBA2-8C29-49F9-84AE-48E1E%05dE0", currentGuid);
}

struct TestShortcutData shortcut_test_db[] = {
    {GetGuid(), "goog", "www.google.com", "http://www.google.com/",
     AutocompleteMatch::DocumentType::NONE, "Google", "0,1,4,0", "Google",
     "0,3,4,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL,
     "", 1, 100},
    {GetGuid(), "slash", "slashdot.org", "http://slashdot.org/",
     AutocompleteMatch::DocumentType::NONE, "slashdot.org", "0,3,5,1",
     "Slashdot - News for nerds, stuff that matters", "0,2,5,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 0, 100},
    {GetGuid(), "news", "slashdot.org", "http://slashdot.org/",
     AutocompleteMatch::DocumentType::NONE, "slashdot.org", "0,1",
     "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 0, 5},
    {GetGuid(), "news", "sports.yahoo.com", "http://sports.yahoo.com/",
     AutocompleteMatch::DocumentType::NONE, "sports.yahoo.com", "0,1",
     "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
     "0,0,23,2,27,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 2, 5},
    {GetGuid(), "news weather", "www.cnn.com/index.html",
     "http://www.cnn.com/index.html", AutocompleteMatch::DocumentType::NONE,
     "www.cnn.com/index.html", "0,1",
     "CNN.com - Breaking News, U.S., World, Weather, Entertainment & Video",
     "0,0,19,2,23,0,38,2,45,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 1, 10},
    {GetGuid(), "nhl scores", "sports.yahoo.com", "http://sports.yahoo.com/",
     AutocompleteMatch::DocumentType::NONE, "sports.yahoo.com", "0,1",
     "Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more",
     "0,0,29,2,35,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_BODY, "", 1, 10},
    {GetGuid(), "nhl scores", "www.nhl.com/scores/index.html",
     "http://www.nhl.com/scores/index.html",
     AutocompleteMatch::DocumentType::NONE, "www.nhl.com/scores/index.html",
     "0,1,4,3,7,1", "January 13, 2010 - NHL.com - Scores",
     "0,0,19,2,22,0,29,2,35,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 5, 1},
    {GetGuid(), "just", "www.testsite.com/a.html",
     "http://www.testsite.com/a.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/a.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 5, 1},
    {GetGuid(), "just", "www.testsite.com/b.html",
     "http://www.testsite.com/b.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/b.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 5, 2},
    {GetGuid(), "just", "www.testsite.com/c.html",
     "http://www.testsite.com/c.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/c.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 8, 1},
    {GetGuid(), "just a", "www.testsite.com/d.html",
     "http://www.testsite.com/d.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/d.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {GetGuid(), "just a t", "www.testsite.com/e.html",
     "http://www.testsite.com/e.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/e.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {GetGuid(), "just a te", "www.testsite.com/f.html",
     "http://www.testsite.com/f.html", AutocompleteMatch::DocumentType::NONE,
     "www.testsite.com/f.html", "0,1", "Test - site - just a test",
     "0,0,14,2,18,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_TITLE, "", 12, 1},
    {GetGuid(), "ago", "www.daysagotest.com/a.html",
     "http://www.daysagotest.com/a.html", AutocompleteMatch::DocumentType::NONE,
     "www.daysagotest.com/a.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {GetGuid(), "ago", "www.daysagotest.com/b.html",
     "http://www.daysagotest.com/b.html", AutocompleteMatch::DocumentType::NONE,
     "www.daysagotest.com/b.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 2, 1},
    {GetGuid(), "ago", "www.daysagotest.com/c.html",
     "http://www.daysagotest.com/c.html", AutocompleteMatch::DocumentType::NONE,
     "www.daysagotest.com/c.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 3, 1},
    {GetGuid(), "ago", "www.daysagotest.com/d.html",
     "http://www.daysagotest.com/d.html", AutocompleteMatch::DocumentType::NONE,
     "www.daysagotest.com/d.html", "0,1,8,3,11,1", "Test - site", "0,0",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 4, 1},
    {GetGuid(), "abcdef.com", "http://abcdef.com", "http://abcdef.com/",
     AutocompleteMatch::DocumentType::NONE, "Abcdef", "0,1,4,0", "Abcdef",
     "0,3,4,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL,
     "", 1, 100},
    {GetGuid(), "query", "query", "https://www.google.com/search?q=query",
     AutocompleteMatch::DocumentType::NONE, "query", "0,0", "Google Search",
     "0,4", ui::PAGE_TRANSITION_GENERATED,
     AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100},
    {GetGuid(), "word", "www.word", "https://www.google.com/search?q=www.word",
     AutocompleteMatch::DocumentType::NONE, "www.word", "0,0", "Google Search",
     "0,4", ui::PAGE_TRANSITION_GENERATED,
     AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100},
    {GetGuid(), "about:o", "chrome://omnibox", "chrome://omnibox/",
     AutocompleteMatch::DocumentType::NONE, "about:omnibox", "0,3,10,1", "", "",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::NAVSUGGEST, "", 1, 100},
    {GetGuid(), "www/real sp", "http://www/real space/long-url-with-space.html",
     "http://www/real%20space/long-url-with-space.html",
     AutocompleteMatch::DocumentType::NONE,
     "www/real space/long-url-with-space.html", "0,3,11,1",
     "Page With Space; Input with Space", "0,0", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {GetGuid(), "duplicate", "http://duplicate.com", "http://duplicate.com/",
     AutocompleteMatch::DocumentType::NONE, "Duplicate", "0,1", "Duplicate",
     "0,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "",
     1, 100},
    {GetGuid(), "dupl", "http://duplicate.com", "http://duplicate.com/",
     AutocompleteMatch::DocumentType::NONE, "Duplicate", "0,1", "Duplicate",
     "0,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "",
     1, 100},
    {GetGuid(), "notrailing.com/", "http://notrailing.com",
     "http://notrailing.com/", AutocompleteMatch::DocumentType::NONE,
     "No Trailing Slash", "0,1", "No Trailing Slash on fill_into_edit", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {GetGuid(), "http:///foo.com", "http://foo.com", "http://foo.com/",
     AutocompleteMatch::DocumentType::NONE, "Foo - Typo in Input", "0,1",
     "Foo - Typo in Input Corrected in fill_into_edit", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {GetGuid(), "trailing1 ", "http://trailing1.com", "http://trailing1.com/",
     AutocompleteMatch::DocumentType::NONE, "Trailing1 - Space in Shortcut",
     "0,1", "Trailing1 - Space in Shortcut", "0,1", ui::PAGE_TRANSITION_TYPED,
     AutocompleteMatchType::HISTORY_URL, "", 1, 100},
    {GetGuid(), "about:trailing2 ", "chrome://trailing2blah",
     "chrome://trailing2blah/", AutocompleteMatch::DocumentType::NONE,
     "Trailing2 - Space in Shortcut", "0,1", "Trailing2 - Space in Shortcut",
     "0,1", ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "",
     1, 100},

    // 4 shortcuts to verify aggregating shortcuts.
    {GetGuid(), "wikipedia", "", "https://wikipedia.org/wilson7",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {GetGuid(), "wilson7", "", "https://wikipedia.org/wilson7",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 2, 2},
    {GetGuid(), "winston", "", "https://wikipedia.org/winston",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 3},
    {GetGuid(), "wilson7", "", "https://wikipedia.org/wilson7-other",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 2, 2},

    // 7 shortcuts to verify the interaction of the provider limit and
    // aggregating shortcuts.
    {GetGuid(), "zebra1", "", "https://wikipedia.org/zebra-a",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
    {GetGuid(), "zebra2", "", "https://wikipedia.org/zebra-a",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 2},
    {GetGuid(), "zebra3", "", "https://wikipedia.org/zebra-a",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 3},
    {GetGuid(), "zebra4", "", "https://wikipedia.org/zebra-a",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 4},
    {GetGuid(), "zebra5", "", "https://wikipedia.org/zebra-b",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 6},
    {GetGuid(), "zebra6", "", "https://wikipedia.org/zebra-b",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 4},
    {GetGuid(), "zebra7", "", "https://wikipedia.org/zebra-c",
     AutocompleteMatch::DocumentType::NONE, "", "0,1", "", "0,1",
     ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 10},
};

ShortcutsDatabase::Shortcut MakeShortcut(
    std::u16string text,
    const base::Time& last_access_time = base::Time::Now(),
    int number_of_hits = 1) {
  return {std::string(), text,
          ShortcutsDatabase::Shortcut::MatchCore(
              u"www.test.com", GURL("http://www.test.com"),
              AutocompleteMatch::DocumentType::NONE, u"www.test.com",
              "0,1,4,3,8,1", u"A test", "0,0,2,2", ui::PAGE_TRANSITION_TYPED,
              AutocompleteMatchType::HISTORY_URL, std::u16string()),
          last_access_time, number_of_hits};
}

}  // namespace

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;
  MOCK_METHOD1(DeleteURLs, void(const std::vector<GURL>&));
};

// ShortcutsProviderTest ------------------------------------------------------

class ShortcutsProviderTest : public testing::Test {
 public:
  ShortcutsProviderTest();

 protected:
  void SetUp() override;
  void TearDown() override;

  // Passthrough to the private `CreateScoredShortcutMatch` function in
  // provider_.
  int CalculateAggregateScore(
      size_t input_length,
      const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts);

  // Passthrough to the private `GetMatches`. Enables populating scoring
  // signals.
  void GetMatchesWithScoringSignals(const AutocompleteInput& input);

  // ScopedFeatureList needs to be defined before TaskEnvironment, so that it is
  // destroyed after TaskEnvironment, to prevent data races on the
  // ScopedFeatureList.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<ShortcutsProvider> provider_;
};

ShortcutsProviderTest::ShortcutsProviderTest() {
  // `scoped_feature_list_` needs to be initialized as early as possible, to
  // avoid data races caused by tasks on other threads accessing it.
  scoped_feature_list_.Reset();
  // Even though these are enabled by default on desktop, they aren't enabled by
  // default on mobile. To avoid having 2 sets of tests around, explicitly
  // enable them for all platforms for tests.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox::kRichAutocompletion,
      {{"RichAutocompletionAutocompleteTitlesShortcutProvider", "true"},
       {"RichAutocompletionAutocompleteTitlesMinChar", "3"},
       {"RichAutocompletionAutocompleteShortcutText", "true"},
       {"RichAutocompletionAutocompleteShortcutTextMinChar", "3"}});
  RichAutocompletionParams::ClearParamsForTesting();
}

void ShortcutsProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();
  client_->set_history_service(std::make_unique<MockHistoryService>());
  auto shortcuts_backend = base::MakeRefCounted<ShortcutsBackend>(
      client_->GetTemplateURLService(), std::make_unique<SearchTermsData>(),
      client_->GetHistoryService(), base::FilePath(), true);
  shortcuts_backend->Init();
  client_->set_shortcuts_backend(std::move(shortcuts_backend));

  ASSERT_TRUE(client_->GetShortcutsBackend());
  provider_ = base::MakeRefCounted<ShortcutsProvider>(client_.get());
  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       shortcut_test_db,
                                       std::size(shortcut_test_db));
}

void ShortcutsProviderTest::TearDown() {
  provider_ = nullptr;
  client_.reset();
  task_environment_.RunUntilIdle();
  scoped_feature_list_.Reset();
  RichAutocompletionParams::ClearParamsForTesting();
}

int ShortcutsProviderTest::CalculateAggregateScore(
    size_t input_length,
    const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts) {
  const int max_relevance =
      ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance;
  return provider_
      ->CreateScoredShortcutMatch(input_length,
                                  /*stripped_destination_url=*/GURL(),
                                  shortcuts, max_relevance)
      .relevance;
}

void ShortcutsProviderTest::GetMatchesWithScoringSignals(
    const AutocompleteInput& input) {
  provider_->matches_.clear();
  provider_->GetMatches(input, /*populate_scoring_signals=*/true);
}

// Actual tests ---------------------------------------------------------------

TEST_F(ShortcutsProviderTest, SimpleSingleMatch) {
  std::u16string text(u"go");
  std::string expected_url("http://www.google.com/");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"ogle");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"ogle.com");

  // A pair of analogous tests where the shortcut ends at the end of
  // |fill_into_edit|.  This exercises the inline autocompletion and default
  // match code.
  text = u"abcdef.com";
  expected_url = "http://abcdef.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           std::u16string());
  // With prevent inline autocomplete, the suggestion should be the same
  // (because there is no completion).
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           std::u16string());

  // Another test, simply for a query match type, not a navigation URL match
  // type.
  text = u"que";
  expected_url = "https://www.google.com/search?q=query";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"ry");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"ry");

  // A pair of analogous tests where the shortcut ends at the end of
  // |fill_into_edit|.  This exercises the inline autocompletion and default
  // match code.
  text = u"query";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           std::u16string());
  // With prevent inline autocomplete, the suggestion should be the same
  // (because there is no completion).
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           std::u16string());

  // Now the shortcut ends at the end of |fill_into_edit| but has a
  // non-droppable prefix.  ("www.", for instance, is not droppable for
  // queries.)
  text = u"word";
  expected_url = "https://www.google.com/search?q=www.word";
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           std::u16string());
}

// These tests are like those in SimpleSingleMatch but more complex,
// involving URLs that need to be fixed up to match properly.
TEST_F(ShortcutsProviderTest, TrickySingleMatch) {
  // Test that about: URLs are fixed up/transformed to chrome:// URLs.
  std::u16string text(u"about:o");
  std::string expected_url("chrome://omnibox/");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"mnibox");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"mnibox");

  // Test that an input with a space can match URLs with a (escaped) space.
  // This would fail if we didn't try to lookup the un-fixed-up string.
  text = u"www/real sp";
  expected_url = "http://www/real%20space/long-url-with-space.html";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"ace/long-url-with-space.html");

  // Same test with prevent inline autocomplete.
  expected_urls.clear();
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault(expected_url, false));
  // The match will have an |inline_autocompletion| set, but the value will not
  // be used because |allowed_to_be_default_match| will be false.
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           u"ace/long-url-with-space.html");

  // Test when the user input has a typo that can be fixed up for matching
  // fill_into_edit.  This should still be allowed to be default.
  text = u"http:///foo.com";
  expected_url = "http://foo.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, true, expected_urls, expected_url,
                           std::u16string());

  // A foursome of tests to verify that trailing spaces does not prevent the
  // shortcut from being allowed to be the default match. For each of two tests,
  // we try the input with and without the trailing whitespace.
  text = u"trailing1";
  expected_url = "http://trailing1.com/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u" - Space in Shortcut");
  text = u"trailing1 ";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"- Space in Shortcut");
  text = u"about:trailing2";
  expected_url = "chrome://trailing2blah/";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u" ");
  text = u"about:trailing2 ";
  expected_urls.clear();
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(expected_url, true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls, expected_url,
                           u"");
}

TEST_F(ShortcutsProviderTest, SimpleSingleMatchKeyword) {
  // Add a non-default search engine.
  TemplateURLData data;
  data.SetShortName(u"yahoo");
  data.SetKeyword(u"yahoo.com");
  data.SetURL("http://www.yahoo.com/{searchTerms}");
  client_->GetTemplateURLService()->Add(std::make_unique<TemplateURL>(data));

  // Add 4 shortcuts with various keyword states to the db.
  const auto create_keyword_shortcut =
      [&](std::string fill_into_edit, std::string keyword,
          std::string destination_url, bool explicit_keyword,
          bool search) -> TestShortcutData {
    const ui::PageTransition transition =
        explicit_keyword ? ui::PageTransition(ui::PAGE_TRANSITION_TYPED |
                                              ui::PAGE_TRANSITION_KEYWORD)
                         : ui::PAGE_TRANSITION_TYPED;
    return {GetGuid(),
            fill_into_edit,
            fill_into_edit,
            destination_url,
            AutocompleteMatch::DocumentType::NONE,
            "",
            "0,1",
            "",
            "0,1",
            transition,
            search ? AutocompleteMatchType::SEARCH_HISTORY
                   : AutocompleteMatchType::HISTORY_URL,
            keyword,
            1,
            10};
  };
  TestShortcutData shortcuts[] = {
      create_keyword_shortcut("yahoo.com explicit keyword", "yahoo.com",
                              "https://yahoo.com/explicit-keyword", true, true),
      create_keyword_shortcut("google.com non-explicit keyword", "google.com",
                              "https://google.com/non-explicit-keyword", false,
                              true),
      create_keyword_shortcut("google.com navigation", "",
                              "https://google.com/navigation", false, false),
      create_keyword_shortcut("yahoo.com search on google.com", "google.com",
                              "https://google.com/q=yahoo.com", true, true),
  };
  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       shortcuts, std::size(shortcuts));

  const auto test = [&](const std::u16string text, bool prefer_keyword,
                        std::string expected_url, bool allowed_to_be_default,
                        std::u16string expected_autocompletion) {
    AutocompleteInput input(text, metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_prefer_keyword(prefer_keyword);

    ExpectedURLs expected_urls;
    expected_urls.push_back(
        ExpectedURLAndAllowedToBeDefault(expected_url, allowed_to_be_default));

    RunShortcutsProviderTest(provider_, input, expected_urls, expected_url,
                             expected_autocompletion);
  };

  // When the input is in keyword mode, a match with the same keyword may be
  // default.
  test(u"yahoo.com exp", true, "https://yahoo.com/explicit-keyword", true,
       u"licit keyword");

  // When the input is in keyword mode, a match with a different keyword can not
  // be default.
  test(u"yahoo.com search ", true, "https://google.com/q=yahoo.com", false,
       u"");

  // When the input is in keyword mode, a match without a keyword can not be
  // default.
  test(u"google.com navigat", true, "https://google.com/navigation", false,
       u"");

  // When the input is in keyword mode, a match with a keyword hint can not be
  // default.
  test(u"google.com non-e", true, "https://google.com/non-explicit-keyword",
       false, u"");

  // When the input is NOT in keyword mode, a match with a keyword can not be
  // default.
  test(u"yahoo.com ex", false, "https://yahoo.com/explicit-keyword", false,
       u"");

  // When the input is NOT in keyword mode, a match with a keyword hint can be
  // default.
  test(u"google.com non-ex", false, "https://google.com/non-explicit-keyword",
       true, u"plicit keyword");
}

TEST_F(ShortcutsProviderTest, MultiMatch) {
  std::u16string text(u"NEWS");
  ExpectedURLs expected_urls;
  // Scores high because of completion length.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://slashdot.org/", true));
  // Scores high because of visit count.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://sports.yahoo.com/", true));
  // Scores high because of visit count but less match span,
  // which is more important.
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.cnn.com/index.html", true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://slashdot.org/", std::u16string());
}

TEST_F(ShortcutsProviderTest, RemoveDuplicates) {
  std::u16string text(u"dupl");
  ExpectedURLs expected_urls;
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://duplicate.com/", true));
  // Make sure the URL only appears once in the output list.
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://duplicate.com/", u"icate");
}

TEST_F(ShortcutsProviderTest, TypedCountMatches) {
  std::u16string text(u"just");
  ExpectedURLs expected_urls;
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.testsite.com/b.html", true));
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.testsite.com/a.html", true));
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.testsite.com/c.html", true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.testsite.com/b.html", std::u16string());
}

TEST_F(ShortcutsProviderTest, FragmentLengthMatches) {
  std::u16string text(u"just a");
  ExpectedURLs expected_urls;
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.testsite.com/d.html", true));
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.testsite.com/e.html", true));
  expected_urls.push_back(
      ExpectedURLAndAllowedToBeDefault("http://www.testsite.com/f.html", true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.testsite.com/d.html", std::u16string());
}

TEST_F(ShortcutsProviderTest, DaysAgoMatches) {
  std::u16string text(u"ago");
  ExpectedURLs expected_urls;
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/a.html", true));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/b.html", true));
  expected_urls.push_back(ExpectedURLAndAllowedToBeDefault(
      "http://www.daysagotest.com/c.html", true));
  RunShortcutsProviderTest(provider_, text, false, expected_urls,
                           "http://www.daysagotest.com/a.html",
                           std::u16string());
}

TEST_F(ShortcutsProviderTest, DeleteMatch) {
  TestShortcutData shortcuts_to_test_delete[] = {
      {GetGuid(), "delete", "www.deletetest.com/1",
       "http://www.deletetest.com/1", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/1", "0,2", "Erase this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
      {GetGuid(), "erase", "www.deletetest.com/1",
       "http://www.deletetest.com/1", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/1", "0,2", "Erase this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 1,
       1},
      {GetGuid(), "keep", "www.deletetest.com/1/2",
       "http://www.deletetest.com/1/2", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/1/2", "0,2", "Keep this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_TITLE, "", 1,
       1},
      {GetGuid(), "delete", "www.deletetest.com/2",
       "http://www.deletetest.com/2", AutocompleteMatch::DocumentType::NONE,
       "http://www.deletetest.com/2", "0,2", "Erase this shortcut!", "0,0",
       ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 1, 1},
  };

  scoped_refptr<ShortcutsBackend> backend = client_->GetShortcutsBackend();
  size_t original_shortcuts_count = backend->shortcuts_map().size();

  PopulateShortcutsBackendWithTestData(backend, shortcuts_to_test_delete,
                                       std::size(shortcuts_to_test_delete));

  EXPECT_EQ(original_shortcuts_count + 4, backend->shortcuts_map().size());
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(u"delete"));
  EXPECT_FALSE(backend->shortcuts_map().end() ==
               backend->shortcuts_map().find(u"erase"));

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
               backend->shortcuts_map().find(u"delete"));
  EXPECT_TRUE(backend->shortcuts_map().end() ==
              backend->shortcuts_map().find(u"erase"));

  match.destination_url = GURL(shortcuts_to_test_delete[3].destination_url);
  match.contents = ASCIIToUTF16(shortcuts_to_test_delete[3].contents);
  match.description = ASCIIToUTF16(shortcuts_to_test_delete[3].description);

  provider_->DeleteMatch(match);
  EXPECT_EQ(original_shortcuts_count + 1, backend->shortcuts_map().size());
  EXPECT_TRUE(backend->shortcuts_map().end() ==
              backend->shortcuts_map().find(u"delete"));
}

TEST_F(ShortcutsProviderTest, DoesNotProvideOnFocus) {
  AutocompleteInput input(u"about:o", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ShortcutsProviderTest, GetMatches) {
  {
    // When multiple shortcuts with the same destination URL match the input,
    // they should be scored together (i.e. their visit counts summed, the most
    // recent visit date and shortest text considered).
    AutocompleteInput input(u"wi", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 3u);
    // There are 2 shortcuts with the wilson7 url which have the same aggregate
    // text length, visit count, and last visit as the 1 winston shortcut.
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://wikipedia.org/winston");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://wikipedia.org/wilson7");
    // Matches with the same score otherwise, are demoted by 1, hence the `+ 1`.
    EXPECT_EQ(matches[0].relevance, matches[1].relevance + 1);
    EXPECT_EQ(matches[2].destination_url.spec(),
              "https://wikipedia.org/wilson7-other");
    EXPECT_GT(matches[0].relevance, matches[2].relevance + 1);
    EXPECT_GT(matches[2].relevance, 0);
  }

  {
    // When multiple shortcuts have the same destination URL but only 1 matches
    // the input, they should not be scored together.
    AutocompleteInput input(u"wilson", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://wikipedia.org/wilson7");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://wikipedia.org/wilson7-other");
    EXPECT_EQ(matches[0].relevance, matches[1].relevance + 1);
    EXPECT_GT(matches[1].relevance, 0);
  }

  {
    // The provider limit should not affect number of shortcuts aggregated, only
    // the matches returned, i.e. the number of aggregate shortcuts. There are
    // 7 shortcuts matching the input with 3 unique URLs. The 3 aggregate
    // shortcuts have the same aggregate score factors and should be scored the
    // same (other than the `+ 1` limitation).
    AutocompleteInput input(u"zebra", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 3u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://wikipedia.org/zebra-c");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://wikipedia.org/zebra-a");
    EXPECT_EQ(matches[2].destination_url.spec(),
              "https://wikipedia.org/zebra-b");
    EXPECT_EQ(matches[0].relevance, matches[1].relevance + 1);
    EXPECT_EQ(matches[1].relevance, matches[2].relevance + 1);
    EXPECT_GT(matches[2].relevance, 0);
  }
}

TEST_F(ShortcutsProviderTest, GetMatchesWithScoringSignals) {
  // When multiple shortcuts with the same destination URL match the input,
  // they should be scored together (i.e. their visit counts summed, the most
  // recent visit date and shortest text considered).
  AutocompleteInput input(u"wi", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  GetMatchesWithScoringSignals(input);
  auto& matches = provider_->matches();
  EXPECT_EQ(matches.size(), 3u);
  // These matches are all HISTORY_URL type, so should have scoring signals
  // attached.
  EXPECT_TRUE(AutocompleteScoringSignalsAnnotator::IsEligibleMatch(matches[0]));
  EXPECT_TRUE(matches[0].scoring_signals.has_value());
  EXPECT_TRUE(AutocompleteScoringSignalsAnnotator::IsEligibleMatch(matches[1]));
  EXPECT_TRUE(matches[1].scoring_signals.has_value());
  EXPECT_TRUE(AutocompleteScoringSignalsAnnotator::IsEligibleMatch(matches[2]));
  EXPECT_TRUE(matches[2].scoring_signals.has_value());
  // There are 2 shortcuts with the wilson7 url which have the same aggregate
  // text length, visit count, and last visit as the 1 winston shortcut.
  EXPECT_EQ(matches[0].scoring_signals->shortcut_visit_count(), 3);
  EXPECT_EQ(matches[0].scoring_signals->shortest_shortcut_len(), 7);

  EXPECT_EQ(matches[1].scoring_signals->shortcut_visit_count(), 3);
  EXPECT_EQ(matches[1].scoring_signals->shortest_shortcut_len(), 7);

  EXPECT_EQ(matches[2].scoring_signals->shortcut_visit_count(), 2);
  EXPECT_EQ(matches[2].scoring_signals->shortest_shortcut_len(), 7);

  // Check again with an ineligible (SEARCH_HISTORY) type match and confirm
  // that the match does not have scoring signals attached.
  AutocompleteInput input2(u"que", metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  GetMatchesWithScoringSignals(input2);
  EXPECT_EQ(matches.size(), 1u);

  EXPECT_FALSE(
      AutocompleteScoringSignalsAnnotator::IsEligibleMatch(matches[0]));
  EXPECT_FALSE(matches[0].scoring_signals.has_value());
}

TEST_F(ShortcutsProviderTest, Score) {
  const auto days_ago = [](int n) { return base::Time::Now() - base::Days(n); };

  // Aggregate score should consider the shortest text length, most recent visit
  // time, and sum of visit counts.
  auto shortcut_a_short = MakeShortcut(u"size______12", days_ago(3), 1);
  auto shortcut_a_frequent = MakeShortcut(u"size__________16", days_ago(3), 10);
  auto shortcut_a_recent = MakeShortcut(u"size__________16", days_ago(1), 1);
  auto score_a = CalculateAggregateScore(
      1, {&shortcut_a_short, &shortcut_a_frequent, &shortcut_a_recent});
  auto shortcut_b = MakeShortcut(u"size______12", days_ago(1), 12);
  auto score_b = CalculateAggregateScore(1, {&shortcut_b});
  EXPECT_EQ(score_a, score_b);
  EXPECT_GT(score_a, 0);

  // Typing more of the text increases score.
  auto score_b_long_query = CalculateAggregateScore(2, {&shortcut_b});
  EXPECT_GT(score_b_long_query, score_b);

  // When creating or updating shortcuts, their text is set longer than the user
  // input (see `ShortcutBackend::AddOrUpdateShortcut()`). So `CalculateScore()`
  // permits up to 10 missing chars before beginning to decrease scores.
  EXPECT_EQ(CalculateAggregateScore(6, {&shortcut_a_frequent}),
            CalculateAggregateScore(14, {&shortcut_a_frequent}));

  // Make sure there's no negative or weird scores when the shortcut text is
  // shorter than the 10 char adjustment.
  const auto shortcut = MakeShortcut(u"test");
  const int kMaxScore = CalculateAggregateScore(4, {&shortcut});
  const auto short_shortcut = MakeShortcut(u"ab");
  EXPECT_EQ(CalculateAggregateScore(2, {&short_shortcut}), kMaxScore);
  EXPECT_EQ(CalculateAggregateScore(1, {&short_shortcut}), kMaxScore);

  // More recent shortcuts should be scored higher.
  auto shortcut_b_old = MakeShortcut(u"size______12", days_ago(2), 12);
  auto score_b_old = CalculateAggregateScore(1, {&shortcut_b_old});
  EXPECT_LT(score_b_old, score_b);

  // Shortcuts with higher visit counts should be scored higher.
  auto shortcut_b_frequent = MakeShortcut(u"size______12", days_ago(1), 13);
  auto score_b_frequent = CalculateAggregateScore(1, {&shortcut_b_frequent});
  EXPECT_GT(score_b_frequent, score_b);
}

TEST_F(ShortcutsProviderTest, ScoreBoost) {
  // The max score a shortcut can have if not boosted.
  const int kMaxUnboostedScore = 1199;

  auto create_shortcut_data = [](std::string text, bool is_search,
                                 int visit_count) -> TestShortcutData {
    std::string desitnation_string =
        "https://" + text + ".com/" + base::NumberToString(visit_count);
    return {GetGuid(),
            text,
            text,
            desitnation_string,
            AutocompleteMatch::DocumentType::NONE,
            "",
            "",
            "",
            "",
            ui::PageTransition::PAGE_TRANSITION_TYPED,
            is_search ? AutocompleteMatchType::SEARCH_SUGGEST
                      : AutocompleteMatchType::HISTORY_URL,
            is_search ? "google" : "",
            1,
            visit_count * 1};
  };

  TestShortcutData shortcut_data[] = {
      create_shortcut_data("only-searches", true, 1),
      create_shortcut_data("only-urls", false, 2),
      create_shortcut_data("only-urls", false, 1),
      create_shortcut_data("searches-before-urls", true, 2),
      create_shortcut_data("searches-before-urls", false, 1),
      create_shortcut_data("urls-before-searches", false, 2),
      create_shortcut_data("urls-before-searches", true, 1),
  };

  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       shortcut_data, std::size(shortcut_data));

  OmniboxTriggeredFeatureService* trigger_service =
      client_->GetOmniboxTriggeredFeatureService();
  OmniboxTriggeredFeatureService::Feature trigger_feature =
      metrics::OmniboxEventProto_Feature_SHORTCUT_BOOST;

  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      omnibox_feature_configs::ShortcutBoosting::kShortcutBoost,
      {{"ShortcutBoostUrlScore", "1300"}});
  omnibox_feature_configs::ScopedConfigForTesting<
      omnibox_feature_configs::ShortcutBoosting>
      scoped_config;

  {
    // Searches shouldn't be boosted since the appropriate param is not set.
    trigger_service->ResetSession();
    AutocompleteInput input(u"only-searches", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 1u);
    EXPECT_EQ(matches[0].destination_url.spec(), "https://only-searches.com/1");
    EXPECT_LE(matches[0].relevance, kMaxUnboostedScore);
    EXPECT_FALSE(
        trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  }

  {
    // Only the 1st URL should be boosted.
    trigger_service->ResetSession();
    AutocompleteInput input(u"only-urls", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].destination_url.spec(), "https://only-urls.com/2");
    EXPECT_EQ(matches[1].destination_url.spec(), "https://only-urls.com/1");
    EXPECT_EQ(matches[0].relevance, 1300);
    EXPECT_LE(matches[1].relevance, kMaxUnboostedScore);
    EXPECT_TRUE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  }

  {
    // URLs should only boosted if they're 1st of all matches (including
    // searches).
    trigger_service->ResetSession();
    AutocompleteInput input(u"searches-before-urls",
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://searches-before-urls.com/2");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://searches-before-urls.com/1");
    EXPECT_LE(matches[0].relevance, kMaxUnboostedScore);
    EXPECT_LE(matches[1].relevance, kMaxUnboostedScore);
    EXPECT_FALSE(
        trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  }

  {
    // URLs should only boosted if they're 1st of all matches (including
    // searches).
    trigger_service->ResetSession();
    AutocompleteInput input(u"urls-before-searches",
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://urls-before-searches.com/2");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://urls-before-searches.com/1");
    EXPECT_EQ(matches[0].relevance, 1300);
    EXPECT_LE(matches[1].relevance, kMaxUnboostedScore);
    EXPECT_TRUE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  }

  {
    // Should not boost when counterfactual is enabled.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        omnibox_feature_configs::ShortcutBoosting::kShortcutBoost,
        {{"ShortcutBoostUrlScore", "1300"},
         {"ShortcutBoostCounterfactual", "true"}});
    scoped_config.Reset();

    trigger_service->ResetSession();
    AutocompleteInput input(u"urls-before-searches",
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    const auto& matches = provider_->matches();
    EXPECT_EQ(matches.size(), 2u);
    EXPECT_EQ(matches[0].destination_url.spec(),
              "https://urls-before-searches.com/2");
    EXPECT_EQ(matches[1].destination_url.spec(),
              "https://urls-before-searches.com/1");
    EXPECT_LE(matches[0].relevance, kMaxUnboostedScore);
    EXPECT_LE(matches[1].relevance, kMaxUnboostedScore);
    EXPECT_TRUE(trigger_service->GetFeatureTriggeredInSession(trigger_feature));
  }
}

#if !BUILDFLAG(IS_IOS)
TEST_F(ShortcutsProviderTest, HistoryClusterSuggestions) {
  const auto create_test_data =
      [](std::string text, bool is_history_cluster) -> TestShortcutData {
    return {GetGuid(), text, "fill_into_edit",
            // Use unique URLs to avoid deduping.
            "http://www.destination_url.com/" + text,
            AutocompleteMatch::DocumentType::NONE, "contents", "0,0",
            "description", "0,0", ui::PAGE_TRANSITION_TYPED,
            is_history_cluster ? AutocompleteMatchType::HISTORY_CLUSTER
                               : AutocompleteMatchType::HISTORY_URL,
            /*keyword=*/"",
            /*days_from_now=*/1,
            /*number_of_hits=*/1};
  };
  // `provider_max_matches_` is 3. Create more than 3 cluster and non-cluster
  // shortcuts.
  TestShortcutData test_data[] = {
      create_test_data("text_history_0", false),
      create_test_data("text_history_1", false),
      create_test_data("text_history_2", false),
      create_test_data("text_history_3", false),
      create_test_data("text_cluster_0", true),
      create_test_data("text_cluster_1", true),
      create_test_data("text_cluster_2", true),
      create_test_data("text_cluster_3", true),
  };
  PopulateShortcutsBackendWithTestData(client_->GetShortcutsBackend(),
                                       test_data, std::size(test_data));

  AutocompleteInput input(u"tex", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider_->Start(input, false);
  const auto matches = provider_->matches();

  // Expect 3 (i.e. `provider_max_matches_`) non-cluster matches, and all
  // cluster matches. Expect only the non-cluster matches to be allowed to be
  // default.
  ASSERT_EQ(matches.size(), 7u);
  EXPECT_EQ(matches[0].type, AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(matches[0].allowed_to_be_default_match, true);
  EXPECT_EQ(matches[1].type, AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(matches[1].allowed_to_be_default_match, true);
  EXPECT_EQ(matches[2].type, AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(matches[2].allowed_to_be_default_match, true);
  EXPECT_EQ(matches[3].type, AutocompleteMatchType::HISTORY_CLUSTER);
  EXPECT_EQ(matches[3].allowed_to_be_default_match, false);
  EXPECT_EQ(matches[4].type, AutocompleteMatchType::HISTORY_CLUSTER);
  EXPECT_EQ(matches[4].allowed_to_be_default_match, false);
  EXPECT_EQ(matches[5].type, AutocompleteMatchType::HISTORY_CLUSTER);
  EXPECT_EQ(matches[5].allowed_to_be_default_match, false);
  EXPECT_EQ(matches[6].type, AutocompleteMatchType::HISTORY_CLUSTER);
  EXPECT_EQ(matches[6].allowed_to_be_default_match, false);

  // Expect only non-cluster matches to have capped decrementing scores.
  EXPECT_EQ(matches[1].relevance, matches[0].relevance - 1);
  EXPECT_EQ(matches[2].relevance, matches[0].relevance - 2);
  EXPECT_EQ(matches[3].relevance, matches[0].relevance);
  EXPECT_EQ(matches[4].relevance, matches[0].relevance);
  EXPECT_EQ(matches[5].relevance, matches[0].relevance);
  EXPECT_EQ(matches[6].relevance, matches[0].relevance);

  // Expect cluster matches to not have grouping.
  EXPECT_EQ(matches[0].suggestion_group_id, absl::nullopt);
  EXPECT_EQ(matches[1].suggestion_group_id, absl::nullopt);
  EXPECT_EQ(matches[2].suggestion_group_id, absl::nullopt);
  EXPECT_EQ(matches[3].suggestion_group_id, absl::nullopt);
  EXPECT_EQ(matches[4].suggestion_group_id, absl::nullopt);
  EXPECT_EQ(matches[5].suggestion_group_id, absl::nullopt);
  EXPECT_EQ(matches[6].suggestion_group_id, absl::nullopt);
}
#endif  // !BUILDFLAG(IS_IOS)
