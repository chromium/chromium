// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/in_memory_url_index.h"

#include <stddef.h>
#include <stdint.h>

#include <fstream>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gtest/include/gtest/gtest.h"

// The test version of the history url database table ('url') is contained in
// a database file created from a text file('in_memory_url_index_test.db.txt').
// The only difference between this table and a live 'urls' table from a
// profile is that the last_visit_time column in the test table contains a
// number specifying the number of days relative to 'today' to which the
// absolute time should be set during the test setup stage.
//
// The format of the test database text file is of a SQLite .dump file.
// Note that only lines whose first character is an upper-case letter are
// processed when creating the test database.

namespace {
const size_t kInvalid = std::u16string::npos;
const size_t kProviderMaxMatches = 3;
const char kClientAllowlistedScheme[] = "xyz";

// TemplateURLs used to test filtering of search engine URLs.
const char16_t kDefaultTemplateURLKeyword[] = u"default-engine.com";
const char16_t kNonDefaultTemplateURLKeyword[] = u"non-default-engine.com";
const TemplateURLService::Initializer kTemplateURLData[] = {
    {"default-engine.com", "http://default-engine.com?q={searchTerms}",
     "Default"},
    {"non-default-engine.com", "http://non-default-engine.com?q={searchTerms}",
     "Not Default"},
};

// Helper function to set lower case |lower_string| and |lower_terms| (words
// list) based on supplied |search_string| and |cursor_position|. If
// |cursor_position| is set and useful (not at either end of the string), allow
// the |search_string| to be broken at |cursor_position|. We do this by
// pretending there's a space where the cursor is. |lower_terms| are obtained by
// splitting the |lower_string| on whitespace into tokens.
void StringToTerms(const char* search_string,
                   size_t cursor_position,
                   std::u16string* lower_string,
                   String16Vector* lower_terms) {
  *lower_string = base::i18n::ToLower(base::ASCIIToUTF16(search_string));
  if ((cursor_position != kInvalid) &&
      (cursor_position < lower_string->length()) && (cursor_position > 0)) {
    lower_string->insert(cursor_position, u" ");
  }

  *lower_terms =
      base::SplitString(*lower_string, base::kWhitespaceUTF16,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

// -----------------------------------------------------------------------------

class InMemoryURLIndexTest : public testing::Test {
 public:
  InMemoryURLIndexTest() = default;

 protected:
  // Test setup.
  void SetUp() override;
  void TearDown() override;

  // Allows the database containing the test data to be customized by
  // subclasses.
  virtual base::FilePath::StringType TestDBName() const;

  // Allows the test to control when the InMemoryURLIndex is initialized.
  virtual bool InitializeInMemoryURLIndexInSetUp() const;

  // Initialize the InMemoryURLIndex for the tests.
  void InitializeInMemoryURLIndex();

  // Validates that the given |term| is contained in |cache| and that it is
  // marked as in-use.
  void CheckTerm(const URLIndexPrivateData::SearchTermCacheMap& cache,
                 std::u16string term) const;

  // Pass-through function to simplify our friendship with HistoryService.
  sql::Database& GetDB();

  // Pass-through functions to simplify our friendship with InMemoryURLIndex.
  URLIndexPrivateData* GetPrivateData() const;
  base::CancelableTaskTracker* GetPrivateDataTracker() const;
  void ClearPrivateData();
  const SchemeSet& scheme_allowlist();

  // Pass-through functions to simplify our friendship with URLIndexPrivateData.
  bool UpdateURL(const history::URLRow& row);
  bool DeleteURL(const GURL& url);

  // Data verification helper functions.
  void ExpectPrivateDataNotEmpty(const URLIndexPrivateData& data);
  void ExpectPrivateDataEmpty(const URLIndexPrivateData& data);
  void ExpectPrivateDataEqual(const URLIndexPrivateData& expected,
                              const URLIndexPrivateData& actual);

  TemplateURLService* template_url_service();

  ScoredHistoryMatches HistoryItemsForTerms(const std::u16string& term_string,
                                            size_t cursor_position,
                                            const std::string& host_filter,
                                            size_t max_matches) {
    OmniboxTriggeredFeatureService x{};
    return url_index_->HistoryItemsForTerms(term_string, cursor_position,
                                            host_filter, max_matches, &x);
  }

  base::ScopedTempDir history_dir_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<history::HistoryService> history_service_;
  raw_ptr<history::HistoryDatabase> history_database_ = nullptr;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_{
      {.template_url_service_initializer = kTemplateURLData}};
  std::unique_ptr<InMemoryURLIndex> url_index_;
};

TemplateURLService* InMemoryURLIndexTest::template_url_service() {
  return search_engines_test_environment_.template_url_service();
}

sql::Database& InMemoryURLIndexTest::GetDB() {
  return history_database_->GetDB();
}

URLIndexPrivateData* InMemoryURLIndexTest::GetPrivateData() const {
  DCHECK(url_index_->private_data());
  return url_index_->private_data();
}

base::CancelableTaskTracker* InMemoryURLIndexTest::GetPrivateDataTracker()
    const {
  DCHECK(url_index_->private_data_tracker());
  return url_index_->private_data_tracker();
}

void InMemoryURLIndexTest::ClearPrivateData() {
  return url_index_->ClearPrivateData();
}

const SchemeSet& InMemoryURLIndexTest::scheme_allowlist() {
  return url_index_->scheme_allowlist();
}

bool InMemoryURLIndexTest::UpdateURL(const history::URLRow& row) {
  return GetPrivateData()->UpdateURL(history_service_.get(), row,
                                     url_index_->scheme_allowlist_,
                                     GetPrivateDataTracker());
}

bool InMemoryURLIndexTest::DeleteURL(const GURL& url) {
  return GetPrivateData()->DeleteURL(url);
}

void InMemoryURLIndexTest::SetUp() {
  // We cannot access the database until the backend has been loaded.
  ASSERT_TRUE(history_dir_.CreateUniqueTempDir());
  history_service_ =
      history::CreateHistoryService(history_dir_.GetPath(), true);
  ASSERT_TRUE(history_service_);

  history::HistoryBackend* backend = history_service_->history_backend_.get();
  history_database_ = backend->db();

  // Mutating the History database should only happen on the backend sequence.
  history_service_->ScheduleTask(
      history::HistoryService::PRIORITY_NORMAL, base::BindLambdaForTesting([&] {
        // TODO(shess): If/when this code gets refactored, consider including
        // the schema in the golden file (as sqlite3 .dump would generate) and
        // using sql::test::CreateDatabaseFromSQL() to load it.  The code above
        // which creates the database can change in ways which may not reliably
        // represent user databases on disks in the fleet.

        // Execute the contents of a golden file to populate the [urls] and
        // [visits] tables.
        base::FilePath golden_path;
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &golden_path);
        golden_path = golden_path.AppendASCII("components/test/data/omnibox");
        golden_path = golden_path.Append(TestDBName());
        ASSERT_TRUE(base::PathExists(golden_path));
        std::string sql;
        ASSERT_TRUE(base::ReadFileToString(golden_path, &sql));
        sql::Database& db(GetDB());
        ASSERT_TRUE(db.is_open());
        ASSERT_TRUE(db.Execute(sql));

        // Update [urls.last_visit_time] and [visits.visit_time] to represent a
        // time relative to 'now'.
        base::Time time_right_now = base::Time::NowFromSystemTime();
        base::TimeDelta day_delta = base::Days(1);
        {
          sql::Statement s(db.GetUniqueStatement(
              "UPDATE urls SET last_visit_time = ? - ? * last_visit_time"));
          s.BindTime(0, time_right_now);
          s.BindTimeDelta(1, day_delta);
          ASSERT_TRUE(s.Run());
        }
        {
          sql::Statement s(db.GetUniqueStatement(
              "UPDATE visits SET visit_time = ? - ? * visit_time"));
          s.BindTime(0, time_right_now);
          s.BindTimeDelta(1, day_delta);
          ASSERT_TRUE(s.Run());
        }
      }));
  // Block for the above lambda to run, otherwise the captured variables will
  // go out of scope. And moreover, subsequent parts of this function need to
  // have the test data already pre-populated.
  BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  // Set up a simple template URL service with a default search engine.
  TemplateURL* template_url = template_url_service()->GetTemplateURLForKeyword(
      kDefaultTemplateURLKeyword);
  template_url_service()->SetUserSelectedDefaultSearchProvider(template_url);

  if (InitializeInMemoryURLIndexInSetUp())
    InitializeInMemoryURLIndex();
}

void InMemoryURLIndexTest::TearDown() {
  // Ensure that the InMemoryURLIndex no longer observes HistoryService before
  // it is destroyed in order to prevent HistoryService calling dead observer.
  if (url_index_)
    url_index_->Shutdown();
  task_environment_.RunUntilIdle();
}

base::FilePath::StringType InMemoryURLIndexTest::TestDBName() const {
  return FILE_PATH_LITERAL("in_memory_url_index_test.sql");
}

bool InMemoryURLIndexTest::InitializeInMemoryURLIndexInSetUp() const {
  return true;
}

void InMemoryURLIndexTest::InitializeInMemoryURLIndex() {
  DCHECK(!url_index_);

  SchemeSet client_schemes_to_allowlist;
  client_schemes_to_allowlist.insert(kClientAllowlistedScheme);
  url_index_ = std::make_unique<InMemoryURLIndex>(
      nullptr, history_service_.get(), template_url_service(), base::FilePath(),
      client_schemes_to_allowlist);
  url_index_->Init();

  BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  ASSERT_TRUE(url_index_->restored());
}

void InMemoryURLIndexTest::CheckTerm(
    const URLIndexPrivateData::SearchTermCacheMap& cache,
    std::u16string term) const {
  auto cache_iter(cache.find(term));
  ASSERT_TRUE(cache.end() != cache_iter)
      << "Cache does not contain '" << term << "' but should.";
  URLIndexPrivateData::SearchTermCacheItem cache_item = cache_iter->second;
  EXPECT_TRUE(cache_item.used_)
      << "Cache item '" << term << "' should be marked as being in use.";
}

void InMemoryURLIndexTest::ExpectPrivateDataNotEmpty(
    const URLIndexPrivateData& data) {
  EXPECT_FALSE(data.word_list_.empty());
  // available_words_ will be empty since we have freshly built the
  // data set for these tests.
  EXPECT_TRUE(data.available_words_.empty());
  EXPECT_FALSE(data.word_map_.empty());
  EXPECT_FALSE(data.char_word_map_.empty());
  EXPECT_FALSE(data.word_id_history_map_.empty());
  EXPECT_FALSE(data.history_id_word_map_.empty());
  EXPECT_FALSE(data.history_info_map_.empty());
}

void InMemoryURLIndexTest::ExpectPrivateDataEmpty(
    const URLIndexPrivateData& data) {
  EXPECT_TRUE(data.word_list_.empty());
  EXPECT_TRUE(data.available_words_.empty());
  EXPECT_TRUE(data.word_map_.empty());
  EXPECT_TRUE(data.char_word_map_.empty());
  EXPECT_TRUE(data.word_id_history_map_.empty());
  EXPECT_TRUE(data.history_id_word_map_.empty());
  EXPECT_TRUE(data.history_info_map_.empty());
}

// Helper function which compares two maps for equivalence. The maps' values
// are associative containers and their contents are compared as well.
template <typename T>
void ExpectMapOfContainersIdentical(const T& expected, const T& actual) {
  ASSERT_EQ(expected.size(), actual.size());
  for (auto expected_iter = expected.begin(); expected_iter != expected.end();
       ++expected_iter) {
    auto actual_iter = actual.find(expected_iter->first);
    ASSERT_TRUE(actual.end() != actual_iter);
    typename T::mapped_type const& expected_values(expected_iter->second);
    typename T::mapped_type const& actual_values(actual_iter->second);
    ASSERT_EQ(expected_values.size(), actual_values.size());
    for (auto set_iter = expected_values.begin();
         set_iter != expected_values.end(); ++set_iter)
      EXPECT_EQ(actual_values.count(*set_iter),
                expected_values.count(*set_iter));
  }
}

void InMemoryURLIndexTest::ExpectPrivateDataEqual(
    const URLIndexPrivateData& expected,
    const URLIndexPrivateData& actual) {
  EXPECT_EQ(expected.word_list_.size(), actual.word_list_.size());
  EXPECT_EQ(expected.word_map_.size(), actual.word_map_.size());
  EXPECT_EQ(expected.char_word_map_.size(), actual.char_word_map_.size());
  EXPECT_EQ(expected.word_id_history_map_.size(),
            actual.word_id_history_map_.size());
  EXPECT_EQ(expected.history_id_word_map_.size(),
            actual.history_id_word_map_.size());
  EXPECT_EQ(expected.history_info_map_.size(), actual.history_info_map_.size());
  EXPECT_EQ(expected.word_starts_map_.size(), actual.word_starts_map_.size());
  // WordList must be index-by-index equal.
  size_t count = expected.word_list_.size();
  for (size_t i = 0; i < count; ++i)
    EXPECT_EQ(expected.word_list_[i], actual.word_list_[i]);

  ExpectMapOfContainersIdentical(expected.char_word_map_,
                                 actual.char_word_map_);
  ExpectMapOfContainersIdentical(expected.word_id_history_map_,
                                 actual.word_id_history_map_);
  ExpectMapOfContainersIdentical(expected.history_id_word_map_,
                                 actual.history_id_word_map_);

  for (const auto& expected_info : expected.history_info_map_) {
    auto actual_info = actual.history_info_map_.find(expected_info.first);
    // NOTE(yfriedman): ASSERT_NE can't be used due to incompatibility between
    // gtest and STLPort in the Android build. See
    // http://code.google.com/p/googletest/issues/detail?id=359
    ASSERT_TRUE(actual_info != actual.history_info_map_.end());
    const history::URLRow& expected_row(expected_info.second.url_row);
    const history::URLRow& actual_row(actual_info->second.url_row);
    EXPECT_EQ(expected_row.visit_count(), actual_row.visit_count());
    EXPECT_EQ(expected_row.typed_count(), actual_row.typed_count());
    EXPECT_EQ(expected_row.last_visit(), actual_row.last_visit());
    EXPECT_EQ(expected_row.url(), actual_row.url());
    const VisitInfoVector& expected_visits(expected_info.second.visits);
    const VisitInfoVector& actual_visits(actual_info->second.visits);
    EXPECT_EQ(expected_visits.size(), actual_visits.size());
    for (size_t i = 0;
         i < std::min(expected_visits.size(), actual_visits.size()); ++i) {
      EXPECT_EQ(expected_visits[i].first, actual_visits[i].first);
      EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
          actual_visits[i].second, expected_visits[i].second));
    }
  }

