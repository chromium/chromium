// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/history_quick_provider.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/history_test_util.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

using base::ASCIIToUTF16;

namespace {

// Waits for OnURLsDeletedNotification and when run quits the supplied run loop.
class WaitForURLsDeletedObserver : public history::HistoryServiceObserver {
 public:
  explicit WaitForURLsDeletedObserver(base::RunLoop* runner);
  ~WaitForURLsDeletedObserver() override;
  WaitForURLsDeletedObserver(const WaitForURLsDeletedObserver&) = delete;
  WaitForURLsDeletedObserver& operator=(const WaitForURLsDeletedObserver&) =
      delete;

 private:
  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* service,
                          const history::DeletionInfo& deletion_info) override;

  // Weak. Owned by our owner.
  raw_ptr<base::RunLoop> runner_;
};

WaitForURLsDeletedObserver::WaitForURLsDeletedObserver(base::RunLoop* runner)
    : runner_(runner) {}

WaitForURLsDeletedObserver::~WaitForURLsDeletedObserver() = default;

void WaitForURLsDeletedObserver::OnHistoryDeletions(
    history::HistoryService* service,
    const history::DeletionInfo& deletion_info) {
  runner_->Quit();
}

void WaitForURLsDeletedNotification(history::HistoryService* history_service) {
  base::RunLoop runner;
  WaitForURLsDeletedObserver observer(&runner);
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(history_service);
  runner.Run();
}

// Post history_backend->GetURL() to the history thread and stop the originating
// thread's message loop when done.
class GetURLTask : public history::HistoryDBTask {
 public:
  GetURLTask(const GURL& url,
             bool* result_storage,
             base::OnceClosure quit_closure)
      : result_storage_(result_storage),
        url_(url),
        quit_closure_(std::move(quit_closure)) {}
  GetURLTask(const GetURLTask&) = delete;
  GetURLTask& operator=(const GetURLTask&) = delete;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    *result_storage_ = backend->GetURL(url_, nullptr);
    return true;
  }

  void DoneRunOnMainThread() override { std::move(quit_closure_).Run(); }

 private:
  ~GetURLTask() override = default;

  raw_ptr<bool> result_storage_;
  const GURL url_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class HistoryQuickProviderTest : public testing::Test {
 public:
  HistoryQuickProviderTest() = default;
  HistoryQuickProviderTest(const HistoryQuickProviderTest&) = delete;
  HistoryQuickProviderTest& operator=(const HistoryQuickProviderTest&) = delete;

 protected:
  struct TestURLInfo {
    std::string url;
    std::string title;
    int visit_count;
    int typed_count;
    int days_from_now;
  };

  class SetShouldContain {
   public:
    explicit SetShouldContain(const ACMatches& matched_urls);

    void operator()(const std::string& expected);

    std::set<std::string> LeftOvers() const { return matches_; }

   private:
    std::set<std::string> matches_;
  };

  void SetUp() override;
  void TearDown() override;

  virtual std::vector<TestURLInfo> GetTestData();

  // Fills test data into the history system.
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const std::u16string& text,
               bool prevent_inline_autocomplete,
               const std::vector<std::string>& expected_urls,
               bool expected_can_inline_top_result,
               const std::u16string& expected_fill_into_edit,
               const std::u16string& autocompletion);

  // As above, simply with a cursor position specified.
  void RunTestWithCursor(const std::u16string& text,
                         const size_t cursor_position,
                         bool prevent_inline_autocomplete,
                         const std::vector<std::string>& expected_urls,
                         bool expected_can_inline_top_result,
                         const std::u16string& expected_fill_into_edit,
                         const std::u16string& autocompletion,
                         bool duplicates_ok = false);

  // TODO(shess): From history_service.h in reference to history_backend:
  // > This class has most of the implementation and runs on the 'thread_'.
  // > You MUST communicate with this class ONLY through the thread_'s
  // > message_loop().
  // Direct use of this object in tests is almost certainly not thread-safe.
  history::HistoryBackend* history_backend() {
    return client_->GetHistoryService()->history_backend_.get();
  }

  // Call history_backend()->GetURL(url, NULL) on the history thread, returning
  // the result.
  bool GetURLProxy(const GURL& url);

  FakeAutocompleteProviderClient& client() { return *client_; }
  ACMatches& ac_matches() { return ac_matches_; }
  HistoryQuickProvider& provider() { return *provider_; }

  AutocompleteMatch QuickMatchToACMatch(const ScoredHistoryMatch& history_match,
                                        int score) {
    return provider_->QuickMatchToACMatch(history_match, score);
  }

 private:
  base::test::ScopedFeatureList feature_list_{omnibox::kDomainSuggestions};

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

  scoped_refptr<HistoryQuickProvider> provider_;
};

void HistoryQuickProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();
  CHECK(history_dir_.CreateUniqueTempDir());

  // Initialize the history service with our test data.
  client_->set_history_service(
      history::CreateHistoryService(history_dir_.GetPath(), true));
  ASSERT_NE(client_->GetHistoryService(), nullptr);
  ASSERT_NO_FATAL_FAILURE(FillData());

  client_->set_bookmark_model(bookmarks::TestBookmarkClient::CreateModel());
  client_->set_in_memory_url_index(std::make_unique<InMemoryURLIndex>(
      client_->GetBookmarkModel(), client_->GetHistoryService(), nullptr,
      history_dir_.GetPath(), SchemeSet()));
  client_->GetInMemoryURLIndex()->Init();

  // Block until History has processed InMemoryURLIndex initialization.
  history::BlockUntilHistoryProcessesPendingRequests(
      client_->GetHistoryService());
  ASSERT_TRUE(client_->GetInMemoryURLIndex()->restored());

  provider_ = new HistoryQuickProvider(client_.get());
}

void HistoryQuickProviderTest::TearDown() {
  ac_matches_.clear();
  provider_ = nullptr;
  client_.reset();
  task_environment_.RunUntilIdle();
}

std::vector<HistoryQuickProviderTest::TestURLInfo>
HistoryQuickProviderTest::GetTestData() {
  return {
      {"http://www.google.com/", "Google", 3, 3, 0},
      {"http://slashdot.org/favorite_page.html", "Favorite page", 200, 100, 0},
      {"http://kerneltrap.org/not_very_popular.html", "Less popular", 4, 0, 0},
      {"http://freshmeat.net/unpopular.html", "Unpopular", 1, 1, 0},
      {"http://news.google.com/?ned=us&topic=n", "Google News - U.S.", 2, 2, 0},
      {"http://news.google.com/", "Google News", 1, 1, 0},
      {"http://foo.com/", "Dir", 200, 100, 0},
      {"http://foo.com/dir/", "Dir", 2, 1, 10},
      {"http://foo.com/dir/another/", "Dir", 10, 5, 0},
      {"http://foo.com/dir/another/again/", "Dir", 5, 1, 0},
      {"http://foo.com/dir/another/again/myfile.html", "File", 3, 1, 0},
      {"http://visitedest.com/y/a", "VA", 10, 1, 20},
      {"http://visitedest.com/y/b", "VB", 9, 1, 20},
      {"http://visitedest.com/x/c", "VC", 8, 1, 20},
      {"http://visitedest.com/x/d", "VD", 7, 1, 20},
      {"http://visitedest.com/y/e", "VE", 6, 1, 20},
      {"http://typeredest.com/y/a", "TA", 5, 5, 0},
      {"http://typeredest.com/y/b", "TB", 5, 4, 0},
      {"http://typeredest.com/x/c", "TC", 5, 3, 0},
      {"http://typeredest.com/x/d", "TD", 5, 2, 0},
      {"http://typeredest.com/y/e", "TE", 5, 1, 0},
      {"http://daysagoest.com/y/a", "DA", 1, 1, 0},
      {"http://daysagoest.com/y/b", "DB", 1, 1, 1},
      {"http://daysagoest.com/x/c", "DC", 1, 1, 2},
      {"http://daysagoest.com/x/d", "DD", 1, 1, 3},
      {"http://daysagoest.com/y/e", "DE", 1, 1, 4},
      {"http://abcdefghixyzjklmnopqrstuvw.com/a", "", 3, 1, 0},
      {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2, 0},
      {"http://abcdefghijklxyzmnopqrstuvw.com/a", "", 3, 1, 0},
      {"http://abcdefxyzghijklmnopqrstuvw.com/a", "", 3, 1, 0},
      {"http://abcxyzdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
      {"http://xyzabcdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
      {"http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice",
       "Dogs & Cats & Mice & Other Animals", 1, 1, 0},
      {"https://monkeytrap.org/", "", 3, 1, 0},
      {"http://popularsitewithpathonly.com/moo",
       "popularsitewithpathonly.com/moo", 50, 50, 0},
      {"http://popularsitewithroot.com/", "popularsitewithroot.com", 50, 50, 0},
      {"http://testsearch.com/?q=thequery", "Test Search Engine", 10, 10, 0},
      {"http://testsearch.com/", "Test Search Engine", 9, 9, 0},
      {"http://anotherengine.com/?q=thequery", "Another Search Engine", 8, 8,
       0},
      // The encoded stuff between /wiki/ and the # is 第二次世界大戦
      {"http://ja.wikipedia.org/wiki/%E7%AC%AC%E4%BA%8C%E6%AC%A1%E4%B8%96%E7%95"
       "%8C%E5%A4%A7%E6%88%A6#.E3.83.B4.E3.82.A7.E3.83.AB.E3.82.B5.E3.82.A4.E3."
       "83.A6.E4.BD.93.E5.88.B6",
       "Title Unimportant", 2, 2, 0},
      {"https://twitter.com/fungoodtimes", "fungoodtimes", 10, 10, 0},
      {"https://deduping-test.com/high-scoring", "xyz", 20, 20, 0},
      {"https://deduping-test.com/med-scoring", "xyz", 10, 10, 0},
      {"https://suffix.com/prefixsuffix1",
       "'pre suf' should score higher than 'presuf'", 3, 3, 1},
      {"https://suffix.com/prefixsuffix2",
       "'pre suf' should score higher than 'presuf'", 3, 3, 2},
      {"https://suffix.com/prefixsuffix3",
       "'pre suf' should score higher than 'presuf'", 3, 3, 3},
      {"http://somedomain.com/a", "a", 1, 1, 1},
      {"http://somedomain.com/b", "b", 1, 1, 1},
      {"http://somedomain.com/c", "c", 1, 1, 1},
      {"http://somedomain.com/d", "d", 1, 1, 1},
      {"http://somedomain.com/e", "e", 1, 1, 1},
      {"http://somedomain.com/f", "f", 1, 1, 1},
      {"http://somedomain.com/g", "g", 1, 1, 1},
      {"http://somedomain.com/h", "h", 1, 1, 1},
  };
}

void HistoryQuickProviderTest::FillData() {
  for (const auto& info : GetTestData()) {
    history::URLRow row{GURL(info.url)};
    ASSERT_TRUE(row.url().is_valid());
    row.set_title(base::UTF8ToUTF16(info.title));
    row.set_visit_count(info.visit_count);
    row.set_typed_count(info.typed_count);
    row.set_last_visit(base::Time::Now() - base::Days(info.days_from_now));

    AddFakeURLToHistoryDB(history_backend()->db(), row);
  }
}

HistoryQuickProviderTest::SetShouldContain::SetShouldContain(
    const ACMatches& matched_urls) {
  for (const auto& matched_url : matched_urls)
    matches_.insert(matched_url.destination_url.spec());
}

void HistoryQuickProviderTest::SetShouldContain::operator()(
    const std::string& expected) {
  EXPECT_EQ(1U, matches_.erase(expected))
      << "Results did not contain '" << expected << "' but should have.";
}

void HistoryQuickProviderTest::RunTest(
    const std::u16string& text,
    bool prevent_inline_autocomplete,
    const std::vector<std::string>& expected_urls,
    bool expected_can_inline_top_result,
    const std::u16string& expected_fill_into_edit,
    const std::u16string& expected_autocompletion) {
  RunTestWithCursor(text, std::u16string::npos, prevent_inline_autocomplete,
                    expected_urls, expected_can_inline_top_result,
                    expected_fill_into_edit, expected_autocompletion);
}

void HistoryQuickProviderTest::RunTestWithCursor(
    const std::u16string& text,
    const size_t cursor_position,
    bool prevent_inline_autocomplete,
    const std::vector<std::string>& expected_urls,
    bool expected_can_inline_top_result,
    const std::u16string& expected_fill_into_edit,
    const std::u16string& expected_autocompletion,
    bool duplicates_ok) {
  SCOPED_TRACE(text);  // Minimal hint to query being run.
  base::RunLoop().RunUntilIdle();
  AutocompleteInput input(text, cursor_position,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());

  ac_matches_ = provider_->matches();

  // We should have gotten back at most
  // AutocompleteProvider::provider_max_matches().
  EXPECT_LE(ac_matches_.size(), provider_->provider_max_matches());

  // If the number of expected and actual matches aren't equal then we need
  // test no further, but let's do anyway so that we know which URLs failed.
  if (duplicates_ok)
    EXPECT_LE(expected_urls.size(), ac_matches_.size());
  else
    EXPECT_EQ(expected_urls.size(), ac_matches_.size());

  // Verify that all expected URLs were found and that all found URLs
  // were expected.
  std::set<std::string> leftovers;
  if (duplicates_ok) {
    for (auto match : ac_matches_)
      leftovers.insert(match.destination_url.spec());
    for (auto expected : expected_urls)
      EXPECT_EQ(1U, leftovers.count(expected)) << "Expected URL " << expected;
    for (auto expected : expected_urls)
      leftovers.erase(expected);
  } else {
    leftovers = for_each(expected_urls.begin(), expected_urls.end(),
                         SetShouldContain(ac_matches_))
                    .LeftOvers();
  }
  EXPECT_EQ(0U, leftovers.size()) << "There were " << leftovers.size()
                                  << " unexpected results, one of which was: '"
                                  << *(leftovers.begin()) << "'.";

  if (expected_urls.empty())
    return;

  ASSERT_FALSE(ac_matches_.empty());
  // Verify that we got the results in the order expected.
  int best_score = ac_matches_.begin()->relevance + 1;
  int i = 0;
  std::vector<std::string>::const_iterator expected = expected_urls.begin();
  for (ACMatches::const_iterator actual = ac_matches_.begin();
       actual != ac_matches_.end() && expected != expected_urls.end();
       ++actual, ++expected, ++i) {
    EXPECT_EQ(*expected, actual->destination_url.spec())
        << "For result #" << i << " we got '" << actual->destination_url.spec()
        << "' but expected '" << *expected << "'.";
    EXPECT_LT(actual->relevance, best_score)
        << "At result #" << i << " (url=" << actual->destination_url.spec()
        << "), we noticed scores are not monotonically decreasing.";
    best_score = actual->relevance;
  }

  EXPECT_EQ(expected_can_inline_top_result,
            ac_matches_[0].allowed_to_be_default_match);
  if (expected_can_inline_top_result)
    EXPECT_EQ(expected_autocompletion, ac_matches_[0].inline_autocompletion);
  EXPECT_EQ(expected_fill_into_edit, ac_matches_[0].fill_into_edit);
}

bool HistoryQuickProviderTest::GetURLProxy(const GURL& url) {
  base::CancelableTaskTracker task_tracker;
  bool result = false;
  base::RunLoop loop;
  client_->GetHistoryService()->ScheduleDBTask(
      FROM_HERE,
      std::unique_ptr<history::HistoryDBTask>(
          new GetURLTask(url, &result, loop.QuitWhenIdleClosure())),
      &task_tracker);
  // Run the message loop until GetURLTask::DoneRunOnMainThread stops it.  If
  // the test hangs, DoneRunOnMainThread isn't being invoked correctly.
  loop.Run();
  return result;
}

TEST_F(HistoryQuickProviderTest, SimpleSingleMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  RunTest(u"slashdot", false, expected_urls, true,
          u"slashdot.org/favorite_page.html", u".org/favorite_page.html");
}

TEST_F(HistoryQuickProviderTest, SingleMatchWithCursor) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  // With cursor after "slash", we should retrieve the desired result but it
  // should not be allowed to be the default match.
  RunTestWithCursor(u"slashfavorite_page.html", 5, false, expected_urls, false,
                    u"slashdot.org/favorite_page.html", std::u16string());
}

TEST_F(HistoryQuickProviderTest, MatchWithAndWithoutCursorWordBreak) {
  // The input 'twitter.com/fungoo|times' matches only with a cursor word break.
  // We should retrieve the desired result but it should not be allowed to be
  // the default match.
  std::vector<std::string> expected_urls;
  expected_urls.push_back("https://twitter.com/fungoodtimes");
  RunTestWithCursor(u"twitter.com/fungootime", 18, true, expected_urls, false,
                    u"https://twitter.com/fungoodtimes", std::u16string());

  // The input 'twitter.com/fungood|times' matches both with and without a
  // cursor word break. We should retrieve both suggestions but neither should
  // be allowed to be the default match.
  RunTestWithCursor(u"twitter.com/fungoodtime", 19, true, expected_urls, false,
                    u"https://twitter.com/fungoodtimes", std::u16string(),
                    true);

  // A suggestion with a cursor not at the input end can only be default if
  // the input matches suggestion exactly.
  RunTestWithCursor(u"twitter.com/fungoodtimes", 19, true, expected_urls, true,
                    u"https://twitter.com/fungoodtimes", std::u16string(),
                    true);
}

TEST_F(HistoryQuickProviderTest, MatchWithAndWithoutCursorWordBreak_Dedupe) {
  std::vector<std::string> expected_urls;
  // An input can match a suggestion both with and without cursor word break.
  // When doing so exceeds 3 (|max_matches|), they should be deduped and return
  // unique suggestions. Cursor position selected arbitrarily; it doesn't matter
  // as long as it's not at the start or the end.
  expected_urls.push_back("https://deduping-test.com/high-scoring");
  expected_urls.push_back("https://deduping-test.com/med-scoring");
  RunTestWithCursor(u"deduping-test", 1, true, expected_urls, false,
                    u"https://deduping-test.com/high-scoring",
                    std::u16string());
}

TEST_F(HistoryQuickProviderTest,
       MatchWithAndWithoutCursorWordBreak_DedupingKeepsHigherScoredSuggestion) {
  // When the input matches a suggestion both with and without a cursor word
  // break, HQP will generate the suggestion twice. When doing so exceeds
  // 3 (|max_matches|), they should be deduped and the higher scored suggestions
  // should be kept, as oppposed to the first suggestion encountered.
  std::vector<std::string> expected_urls;
  expected_urls.push_back("https://suffix.com/prefixsuffix1");
  expected_urls.push_back("https://suffix.com/prefixsuffix2");
  expected_urls.push_back("https://suffix.com/prefixsuffix3");

  // Get scores for 'prefixsuffix'
  RunTestWithCursor(u"prefixsuffix", std::string::npos, false, expected_urls,
                    false, u"https://suffix.com/prefixsuffix1",
                    std::u16string());
  std::vector<int> unbroken_scores =
      base::ToVector(ac_matches(), &AutocompleteMatch::relevance);
  EXPECT_EQ(unbroken_scores.size(), 3U);

  // Get scores for 'prefix suffix'
  RunTestWithCursor(u"prefix suffix", std::string::npos, false, expected_urls,
                    false, u"https://suffix.com/prefixsuffix1",
                    std::u16string());
  std::vector<int> broken_scores =
      base::ToVector(ac_matches(), &AutocompleteMatch::relevance);
  EXPECT_EQ(broken_scores.size(), 3U);
  // Ensure the latter scores are higher than the former.
  for (size_t i = 0; i < 3; ++i)
    EXPECT_GT(broken_scores[i], unbroken_scores[i]);

  // Get scores for 'prefix|suffix', which will create duplicate
  // ScoredHistoryMatches.
  RunTestWithCursor(u"prefixsuffix", 6, true, expected_urls, false,
                    u"https://suffix.com/prefixsuffix1", std::u16string());
  // Ensure the higher scored ScoredHistoryMatches are promoted to suggestions
  // during deduping.
  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ(ac_matches()[i].relevance, broken_scores[i]);
}

TEST_F(HistoryQuickProviderTest, WordBoundariesWithPunctuationMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://popularsitewithpathonly.com/moo");
  RunTest(u"/moo", false, expected_urls, false,
          u"popularsitewithpathonly.com/moo", std::u16string());
}

TEST_F(HistoryQuickProviderTest, MultiTermTitleMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back(
      "http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice");
  RunTest(u"mice other animals", false, expected_urls, false,
          u"cda.com/Dogs Cats Gorillas Sea Slugs and Mice", std::u16string());
}

TEST_F(HistoryQuickProviderTest, NonWordLastCharacterMatch) {
  std::string expected_url("http://slashdot.org/favorite_page.html");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(u"slashdot.org/", false, expected_urls, true,
          u"slashdot.org/favorite_page.html", u"favorite_page.html");
}

TEST_F(HistoryQuickProviderTest, MultiMatch) {
  std::vector<std::string> expected_urls;
  // Scores high because of typed_count.
  expected_urls.push_back("http://foo.com/");
  // Scores high because of visit count.
  expected_urls.push_back("http://foo.com/dir/another/");
  // Scores high because of high visit count.
  expected_urls.push_back("http://foo.com/dir/another/again/");
  RunTest(u"foo", false, expected_urls, true, u"foo.com", u".com");
}

TEST_F(HistoryQuickProviderTest, StartRelativeMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://xyzabcdefghijklmnopqrstuvw.com/a");
  RunTest(u"xyza", false, expected_urls, true,
          u"xyzabcdefghijklmnopqrstuvw.com/a", u"bcdefghijklmnopqrstuvw.com/a");
}

TEST_F(HistoryQuickProviderTest, EncodingMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://spaces.com/path%20with%20spaces/foo.html");
  RunTest(u"path with spaces", false, expected_urls, false,
          u"spaces.com/path with spaces/foo.html", std::u16string());
}

TEST_F(HistoryQuickProviderTest, ContentsClass) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back(
      "http://ja.wikipedia.org/wiki/%E7%AC%AC%E4%BA%8C%E6%AC%A1%E4%B8%96%E7"
      "%95%8C%E5%A4%A7%E6%88%A6#.E3.83.B4.E3.82.A7.E3.83.AB.E3.82.B5.E3.82."
      "A4.E3.83.A6.E4.BD.93.E5.88.B6");
  RunTest(u"第二 e3", false, expected_urls, false,
          u"ja.wikipedia.org/wiki/第二次世界大戦#.E3.83.B4.E3."
          u"82.A7.E3.83.AB.E3.82.B5.E3.82.A4.E3.83.A6.E4.BD."
          u"93.E5.88.B6",
          std::u16string());
#if DCHECK_IS_ON()
  ac_matches()[0].Validate();
#endif  // DCHECK_IS_ON();
  // Verify that contents_class divides the string in the right places.
  // [22, 24) is the "第二".  All the other pairs are the "e3".
  ACMatchClassifications contents_class(ac_matches()[0].contents_class);
  size_t expected_offsets[] = {0,  22, 24, 31, 33, 40, 42, 49, 51, 58,
                               60, 67, 69, 76, 78, 85, 86, 94, 95};
  // ScoredHistoryMatch may not highlight all the occurrences of these terms
  // because it only highlights terms at word breaks, and it only stores word
  // breaks up to some specified number of characters (50 at the time of this
  // comment).  This test is written flexibly so it still will pass if we
  // increase that number in the future.  Regardless, we require the first
  // five offsets to be correct--in this example these cover at least one
  // occurrence of each term.
  EXPECT_LE(contents_class.size(), std::size(expected_offsets));
  EXPECT_GE(contents_class.size(), 5u);
  for (size_t i = 0; i < contents_class.size(); ++i)
    EXPECT_EQ(expected_offsets[i], contents_class[i].offset);
}