  for (const auto& expected_starts : expected.word_starts_map_) {
    auto actual_starts = actual.word_starts_map_.find(expected_starts.first);
    // NOTE(yfriedman): ASSERT_NE can't be used due to incompatibility between
    // gtest and STLPort in the Android build. See
    // http://code.google.com/p/googletest/issues/detail?id=359
    ASSERT_TRUE(actual_starts != actual.word_starts_map_.end());
    const RowWordStarts& expected_word_starts(expected_starts.second);
    const RowWordStarts& actual_word_starts(actual_starts->second);
    EXPECT_TRUE(base::ranges::equal(expected_word_starts.url_word_starts_,
                                    actual_word_starts.url_word_starts_));
    EXPECT_TRUE(base::ranges::equal(expected_word_starts.title_word_starts_,
                                    actual_word_starts.title_word_starts_));
  }
}

//------------------------------------------------------------------------------

class LimitedInMemoryURLIndexTest : public InMemoryURLIndexTest {
 protected:
  base::FilePath::StringType TestDBName() const override;
  bool InitializeInMemoryURLIndexInSetUp() const override;
};

base::FilePath::StringType LimitedInMemoryURLIndexTest::TestDBName() const {
  return FILE_PATH_LITERAL("in_memory_url_index_test_limited.sql");
}