TEST_F(HistoryQuickProviderTest, VisitCountMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://visitedest.com/y/a");
  expected_urls.push_back("http://visitedest.com/y/b");
  expected_urls.push_back("http://visitedest.com/x/c");
  RunTest(u"visitedest", false, expected_urls, true, u"visitedest.com/y/a",
          u".com/y/a");
}

TEST_F(HistoryQuickProviderTest, TypedCountMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://typeredest.com/y/a");
  expected_urls.push_back("http://typeredest.com/y/b");
  expected_urls.push_back("http://typeredest.com/x/c");
  RunTest(u"typeredest", false, expected_urls, true, u"typeredest.com/y/a",
          u".com/y/a");
}

TEST_F(HistoryQuickProviderTest, DaysAgoMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://daysagoest.com/y/a");
  expected_urls.push_back("http://daysagoest.com/y/b");
  expected_urls.push_back("http://daysagoest.com/x/c");
  RunTest(u"daysagoest", false, expected_urls, true, u"daysagoest.com/y/a",
          u".com/y/a");
}

TEST_F(HistoryQuickProviderTest, EncodingLimitMatch) {
  std::vector<std::string> expected_urls;
  std::string url(
      "http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice");
  // First check that a mid-word match yield no results.
  RunTest(u"ice", false, expected_urls, false,
          u"cda.com/Dogs Cats Gorillas Sea Slugs and Mice", std::u16string());
  // Then check that we get results when the match is at a word start
  // that is present because of an encoded separate (%20 = space).
  expected_urls.push_back(url);
  RunTest(u"Mice", false, expected_urls, false,
          u"cda.com/Dogs Cats Gorillas Sea Slugs and Mice", std::u16string());
  // Verify that the matches' ACMatchClassifications offsets are in range.
  ACMatchClassifications content(ac_matches()[0].contents_class);
  // The max offset accounts for 6 occurrences of '%20' plus the 'http://'.
  const size_t max_offset = url.length() - ((6 * 2) + 7);
  for (ACMatchClassifications::const_iterator citer = content.begin();
       citer != content.end(); ++citer)
    EXPECT_LT(citer->offset, max_offset);
  ACMatchClassifications description(ac_matches()[0].description_class);
  std::string page_title("Dogs & Cats & Mice & Other Animals");
  for (ACMatchClassifications::const_iterator diter = description.begin();
       diter != description.end(); ++diter)
    EXPECT_LT(diter->offset, page_title.length());
}