bool LimitedInMemoryURLIndexTest::InitializeInMemoryURLIndexInSetUp() const {
  return false;
}

TEST_F(LimitedInMemoryURLIndexTest, Initialization) {
  // Verify that the database contains the expected number of items, which is
  // the pre-filtered count, i.e. all the items.
  sql::Statement statement(GetDB().GetUniqueStatement("SELECT * FROM urls;"));
  ASSERT_TRUE(statement.is_valid());
  uint64_t row_count = 0;
  while (statement.Step())
    ++row_count;
  EXPECT_EQ(1U, row_count);

  InitializeInMemoryURLIndex();
  URLIndexPrivateData& private_data(*GetPrivateData());

  // history_info_map_ should have the same number of items as were filtered.
  EXPECT_EQ(1U, private_data.history_info_map_.size());
  EXPECT_EQ(35U, private_data.char_word_map_.size());
  EXPECT_EQ(19U, private_data.word_map_.size());
}

TEST_F(InMemoryURLIndexTest, HiddenURLRowsAreIgnored) {
  history::URLID new_row_id = 87654321;  // Arbitrarily chosen large new row id.
  history::URLRow new_row =
      history::URLRow(GURL("http://hidden.com/"), new_row_id++);
  new_row.set_last_visit(base::Time::Now());
  new_row.set_hidden(true);

  EXPECT_FALSE(UpdateURL(new_row));
  EXPECT_EQ(0U, HistoryItemsForTerms(u"hidden", std::u16string::npos, "",
                                     kProviderMaxMatches)
                    .size());
}

TEST_F(InMemoryURLIndexTest, Retrieval) {
  // See if a very specific term gives a single result.
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"DrudgeReport", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(5, matches[0].url_info.id());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(u"DRUDGE REPORT 2010", matches[0].url_info.title());

  // Make sure a trailing space still results in the expected result.
  matches = HistoryItemsForTerms(u"DrudgeReport ", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(5, matches[0].url_info.id());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(u"DRUDGE REPORT 2010", matches[0].url_info.title());

  // Search which should result in multiple results.
  matches = HistoryItemsForTerms(u"drudge", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(2U, matches.size());
  // The results should be in descending score order.
  EXPECT_GE(matches[0].raw_score, matches[1].raw_score);

  // Search which should result in nearly perfect result.
  matches = HistoryItemsForTerms(u"Nearly Perfect Result", std::u16string::npos,
                                 "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  // The results should have a very high score.
  EXPECT_GT(matches[0].raw_score, 900);
  EXPECT_EQ(32, matches[0].url_info.id());
  EXPECT_EQ("https://nearlyperfectresult.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(u"Practically Perfect Search Result", matches[0].url_info.title());

  // Search which should result in very poor result.  (It's a mid-word match
  // in a hostname.)  No results since it will be suppressed by default scoring.
  matches = HistoryItemsForTerms(u"heinqui", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());
  // But if the user adds a term that matches well against the same result,
  // the result should be returned.
  matches = HistoryItemsForTerms(u"heinqui microprocessor",
                                 std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(18, matches[0].url_info.id());
  EXPECT_EQ("http://www.theinquirer.net/", matches[0].url_info.url().spec());
  EXPECT_EQ(
      u"THE INQUIRER - Microprocessor, Server, Memory, PCS, "
      u"Graphics, Networking, Storage",
      matches[0].url_info.title());

  // A URL that comes from the default search engine should not be returned.
  matches = HistoryItemsForTerms(u"query", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // But if it's not from the default search engine, it should be returned.
  TemplateURL* template_url = template_url_service()->GetTemplateURLForKeyword(
      kNonDefaultTemplateURLKeyword);
  template_url_service()->SetUserSelectedDefaultSearchProvider(template_url);
  matches = HistoryItemsForTerms(u"query", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // Search which will match at the end of a URL with encoded characters.
  matches = HistoryItemsForTerms(u"Mice", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(30, matches[0].url_info.id());

  // Check that URLs are not escaped an extra time.
  matches = HistoryItemsForTerms(u"1% wikipedia", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(35, matches[0].url_info.id());
  EXPECT_EQ("http://en.wikipedia.org/wiki/1%25_rule_(Internet_culture)",
            matches[0].url_info.url().spec());

  // Verify that a single term can appear multiple times in the URL.
  matches = HistoryItemsForTerms(u"fubar", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(34, matches[0].url_info.id());
  EXPECT_EQ("http://fubarfubarandfubar.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(u"Situation Normal -- FUBARED", matches[0].url_info.title());
}

// Regression test for crbug.com/1494484. Exercises a URL that is valid but may
// become invalid if handled with url_formatter::FormatUrl().
TEST_F(InMemoryURLIndexTest,
       RetrievalWithInternationalizedDomainNameWithInvalidCodePoint) {
  const GURL url("https://xn--b4ab3a0a.xn--b4aew.com/");

  ASSERT_TRUE(url.is_valid());
  ASSERT_FALSE(GURL(url_formatter::FormatUrl(
                        url, url_formatter::kFormatUrlOmitUsernamePassword,
                        base::UnescapeRule::NONE, nullptr, nullptr, nullptr))
                   .is_valid());

  history::URLID new_row_id = 87654321;  // Arbitrarily chosen large new row id.
  history::URLRow new_row = history::URLRow(url, new_row_id++);
  new_row.set_last_visit(base::Time::Now());

  EXPECT_TRUE(UpdateURL(new_row));
  EXPECT_EQ(
      1U, HistoryItemsForTerms(u"\u04a5\u049b\u049d", std::u16string::npos, "",
                               kProviderMaxMatches)
              .size());
}

TEST_F(InMemoryURLIndexTest, CursorPositionRetrieval) {
  // See if a very specific term with no cursor gives an empty result.
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"DrudReport", std::u16string::npos, "", kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // The same test with the cursor at the end should give an empty result.
  matches = HistoryItemsForTerms(u"DrudReport", 10u, "", kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // If the cursor is between Drud and Report, we should find the desired
  // result.
  matches = HistoryItemsForTerms(u"DrudReport", 4u, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(u"DRUDGE REPORT 2010", matches[0].url_info.title());

  // Now check multi-word inputs.  No cursor should fail to find a
  // result on this input.
  matches = HistoryItemsForTerms(u"MORTGAGERATE DROPS", std::u16string::npos,
                                 "", kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // Ditto with cursor at end.
  matches =
      HistoryItemsForTerms(u"MORTGAGERATE DROPS", 18u, "", kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // If the cursor is between MORTAGE And RATE, we should find the
  // desired result.
  matches =
      HistoryItemsForTerms(u"MORTGAGERATE DROPS", 8u, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ("http://www.reuters.com/article/idUSN0839880620100708",
            matches[0].url_info.url().spec());
  EXPECT_EQ(
      u"UPDATE 1-US 30-yr mortgage rate drops to new record low | Reuters",
      matches[0].url_info.title());
}

TEST_F(InMemoryURLIndexTest, URLPrefixMatching) {
  // "drudgere" - found
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"drudgere", std::u16string::npos, "", kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // "www.atdmt" - not found
  matches = HistoryItemsForTerms(u"www.atdmt", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // "atdmt" - found
  matches = HistoryItemsForTerms(u"atdmt", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // "view.atdmt" - found
  matches = HistoryItemsForTerms(u"view.atdmt", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // "view.atdmt" - found
  matches = HistoryItemsForTerms(u"view.atdmt", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // "cnn.com" - found
  matches = HistoryItemsForTerms(u"cnn.com", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(2U, matches.size());

  // "www.cnn.com" - found
  matches = HistoryItemsForTerms(u"www.cnn.com", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // "ww.cnn.com" - found because we suppress mid-term matches.
  matches = HistoryItemsForTerms(u"ww.cnn.com", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // "www.cnn.com" - found
  matches = HistoryItemsForTerms(u"www.cnn.com", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());

  // "tp://www.cnn.com" - not found because we don't allow tp as a mid-term
  // match
  matches = HistoryItemsForTerms(u"tp://www.cnn.com", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());
}

TEST_F(InMemoryURLIndexTest, ProperStringMatching) {
  // Search for the following with the expected results:
  // "atdmt view" - found
  // "atdmt.view" - not found
  // "view.atdmt" - found
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"atdmt view", std::u16string::npos, "", kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());
  matches = HistoryItemsForTerms(u"atdmt.view", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());
  matches = HistoryItemsForTerms(u"view.atdmt", std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(1U, matches.size());
}

TEST_F(InMemoryURLIndexTest, TrimHistoryIds) {
  // Constants ---------------------------------------------------------------

  constexpr size_t kItemsToScoreLimit = 500;
  constexpr size_t kAlmostLimit = kItemsToScoreLimit - 5;

  constexpr int kLowTypedCount = 2;
  constexpr int kHighTypedCount = 100;

  constexpr int kLowVisitCount = 20;
  constexpr int kHighVisitCount = 200;

  constexpr base::TimeDelta kOld = base::Days(15);
  constexpr base::TimeDelta kNew = base::Days(2);

  constexpr int kMinRowId = 5000;

  // Helpers -----------------------------------------------------------------

  struct ItemGroup {
    history::URLID min_id;
    history::URLID max_id;
    int typed_count;
    int visit_count;
    base::TimeDelta days_ago;
  };

  auto GetHistoryIdsUpTo = [&](HistoryID max) {
    HistoryIDVector res(max - kMinRowId);
    std::iota(res.begin(), res.end(), kMinRowId);
    return res;
  };

  auto CountGroupElementsInIds = [](const ItemGroup& group,
                                    const HistoryIDVector& ids) {
    return base::ranges::count_if(ids, [&](history::URLID id) {
      return group.min_id <= id && id < group.max_id;
    });
  };

  // Test body --------------------------------------------------------------

  // Groups of items ordered by increasing priority.
  ItemGroup item_groups[] = {
      {0, 0, kLowTypedCount, kLowVisitCount, kOld},
      {0, 0, kLowTypedCount, kLowVisitCount, kNew},
      {0, 0, kLowTypedCount, kHighVisitCount, kOld},
      {0, 0, kLowTypedCount, kHighVisitCount, kNew},
      {0, 0, kHighTypedCount, kLowVisitCount, kOld},
      {0, 0, kHighTypedCount, kLowVisitCount, kNew},
      {0, 0, kHighTypedCount, kHighVisitCount, kOld},
      {0, 0, kHighTypedCount, kHighVisitCount, kNew},
  };

  // Initialize groups.
  history::URLID row_id = kMinRowId;
  for (auto& group : item_groups) {
    group.min_id = row_id;
    for (size_t i = 0; i < kAlmostLimit; ++i) {
      history::URLRow new_row(
          GURL("http://www.fake_url" + base::NumberToString(row_id) + ".com"),
          row_id);
      new_row.set_typed_count(group.typed_count);
      new_row.set_visit_count(group.visit_count);
      new_row.set_last_visit(base::Time::Now() - group.days_ago);
      UpdateURL(std::move(new_row));
      ++row_id;
    }
    group.max_id = row_id;
  }

  // First group. Because number of entries is small enough no trimming occurs.
  {
    auto ids = GetHistoryIdsUpTo(item_groups[0].max_id);
    EXPECT_FALSE(GetPrivateData()->TrimHistoryIdsPool(&ids));
    EXPECT_EQ(kAlmostLimit, ids.size());
  }

  // Each next group should fill almost everything, while the previous group
  // should occupy what's left.
  auto* error_position = base::ranges::adjacent_find(
      item_groups, [&](const ItemGroup& previous, const ItemGroup& current) {
        auto ids = GetHistoryIdsUpTo(current.max_id);
        EXPECT_TRUE(GetPrivateData()->TrimHistoryIdsPool(&ids));

        size_t current_count = CountGroupElementsInIds(current, ids);
        EXPECT_EQ(kAlmostLimit, current_count);
        if (current_count != kAlmostLimit)
          return true;

        size_t previous_count = CountGroupElementsInIds(previous, ids);
        EXPECT_EQ(kItemsToScoreLimit - kAlmostLimit, previous_count);
        return previous_count != kItemsToScoreLimit - kAlmostLimit;
      });

  EXPECT_TRUE(error_position == std::end(item_groups))
      << "broken after: " << error_position - std::begin(item_groups);
}

TEST_F(InMemoryURLIndexTest, HugeResultSet) {
  // Create a huge set of qualifying history items.
  for (history::URLID row_id = 5000; row_id < 6000; ++row_id) {
    history::URLRow new_row(GURL("http://www.brokeandaloneinmanitoba.com/"),
                            row_id);
    new_row.set_last_visit(base::Time::Now());
    EXPECT_TRUE(UpdateURL(new_row));
  }

  ScoredHistoryMatches matches =
      HistoryItemsForTerms(u"b", std::u16string::npos, "", kProviderMaxMatches);
  EXPECT_EQ(kProviderMaxMatches, matches.size());
}

TEST_F(InMemoryURLIndexTest, TitleSearch) {
  // Signal if someone has changed the test DB.
  EXPECT_EQ(30U, GetPrivateData()->history_info_map_.size());

  // Ensure title is being searched.
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"MORTGAGE RATE DROPS", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(1, matches[0].url_info.id());
  EXPECT_EQ("http://www.reuters.com/article/idUSN0839880620100708",
            matches[0].url_info.url().spec());
  EXPECT_EQ(
      u"UPDATE 1-US 30-yr mortgage rate drops to new record low | Reuters",
      matches[0].url_info.title());
}

TEST_F(InMemoryURLIndexTest, TitleChange) {
  // Verify current title terms retrieves desired item.
  std::u16string original_terms = u"lebronomics could high taxes influence";
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      original_terms, std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  const history::URLID expected_id = 3;
  EXPECT_EQ(expected_id, matches[0].url_info.id());
  EXPECT_EQ("http://www.businessandmedia.org/articles/2010/20100708120415.aspx",
            matches[0].url_info.url().spec());
  EXPECT_EQ(u"LeBronomics: Could High Taxes Influence James' Team Decision?",
            matches[0].url_info.title());
  history::URLRow old_row(matches[0].url_info);

  // Verify new title terms retrieves nothing.
  std::u16string new_terms = u"does eat oats little lambs ivy";
  matches = HistoryItemsForTerms(new_terms, std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());

  // Update the row.
  old_row.set_title(u"Does eat oats and little lambs eat ivy");
  EXPECT_TRUE(UpdateURL(old_row));

  // Verify we get the row using the new terms but not the original terms.
  matches = HistoryItemsForTerms(new_terms, std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(expected_id, matches[0].url_info.id());
  matches = HistoryItemsForTerms(original_terms, std::u16string::npos, "",
                                 kProviderMaxMatches);
  EXPECT_EQ(0U, matches.size());
}

TEST_F(InMemoryURLIndexTest, NonUniqueTermCharacterSets) {
  // The presence of duplicate characters should succeed. Exercise by cycling
  // through a string with several duplicate characters.
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"ABRA", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());
  EXPECT_EQ("http://www.ddj.com/windows/184416623",
            matches[0].url_info.url().spec());

  matches = HistoryItemsForTerms(u"ABRACAD", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = HistoryItemsForTerms(u"ABRACADABRA", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = HistoryItemsForTerms(u"ABRACADABR", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = HistoryItemsForTerms(u"ABRACA", std::u16string::npos, "",
                                 kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());
}

TEST_F(InMemoryURLIndexTest, TypedCharacterCaching) {
  // Verify that match results for previously typed characters are retained
  // (in the term_char_word_set_cache_) and reused, if possible, in future
  // autocompletes.

  URLIndexPrivateData::SearchTermCacheMap& cache(
      GetPrivateData()->search_term_cache_);

  // The cache should be empty at this point.
  EXPECT_EQ(0U, cache.size());

  // Now simulate typing search terms into the omnibox and check the state of
  // the cache as each item is 'typed'.

  // Simulate typing "r" giving "r" in the simulated omnibox. The results for
  // 'r' will be not cached because it is only 1 character long.
  HistoryItemsForTerms(u"r", std::u16string::npos, "", kProviderMaxMatches);
  EXPECT_EQ(0U, cache.size());

  // Simulate typing "re" giving "r re" in the simulated omnibox.
  // 're' should be cached at this point but not 'r' as it is a single
  // character.
  HistoryItemsForTerms(u"r re", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, cache.size());
  CheckTerm(cache, u"re");

  // Simulate typing "reco" giving "r re reco" in the simulated omnibox.
  // 're' and 'reco' should be cached at this point but not 'r' as it is a
  // single character.
  HistoryItemsForTerms(u"r re reco", std::u16string::npos, "",
                       kProviderMaxMatches);
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, u"re");
  CheckTerm(cache, u"reco");

  // Simulate typing "mort".
  // Since we now have only one search term, the cached results for 're' and
  // 'reco' should be purged, giving us only 1 item in the cache (for 'mort').
  HistoryItemsForTerms(u"mort", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, cache.size());
  CheckTerm(cache, u"mort");

  // Simulate typing "reco" giving "mort reco" in the simulated omnibox.
  HistoryItemsForTerms(u"mort reco", std::u16string::npos, "",
                       kProviderMaxMatches);
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, u"mort");
  CheckTerm(cache, u"reco");

  // Simulate a <DELETE> by removing the 'reco' and adding back the 'rec'.
  HistoryItemsForTerms(u"mort rec", std::u16string::npos, "",
                       kProviderMaxMatches);
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, u"mort");
  CheckTerm(cache, u"rec");
}

TEST_F(InMemoryURLIndexTest, AddNewRows) {
  // Verify that the row we're going to add does not already exist.
  history::URLID new_row_id = 87654321;
  // Newly created history::URLRows get a last_visit time of 'right now' so it
  // should
  // qualify as a quick result candidate.
  EXPECT_TRUE(HistoryItemsForTerms(u"brokeandalone", std::u16string::npos, "",
                                   kProviderMaxMatches)
                  .empty());

  // Add a new row.
  history::URLRow new_row(GURL("http://www.brokeandaloneinmanitoba.com/"),
                          new_row_id++);
  new_row.set_last_visit(base::Time::Now());
  EXPECT_TRUE(UpdateURL(new_row));

  // Verify that we can retrieve it.
  EXPECT_EQ(1U, HistoryItemsForTerms(u"brokeandalone", std::u16string::npos, "",
                                     kProviderMaxMatches)
                    .size());

  // Add it again just to be sure that is harmless and that it does not update
  // the index.
  EXPECT_FALSE(UpdateURL(new_row));
  EXPECT_EQ(1U, HistoryItemsForTerms(u"brokeandalone", std::u16string::npos, "",
                                     kProviderMaxMatches)
                    .size());

  // Make up a URL that does not qualify and try to add it.
  history::URLRow unqualified_row(
      GURL("http://www.brokeandaloneinmanitoba.com/"), new_row_id++);
  EXPECT_FALSE(UpdateURL(new_row));
}

TEST_F(InMemoryURLIndexTest, DeleteRows) {
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"DrudgeReport", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());

  // Delete the URL then search again.
  EXPECT_TRUE(DeleteURL(matches[0].url_info.url()));
  EXPECT_TRUE(HistoryItemsForTerms(u"DrudgeReport", std::u16string::npos, "",
                                   kProviderMaxMatches)
                  .empty());

  // Make up a URL that does not exist in the database and delete it.
  GURL url("http://www.hokeypokey.com/putyourrightfootin.html");
  EXPECT_FALSE(DeleteURL(url));
}

TEST_F(InMemoryURLIndexTest, ExpireRow) {
  ScoredHistoryMatches matches = HistoryItemsForTerms(
      u"DrudgeReport", std::u16string::npos, "", kProviderMaxMatches);
  ASSERT_EQ(1U, matches.size());

  // Determine the row ID for the result, remember that ID, broadcast a delete
  // notification, then ensure that the row has been deleted.
  history::URLRows deleted_rows;
  deleted_rows.push_back(matches[0].url_info);
  url_index_->OnHistoryDeletions(
      nullptr, history::DeletionInfo::ForUrls(deleted_rows, std::set<GURL>()));
  EXPECT_TRUE(HistoryItemsForTerms(u"DrudgeReport", std::u16string::npos, "",
                                   kProviderMaxMatches)
                  .empty());
}

TEST_F(InMemoryURLIndexTest, AllowlistedURLs) {
  std::string client_allowlisted_url =
      base::StringPrintf("%s://foo", kClientAllowlistedScheme);
  struct TestData {
    const std::string url_spec;
    const bool expected_is_allowlisted;
  } data[] = {
      // URLs with allowlisted schemes.
      {"about:histograms", true},
      {"file://localhost/Users/joeschmoe/sekrets", true},
      {"ftp://public.mycompany.com/myfile.txt", true},
      {"http://www.google.com/translate", true},
      {"https://www.gmail.com/", true},
      {"mailto:support@google.com", true},
      {client_allowlisted_url, true},
      // URLs with unacceptable schemes.
      {"aaa://www.dummyhost.com;frammy", false},
      {"aaas://www.dummyhost.com;frammy", false},
      {"acap://suzie@somebody.com", false},
      {"cap://cal.example.com/Company/Holidays", false},
      {"cid:foo4*foo1@bar.net", false},
      {"crid://example.com/foobar", false},
      {"data:image/png;base64,iVBORw0KGgoAAAANSUhE=", false},
      {"dict://dict.org/d:shortcake:", false},
      {"dns://192.168.1.1/ftp.example.org?type=A", false},
      {"fax:+358.555.1234567", false},
      {"geo:13.4125,103.8667", false},
      {"go:Mercedes%20Benz", false},
      {"gopher://farnsworth.ca:666/gopher", false},
      {"h323:farmer-john;sixpence", false},
      {"iax:johnQ@example.com/12022561414", false},
      {"icap://icap.net/service?mode=translate&lang=french", false},
      {"im:fred@example.com", false},
      {"imap://michael@minbari.org/users.*", false},
      {"info:ddc/22/eng//004.678", false},
      {"ipp://example.com/printer/fox", false},
      {"iris:dreg1//example.com/local/myhosts", false},
      {"iris.beep:dreg1//example.com/local/myhosts", false},
      {"iris.lws:dreg1//example.com/local/myhosts", false},
      {"iris.xpc:dreg1//example.com/local/myhosts", false},
      {"iris.xpcs:dreg1//example.com/local/myhosts", false},
      {"ldap://ldap.itd.umich.edu/o=University%20of%20Michigan,c=US", false},
      {"mid:foo4%25foo1@bar.net", false},
      {"modem:+3585551234567;type=v32b?7e1;type=v110", false},
      {"msrp://atlanta.example.com:7654/jshA7weztas;tcp", false},
      {"msrps://atlanta.example.com:7654/jshA7weztas;tcp", false},
      {"news:colorectal.info.banned", false},
      {"nfs://server/d/e/f", false},
      {"nntp://www.example.com:6543/info.comp.lies/1234", false},
      {"pop://rg;AUTH=+APOP@mail.mycompany.com:8110", false},
      {"pres:fred@example.com", false},
      {"prospero://host.dom//pros/name", false},
      {"rsync://syler@lost.com/Source", false},
      {"rtsp://media.example.com:554/twister/audiotrack", false},
      {"acap://some.where.net;authentication=KERBEROSV4", false},
      {"shttp://www.terces.com/secret", false},
      {"sieve://example.com//script", false},
      {"sip:+1-212-555-1212:1234@gateway.com;user=phone", false},
      {"sips:+1-212-555-1212:1234@gateway.com;user=phone", false},
      {"sms:+15105551212?body=hello%20there", false},
      {"snmp://tester5@example.com:8161/bridge1;800002b804616263", false},
      {"soap.beep://stockquoteserver.example.com/StockQuote", false},
      {"soap.beeps://stockquoteserver.example.com/StockQuote", false},
      {"tag:blogger.com,1999:blog-555", false},
      {"tel:+358-555-1234567;postd=pp22", false},
      {"telnet://mayor_margie:one2rule4All@www.mycity.com:6789/", false},
      {"tftp://example.com/mystartupfile", false},
      {"tip://123.123.123.123/?urn:xopen:xid", false},
      {"tv:nbc.com", false},
      {"urn:foo:A123,456", false},
      {"vemmi://zeus.mctel.fr/demo", false},
      {"wais://www.mydomain.net:8765/mydatabase", false},
      {"xmpp:node@example.com", false},
      {"xmpp://guest@example.com", false},
  };

  const SchemeSet& allowlist(scheme_allowlist());
  for (const auto& d : data) {
    GURL url(d.url_spec);
    EXPECT_EQ(d.expected_is_allowlisted,
              URLIndexPrivateData::URLSchemeIsAllowlisted(url, allowlist));
  }
}

TEST_F(InMemoryURLIndexTest, ReadVisitsFromHistory) {
  const HistoryInfoMap& history_info_map = GetPrivateData()->history_info_map_;

  // Check (for URL with id 1) that the number of visits and their
  // transition types are what we expect.  We don't bother checking
  // the timestamps because it's too much trouble.  (The timestamps go
  // through a transformation in InMemoryURLIndexTest::SetUp().  We
  // assume that if the count and transitions show up with the right
  // information, we're getting the right information from the history
  // database file.)
  auto entry = history_info_map.find(1);
  ASSERT_TRUE(entry != history_info_map.end());
  {
    const VisitInfoVector& visits = entry->second.visits;
    ASSERT_EQ(3u, visits.size());
    EXPECT_EQ(0, static_cast<int32_t>(visits[0].second));
    EXPECT_EQ(1, static_cast<int32_t>(visits[1].second));
    EXPECT_EQ(0, static_cast<int32_t>(visits[2].second));
  }

  // Ditto but for URL with id 35.
  entry = history_info_map.find(35);
  ASSERT_TRUE(entry != history_info_map.end());
  {
    const VisitInfoVector& visits = entry->second.visits;
    ASSERT_EQ(2, static_cast<int32_t>(visits.size()));
    EXPECT_EQ(1, static_cast<int32_t>(visits[0].second));
    EXPECT_EQ(1, static_cast<int32_t>(visits[1].second));
  }

  // The URL with id 32 has many visits listed in the database, but we
  // should only read the most recent 10 (which are all transition type 0).
  entry = history_info_map.find(32);
  ASSERT_TRUE(entry != history_info_map.end());
  {
    const VisitInfoVector& visits = entry->second.visits;
    EXPECT_EQ(10u, visits.size());
    for (const auto& visit : visits)
      EXPECT_EQ(0, static_cast<int32_t>(visit.second));
  }
}

TEST_F(InMemoryURLIndexTest, CalculateWordStartsOffsets) {
  const struct {
    const char* search_string;
    size_t cursor_position;
    const size_t expected_word_starts_offsets_size;
    const size_t expected_word_starts_offsets[3];
  } test_cases[] = {
      /* No punctuations, only cursor position change. */
      {"ABCD", kInvalid, 1, {0, kInvalid, kInvalid}},
      {"abcd", 0, 1, {0, kInvalid, kInvalid}},
      {"AbcD", 1, 2, {0, 0, kInvalid}},
      {"abcd", 4, 1, {0, kInvalid, kInvalid}},

      /* Starting with punctuation. */
      {".abcd", kInvalid, 1, {1, kInvalid, kInvalid}},
      {".abcd", 0, 1, {1, kInvalid, kInvalid}},
      {"!abcd", 1, 2, {1, 0, kInvalid}},
      {"::abcd", 1, 2, {1, 1, kInvalid}},
      {":abcd", 5, 1, {1, kInvalid, kInvalid}},

      /* Ending with punctuation. */
      {"abcd://", kInvalid, 1, {0, kInvalid, kInvalid}},
      {"ABCD://", 0, 1, {0, kInvalid, kInvalid}},
      {"abcd://", 1, 2, {0, 0, kInvalid}},
      {"abcd://", 4, 2, {0, 3, kInvalid}},
      {"abcd://", 7, 1, {0, kInvalid, kInvalid}},

      /* Punctuation in the middle. */
      {"ab.cd", kInvalid, 1, {0, kInvalid, kInvalid}},
      {"ab.cd", 0, 1, {0, kInvalid, kInvalid}},
      {"ab!cd", 1, 2, {0, 0, kInvalid}},
      {"AB.cd", 2, 2, {0, 1, kInvalid}},
      {"AB.cd", 3, 2, {0, 0, kInvalid}},
      {"ab:cd", 5, 1, {0, kInvalid, kInvalid}},

      /* Hyphenation */
      {"Ab-cd", kInvalid, 1, {0, kInvalid, kInvalid}},
      {"ab-cd", 0, 1, {0, kInvalid, kInvalid}},
      {"-abcd", 0, 1, {1, kInvalid, kInvalid}},
      {"-abcd", 1, 2, {1, 0, kInvalid}},
      {"abcd-", 2, 2, {0, 0, kInvalid}},
      {"abcd-", 4, 2, {0, 1, kInvalid}},
      {"ab-cd", 5, 1, {0, kInvalid, kInvalid}},

      /* Whitespace */
      {"Ab cd", kInvalid, 2, {0, 0, kInvalid}},
      {"ab cd", 0, 2, {0, 0, kInvalid}},
      {" abcd", 0, 1, {0, kInvalid, kInvalid}},
      {" abcd", 1, 1, {0, kInvalid, kInvalid}},
      {"abcd ", 2, 2, {0, 0, kInvalid}},
      {"abcd :", 4, 2, {0, 1, kInvalid}},
      {"abcd :", 5, 2, {0, 1, kInvalid}},
      {"abcd :", 2, 3, {0, 0, 1}},

      /* Underscore */
      {"Ab_cd", kInvalid, 1, {0, kInvalid, kInvalid}},
      {"ab_cd", 0, 1, {0, kInvalid, kInvalid}},
      {"_abcd", 0, 1, {1, kInvalid, kInvalid}},
      {"_abcd", 1, 2, {1, 0, kInvalid}},
      {"abcd_", 2, 2, {0, 0, kInvalid}},
      {"abcd_", 4, 2, {0, 1, kInvalid}},
      {"ab_cd", 5, 1, {0, kInvalid, kInvalid}},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "search_string = " << test_case.search_string
                 << ", cursor_position = " << test_case.cursor_position);

    std::u16string lower_string;
    String16Vector lower_terms;
    StringToTerms(test_case.search_string, test_case.cursor_position,
                  &lower_string, &lower_terms);
    WordStarts lower_terms_to_word_starts_offsets;
    URLIndexPrivateData::CalculateWordStartsOffsets(
        lower_terms, &lower_terms_to_word_starts_offsets);

    // Verify against expectations.
    EXPECT_EQ(test_case.expected_word_starts_offsets_size,
              lower_terms_to_word_starts_offsets.size());
    for (size_t j = 0; j < test_case.expected_word_starts_offsets_size; ++j) {
      EXPECT_EQ(test_case.expected_word_starts_offsets[j],
                lower_terms_to_word_starts_offsets[j]);
    }
  }
}