TEST_F(HistoryQuickProviderTest, Spans) {
  // Test SpansFromTermMatch
  TermMatches matches_a;
  // Simulates matches: '.xx.xxx..xx...xxxxx..' which will test no match at
  // either beginning or end as well as adjacent matches.
  matches_a.push_back(TermMatch(1, 1, 2));
  matches_a.push_back(TermMatch(2, 4, 3));
  matches_a.push_back(TermMatch(3, 9, 1));
  matches_a.push_back(TermMatch(3, 10, 1));
  matches_a.push_back(TermMatch(4, 14, 5));
  ACMatchClassifications spans_a =
      HistoryQuickProvider::SpansFromTermMatch(matches_a, 20, false);
  // ACMatch spans should be: 'NM-NM---N-M-N--M----N-'
  ASSERT_EQ(9U, spans_a.size());
  EXPECT_EQ(0U, spans_a[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[0].style);
  EXPECT_EQ(1U, spans_a[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[1].style);
  EXPECT_EQ(3U, spans_a[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[2].style);
  EXPECT_EQ(4U, spans_a[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[3].style);
  EXPECT_EQ(7U, spans_a[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[4].style);
  EXPECT_EQ(9U, spans_a[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[5].style);
  EXPECT_EQ(11U, spans_a[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[6].style);
  EXPECT_EQ(14U, spans_a[7].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[7].style);
  EXPECT_EQ(19U, spans_a[8].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[8].style);
  // Simulates matches: 'xx.xx' which will test matches at both beginning and
  // end.
  TermMatches matches_b;
  matches_b.push_back(TermMatch(1, 0, 2));
  matches_b.push_back(TermMatch(2, 3, 2));
  ACMatchClassifications spans_b =
      HistoryQuickProvider::SpansFromTermMatch(matches_b, 5, true);
  // ACMatch spans should be: 'M-NM-'
  ASSERT_EQ(3U, spans_b.size());
  EXPECT_EQ(0U, spans_b[0].offset);
  EXPECT_EQ(ACMatchClassification::MATCH | ACMatchClassification::URL,
            spans_b[0].style);
  EXPECT_EQ(2U, spans_b[1].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_b[1].style);
  EXPECT_EQ(3U, spans_b[2].offset);
  EXPECT_EQ(ACMatchClassification::MATCH | ACMatchClassification::URL,
            spans_b[2].style);
}

TEST_F(HistoryQuickProviderTest, DeleteMatch) {
  GURL test_url("http://slashdot.org/favorite_page.html");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(test_url.spec());
  // Fill up ac_matches_; we don't really care about the test yet.
  RunTest(u"slashdot", false, expected_urls, true,
          u"slashdot.org/favorite_page.html", u".org/favorite_page.html");
  EXPECT_EQ(1U, ac_matches().size());
  EXPECT_TRUE(GetURLProxy(test_url));
  provider().DeleteMatch(ac_matches()[0]);

  // Check that the underlying URL is deleted from the history DB (this implies
  // that all visits are gone as well). Also verify that a deletion notification
  // is sent, in response to which the secondary data stores (InMemoryDatabase,
  // InMemoryURLIndex) will drop any data they might have pertaining to the URL.
  // To ensure that the deletion has been propagated everywhere before we start
  // verifying post-deletion states, first wait until we see the notification.
  WaitForURLsDeletedNotification(client().GetHistoryService());
  EXPECT_FALSE(GetURLProxy(test_url));

  // Just to be on the safe side, explicitly verify that we have deleted enough
  // data so that we will not be serving the same result again.
  expected_urls.clear();
  RunTest(u"slashdot", false, expected_urls, true, u"NONE EXPECTED",
          std::u16string());
}

TEST_F(HistoryQuickProviderTest, PreventBeatingURLWhatYouTypedMatch) {
  std::vector<std::string> expected_urls;

  expected_urls.clear();
  expected_urls.push_back("http://popularsitewithroot.com/");
  // If the user enters a hostname (no path) that they have visited
  // before, we should make sure that all HistoryQuickProvider results
  // have scores less than what HistoryURLProvider will assign the
  // URL-what-you-typed match.
  RunTest(u"popularsitewithroot.com", false, expected_urls, true,
          u"popularsitewithroot.com", std::u16string());
  EXPECT_LT(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForBestInlineableResult);

  // Check that if the user didn't quite enter the full hostname, this
  // hostname would've normally scored above the URL-what-you-typed match.
  RunTest(u"popularsitewithroot.c", false, expected_urls, true,
          u"popularsitewithroot.com", u"om");
  EXPECT_GE(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  expected_urls.clear();
  expected_urls.push_back("http://popularsitewithpathonly.com/moo");
  // If the user enters a hostname of a host that they have visited
  // but never visited the root page of, we should make sure that all
  // HistoryQuickProvider results have scores less than what the
  // HistoryURLProvider will assign the URL-what-you-typed match.
  RunTest(u"popularsitewithpathonly.com", false, expected_urls, true,
          u"popularsitewithpathonly.com/moo", u"/moo");
  EXPECT_LT(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForUnvisitedIntranetResult);

  // Verify the same thing happens if the user adds a / to end of the
  // hostname.
  RunTest(u"popularsitewithpathonly.com/", false, expected_urls, true,
          u"popularsitewithpathonly.com/moo", u"moo");
  EXPECT_LT(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForUnvisitedIntranetResult);

  // Check that if the user didn't quite enter the full hostname, this
  // page would've normally scored above the URL-what-you-typed match.
  RunTest(u"popularsitewithpathonly.co", false, expected_urls, true,
          u"popularsitewithpathonly.com/moo", u"m/moo");
  EXPECT_GE(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // If the user enters a hostname + path that they have not visited
  // before (but visited other things on the host), we can allow
  // inline autocompletions.
  RunTest(u"popularsitewithpathonly.com/mo", false, expected_urls, true,
          u"popularsitewithpathonly.com/moo", u"o");
  EXPECT_GE(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // If the user enters a hostname + path that they have visited
  // before, we should make sure that all HistoryQuickProvider results
  // have scores less than what the HistoryURLProvider will assign
  // the URL-what-you-typed match.
  RunTest(u"popularsitewithpathonly.com/moo", false, expected_urls, true,
          u"popularsitewithpathonly.com/moo", std::u16string());
  EXPECT_LT(ac_matches()[0].relevance,
            HistoryURLProvider::kScoreForBestInlineableResult);
}

TEST_F(HistoryQuickProviderTest, PreventInlineAutocomplete) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://popularsitewithroot.com/");

  // Check that the desired URL is normally allowed to be the default match
  // against input that is a prefex of the URL.
  RunTest(u"popularsitewithr", false, expected_urls, true,
          u"popularsitewithroot.com", u"oot.com");

  // Check that it's not allowed to be the default match if
  // prevent_inline_autocomplete is true.
  RunTest(u"popularsitewithr", true, expected_urls, false,
          u"popularsitewithroot.com", u"oot.com");

  // But the exact hostname can still match even if prevent inline autocomplete
  // is true.  i.e., there's no autocompletion necessary; this is effectively
  // URL-what-you-typed.
  RunTest(u"popularsitewithroot.com", true, expected_urls, true,
          u"popularsitewithroot.com", std::u16string());

  // The above still holds even with an extra trailing slash.
  RunTest(u"popularsitewithroot.com/", true, expected_urls, true,
          u"popularsitewithroot.com", std::u16string());
}

TEST_F(HistoryQuickProviderTest, DoesNotProvideMatchesOnFocus) {
  AutocompleteInput input(u"popularsite", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider().Start(input, false);
  EXPECT_TRUE(provider().matches().empty());
}

ScoredHistoryMatch BuildScoredHistoryMatch(const std::string& url_text,
                                           const std::u16string& input_term) {
  return ScoredHistoryMatch(history::URLRow(GURL(url_text)), VisitInfoVector(),
                            input_term, String16Vector(1, input_term),
                            WordStarts(1, 0), RowWordStarts(), false, 0, false,
                            base::Time());
}

// Trim the http:// scheme from the contents in the general case.
TEST_F(HistoryQuickProviderTest, DoTrimHttpScheme) {
  AutocompleteInput input(u"face", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  ScoredHistoryMatch history_match =
      BuildScoredHistoryMatch("http://www.facebook.com", u"face");

  AutocompleteMatch match = QuickMatchToACMatch(history_match, 100);
  EXPECT_EQ(u"facebook.com", match.contents);
}

// Don't trim the http:// scheme from the match contents if
// the user input included a scheme.
TEST_F(HistoryQuickProviderTest, DontTrimHttpSchemeIfInputHasScheme) {
  AutocompleteInput input(u"http://face", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  ScoredHistoryMatch history_match =
      BuildScoredHistoryMatch("http://www.facebook.com", u"http://face");

  AutocompleteMatch match = QuickMatchToACMatch(history_match, 100);
  EXPECT_EQ(u"http://facebook.com", match.contents);
}

// Don't trim the http:// scheme from the match contents if
// the user input matched it.
TEST_F(HistoryQuickProviderTest, DontTrimHttpSchemeIfInputMatches) {
  AutocompleteInput input(u"ht", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  ScoredHistoryMatch history_match =
      BuildScoredHistoryMatch("http://www.facebook.com", u"ht");
  history_match.match_in_scheme = true;

  AutocompleteMatch match = QuickMatchToACMatch(history_match, 100);
  EXPECT_EQ(u"http://facebook.com", match.contents);
}

// Don't trim the https:// scheme from the match contents if the user input
// included a scheme.
TEST_F(HistoryQuickProviderTest, DontTrimHttpsSchemeIfInputHasScheme) {
  AutocompleteInput input(u"https://face", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  ScoredHistoryMatch history_match =
      BuildScoredHistoryMatch("https://www.facebook.com", u"https://face");

  AutocompleteMatch match = QuickMatchToACMatch(history_match, 100);
  EXPECT_EQ(u"https://facebook.com", match.contents);
}

// Trim the https:// scheme from the match contents if nothing prevents it.
TEST_F(HistoryQuickProviderTest, DoTrimHttpsScheme) {
  AutocompleteInput input(u"face", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  ScoredHistoryMatch history_match =
      BuildScoredHistoryMatch("https://www.facebook.com", u"face");

  AutocompleteMatch match = QuickMatchToACMatch(history_match, 100);
  EXPECT_EQ(u"facebook.com", match.contents);
}

TEST_F(HistoryQuickProviderTest, CorrectAutocompleteWithTrailingSlash) {
  AutocompleteInput input(u"cr/", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  RowWordStarts word_starts;
  word_starts.url_word_starts_ = {0};
  ScoredHistoryMatch sh_match(history::URLRow(GURL("http://cr/")),
                              VisitInfoVector(), u"cr/", {u"cr"}, {0},
                              word_starts, false, 0, false, base::Time());
  AutocompleteMatch ac_match(QuickMatchToACMatch(sh_match, 0));
  EXPECT_EQ(u"cr/", ac_match.fill_into_edit);
  EXPECT_EQ(u"", ac_match.inline_autocompletion);
  EXPECT_TRUE(ac_match.allowed_to_be_default_match);
}

TEST_F(HistoryQuickProviderTest, KeywordModeExtractUserInput) {
  // Populate template URL with starter pack entries
  std::vector<std::unique_ptr<TemplateURLData>> turls =
      TemplateURLStarterPackData::GetStarterPackEngines();
  for (auto& turl : turls) {
    client().GetTemplateURLService()->Add(
        std::make_unique<TemplateURL>(std::move(*turl)));
  }
  // Test result for user text "google", we should get back a result for google.
  AutocompleteInput input(u"google", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  if (!provider().done())
    base::RunLoop().Run();

  ACMatches matches = provider().matches();
  ASSERT_GT(matches.size(), 0u);
  EXPECT_EQ(GURL("http://www.google.com/"), matches[0].destination_url);

  // Test result for "@history google" while NOT in keyword mode, we should not
  // get a result for google since we're searching for the whole input text
  // including "@history".
  AutocompleteInput input2(u"@history google",
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  provider().Start(input2, false);
  if (!provider().done())
    base::RunLoop().Run();

  matches = provider().matches();
  ASSERT_EQ(matches.size(), 0u);

  // Turn on keyword mode, test result again, we should get back the result for
  // google.com since we're searching only for the user text after the keyword.
  input2.set_prefer_keyword(true);
  input2.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  provider().Start(input2, false);
  if (!provider().done())
    base::RunLoop().Run();

  matches = provider().matches();
  ASSERT_GT(matches.size(), 0u);
  EXPECT_EQ(GURL("http://www.google.com/"), matches[0].destination_url);
  EXPECT_TRUE(matches[0].from_keyword);

  // Ensure keyword and transition are set properly to keep user in keyword
  // mode.
  EXPECT_EQ(matches[0].keyword, u"@history");
  EXPECT_TRUE(PageTransitionCoreTypeIs(matches[0].transition,
                                       ui::PAGE_TRANSITION_KEYWORD));
}

TEST_F(HistoryQuickProviderTest,
       QuickMatchToACMatch_HideUrlForDocumentSuggestion) {
  AutocompleteInput input(u"face", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);
  ScoredHistoryMatch history_match = BuildScoredHistoryMatch(
      "https://docs.google.com/a/google.com/document/d/tH3_d0C-1d/edit",
      u"doc");

  AutocompleteMatch match = QuickMatchToACMatch(history_match, 100);
  EXPECT_TRUE(match.contents.empty());
}

TEST_F(HistoryQuickProviderTest, MaxMatches) {
  // Keyword mode is off. We should only get provider_max_matches_ matches.
  AutocompleteInput input(u"somedomain.com", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  provider().Start(input, false);

  ACMatches matches = provider().matches();
  EXPECT_EQ(matches.size(), provider().provider_max_matches());

  // Turn keyword mode on. we should be able to get more matches now.
  input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB);
  input.set_prefer_keyword(true);
  provider().Start(input, false);

  matches = provider().matches();
  EXPECT_EQ(matches.size(), provider().provider_max_matches_in_keyword_mode());

  // The provider should not limit the number of suggestions when ML scoring
  // w/increased candidates is enabled. Any matches beyond the limit should be
  // marked as culled_by_provider and have a relevance of 0.
  input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID);
  input.set_prefer_keyword(false);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{omnibox::kUrlScoringModel, {}},
       {omnibox::kMlUrlScoring,
        {{"MlUrlScoringUnlimitedNumCandidates", "true"}}}},
      /*disabled_features=*/{});
  OmniboxFieldTrial::ScopedMLConfigForTesting scoped_ml_config;

  provider().Start(input, false);
  matches = provider().matches();
  EXPECT_EQ(matches.size(), 8u);
  // Matches below the `max_matches` limit.
  for (size_t i = 0; i < provider().provider_max_matches(); i++) {
    EXPECT_FALSE(matches[i].culled_by_provider);
    EXPECT_GT(matches[i].relevance, 0);
  }
  // "Extra" matches above the `max_matches` limit. Should have 0 relevance and
  // be marked as `culled_by_provider`.
  for (size_t i = provider().provider_max_matches(); i < matches.size(); i++) {
    EXPECT_TRUE(matches[i].culled_by_provider);
    EXPECT_EQ(matches[i].relevance, 0);
  }

  // Unlimited matches should ignore the provider max matches, even if the
  // `kMlUrlScoringMaxMatchesByProvider` param is set.
  scoped_ml_config.GetMLConfig().ml_url_scoring_max_matches_by_provider = "*:6";

  provider().Start(input, false);
  matches = provider().matches();
  EXPECT_EQ(matches.size(), 8u);
}

class HQPDomainSuggestionsTest : public HistoryQuickProviderTest {
 protected:
  std::vector<TestURLInfo> GetTestData() override {
    return {
        // Popular domain spread across 4 2-typed and 3 1-typed visits.
        {"http://www.Dilijan.com/1", "Dilijan 1", 100, 2, 101},
        {"http://www.Dilijan.com/2", "Dilijan 2", 100, 2, 102},
        {"http://www.Dilijan.com/3", "Dilijan 3", 100, 2, 103},
        {"http://www.Dilijan.com/4", "Dilijan 4", 100, 2, 104},
        {"http://www.Dilijan.com/5", "Dilijan 5", 100, 1, 105},
        {"http://www.Dilijan.com/6", "Dilijan 6", 100, 1, 106},
        {"http://www.Dilijan.com/7", "Dilijan 7", 100, 1, 107},
        // Popular domain spread across 2 3-typed and 5 1-typed visits.
        {"http://www.Geghard.com/1", "Geghard 1", 100, 3, 101},
        {"http://www.Geghard.com/2", "Geghard 2", 100, 3, 102},
        {"http://www.Geghard.com/3", "Geghard 3", 100, 1, 103},
        {"http://www.Geghard.com/4", "Geghard 4", 100, 1, 104},
        {"http://www.Geghard.com/5", "Geghard 5", 100, 1, 105},
        {"http://www.Geghard.com/6", "Geghard 6", 100, 1, 106},
        {"http://www.Geghard.com/7", "Geghard 7", 100, 1, 107},
        // Unpopular domain despite a 100-typed and 6 1-typed visits. Popular
        // domains require at least 4 capped & offset typed visits, where each
        // URL's typed visits are offset by 1 and capped at 2.
        {"http://www.Tatev.com/1", "Tatev 1", 100, 100, 101},
        {"http://www.Tatev.com/2", "Tatev 2", 100, 1, 102},
        {"http://www.Tatev.com/3", "Tatev 3", 100, 1, 103},
        {"http://www.Tatev.com/4", "Tatev 4", 100, 1, 104},
        {"http://www.Tatev.com/5", "Tatev 5", 100, 1, 105},
        {"http://www.Tatev.com/6", "Tatev 6", 100, 1, 106},
        {"http://www.Tatev.com/7", "Tatev 7", 100, 1, 107},
        // Unpopular domain despite a 6 100-typed and 1 0-typed visits. Popular
        // domains require at least 7 1-typed visits.
        {"http://www.Gyumri.com/1", "Gyumri 1", 100, 100, 101},
        {"http://www.Gyumri.com/2", "Gyumri 2", 100, 100, 102},
        {"http://www.Gyumri.com/3", "Gyumri 3", 100, 100, 103},
        {"http://www.Gyumri.com/4", "Gyumri 4", 100, 100, 104},
        {"http://www.Gyumri.com/5", "Gyumri 5", 100, 100, 105},
        {"http://www.Gyumri.com/6", "Gyumri 6", 100, 100, 106},
        {"http://www.Gyumri.com/7", "Gyumri 7", 100, 0, 107},
    };
  }
};

TEST_F(HQPDomainSuggestionsTest, DomainSuggestions) {
  const auto test = [&](const std::u16string& input_text, bool input_keyword,
                        std::vector<std::u16string> expected_matches,
                        bool expected_triggered) {
    SCOPED_TRACE("input_text: " + base::UTF16ToUTF8(input_text) +
                 ", input_keyword: " + (input_keyword ? "true" : "false"));

    AutocompleteInput input(input_text, metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_keyword_mode_entry_method(
        input_keyword
            ? metrics::OmniboxEventProto_KeywordModeEntryMethod_TAB
            : metrics::OmniboxEventProto_KeywordModeEntryMethod_INVALID);
    input.set_prefer_keyword(input_keyword);

    client().GetOmniboxTriggeredFeatureService()->ResetSession();
    provider().Start(input, false);
    auto matches = provider().matches();
    std::vector<std::u16string> match_titles;
    base::ranges::transform(
        matches, std::back_inserter(match_titles),
        [](const auto& match) { return match.description; });
    EXPECT_THAT(match_titles, testing::ElementsAreArray(expected_matches));

    EXPECT_EQ(client()
                  .GetOmniboxTriggeredFeatureService()
                  ->GetFeatureTriggeredInSession(
                      metrics::OmniboxEventProto_Feature_DOMAIN_SUGGESTIONS),
              expected_triggered);
  };

  // When matching a popular domain, its top 3 suggestions should be suggested
  // twice: 1st from the overall pass, 2nd from domain pass. They should each be
  // limited to individually, 3 matches for the 1st, 2 matches for the latter.
  // Duplicates aren't necessary behavior, just a harmless side effect. The
  // domain algorithm may change in the future to not add duplicates.
  test(u"Dilijan", false,
       {u"Dilijan 1", u"Dilijan 2", u"Dilijan 3", u"Dilijan 1", u"Dilijan 2"},
       true);

  // Like above, but when only some of its suggestions match, only those should
  // be suggested by both the overall and domain passes.
  test(u"Dilijan 1", false, {u"Dilijan 1", u"Dilijan 1"}, true);

  // Domains with more than 4 typed visits should be considered popular.
  test(u"Geghard", false,
       {u"Geghard 1", u"Geghard 2", u"Geghard 3", u"Geghard 1", u"Geghard 2"},
       true);

  // Domains with more than 4 typed visits but less than 4 capped typed visits
  // should not be considered popular.
  test(u"Tatev", false, {u"Tatev 1", u"Tatev 2", u"Tatev 3"}, false);

  // Domains with more than 7 visits, but less than 7 1-typed visits should not
  // be considered popular.
  test(u"Gyumri", false, {u"Gyumri 1", u"Gyumri 2", u"Gyumri 3"}, false);

  // When matching multiple domains, the overall pass should suggest the top
  // suggestion, even if some of them aren't from a popular domain, then each
  // domain's suggestions should be appended, each individually limited to 2.
  test(u"www.", false,
       {u"Gyumri 1", u"Tatev 1", u"Gyumri 2", u"Geghard 1", u"Geghard 2",
        u"Dilijan 1", u"Dilijan 2"},
       true);

  // Short inputs should not have domain suggestions. They should still log the
  // feature as triggered since their scores may potentially be boosted.
  test(u"Dil", false, {u"Dilijan 1", u"Dilijan 2", u"Dilijan 3"}, false);

  // Keyword inputs should not have domain suggestions, so we shouldn't see
  // duplicates. But keyword inputs have a higher provider limit, so we should
  // see all 7 matching suggestions.
  test(u"Dilijan", true,
       {u"Dilijan 1", u"Dilijan 2", u"Dilijan 3", u"Dilijan 4", u"Dilijan 5",
        u"Dilijan 6", u"Dilijan 7"},
       false);
}

// HQPOrderingTest -------------------------------------------------------------

class HQPOrderingTest : public HistoryQuickProviderTest {
 public:
  HQPOrderingTest() = default;
  HQPOrderingTest(const HQPOrderingTest&) = delete;
  HQPOrderingTest& operator=(const HQPOrderingTest&) = delete;

 protected:
  std::vector<TestURLInfo> GetTestData() override;
};

std::vector<HistoryQuickProviderTest::TestURLInfo>
HQPOrderingTest::GetTestData() {
  return {
      {"http://www.teamliquid.net/tlpd/korean/games/21648_bisu_vs_iris", "", 6,
       3, 256},
      {"http://www.amazon.com/",
       "amazon.com: online shopping for electronics, apparel, computers, "
       "books, dvds & more",
       20, 20, 10},
      {"http://www.teamliquid.net/forum/viewmessage.php?topic_id=52045&"
       "currentpage=83",
       "google images", 6, 6, 0},
      {"http://www.tempurpedic.com/", "tempur-pedic", 7, 7, 0},
      {"http://www.teamfortress.com/", "", 5, 5, 6},
      {"http://www.rottentomatoes.com/", "", 3, 3, 7},
      {"http://music.google.com/music/listen?u=0#start_pl", "", 3, 3, 9},
      {"https://www.emigrantdirect.com/",
       "high interest savings account, high yield savings - emigrantdirect", 5,
       5, 3},
      {"http://store.steampowered.com/", "", 6, 6, 1},
      {"http://techmeme.com/", "techmeme", 111, 110, 4},
      {"http://www.teamliquid.net/tlpd", "team liquid progaming database", 15,
       15, 4},
      {"http://store.steampowered.com/", "the steam summer camp sale", 6, 6, 1},
      {"http://www.teamliquid.net/tlpd/korean/players",
       "tlpd - bw korean - player index", 25, 7, 219},
      {"http://slashdot.org/", "slashdot: news for nerds, stuff that matters",
       3, 3, 6},
      {"http://translate.google.com/", "google translate", 3, 3, 0},
      {"http://arstechnica.com/", "ars technica", 3, 3, 3},
      {"http://www.rottentomatoes.com/",
       "movies | movie trailers | reviews - rotten tomatoes", 3, 3, 7},
      {"http://www.teamliquid.net/",
       "team liquid - starcraft 2 and brood war pro gaming news", 26, 25, 3},
      {"http://metaleater.com/", "metaleater", 4, 3, 8},
      {"http://half.com/",
       "half.com: textbooks , books , music , movies , games , video games", 4,
       4, 6},
      {"http://teamliquid.net/",
       "team liquid - starcraft 2 and brood war pro gaming news", 8, 5, 9},
  };
}

TEST_F(HQPOrderingTest, TEMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.teamliquid.net/");
  expected_urls.push_back("http://techmeme.com/");
  expected_urls.push_back("http://www.teamliquid.net/tlpd");
  RunTest(u"te", false, expected_urls, true, u"www.teamliquid.net",
          u"amliquid.net");
}

TEST_F(HQPOrderingTest, TEAMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.teamliquid.net/");
  expected_urls.push_back("http://www.teamliquid.net/tlpd");
  expected_urls.push_back("http://teamliquid.net/");
  RunTest(u"tea", false, expected_urls, true, u"www.teamliquid.net",
          u"mliquid.net");
}
