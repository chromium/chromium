// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_index_private_data.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class URLIndexPrivateDataTest : public testing::Test {
 protected:
  void SetUp() override {
    data_ = base::MakeRefCounted<URLIndexPrivateData>();
    scheme_allowlist_.insert("http");
    scheme_allowlist_.insert("https");
  }

  // Creates a URLRow with the given URL, id, and optional metadata.
  history::URLRow MakeRow(const std::string& url_spec,
                          history::URLID id,
                          int visit_count = 1,
                          int typed_count = 1) {
    history::URLRow row(GURL(url_spec), id);
    row.set_visit_count(visit_count);
    row.set_typed_count(typed_count);
    row.set_last_visit(base::Time::Now());
    row.set_title(u"Test Title");
    return row;
  }

  // Indexes a row using IndexRow with null history_db and history_service.
  bool IndexRow(const history::URLRow& row) {
    return data_->IndexRow(/*history_db=*/nullptr, /*history_service=*/nullptr,
                           row, scheme_allowlist_, /*tracker=*/nullptr);
  }

  // Indexes a row using the batch-visit path.
  bool IndexRowWithPreFetchedVisits(
      const history::URLRow& row,
      const history::HistoryDatabase::RecentVisitsMap& batch_visits) {
    return data_->IndexRowWithPreFetchedVisits(row, scheme_allowlist_,
                                               batch_visits);
  }

  // Accessor helpers. The fixture is a friend of URLIndexPrivateData, so
  // these methods can access private members. Derived TEST_F classes inherit
  // them without needing individual FRIEND_TEST entries.
  const HistoryInfoMap& history_info_map() const {
    return data_->history_info_map_;
  }
  const String16Vector& word_list() const { return data_->word_list_; }
  const WordMap& word_map() const { return data_->word_map_; }
  const CharWordIDMap& char_word_map() const { return data_->char_word_map_; }
  const WordIDHistoryMap& word_id_history_map() const {
    return data_->word_id_history_map_;
  }
  const HistoryIDWordMap& history_id_word_map() const {
    return data_->history_id_word_map_;
  }
  const WordStartsMap& word_starts_map() const {
    return data_->word_starts_map_;
  }
  URLIndexPrivateData::SearchTermCacheMap& search_term_cache() {
    return data_->search_term_cache_;
  }

  static bool URLSchemeIsAllowlisted(const GURL& gurl,
                                     const std::set<std::string>& allowlist) {
    return URLIndexPrivateData::URLSchemeIsAllowlisted(gurl, allowlist);
  }

  // Compares the internal index state of two URLIndexPrivateData instances.
  void ExpectIndexDataEqual(const URLIndexPrivateData& a,
                            const URLIndexPrivateData& b) {
    EXPECT_EQ(a.word_list_.size(), b.word_list_.size());
    for (size_t i = 0; i < a.word_list_.size(); ++i) {
      EXPECT_EQ(a.word_list_[i], b.word_list_[i]);
    }
    EXPECT_EQ(a.word_map_.size(), b.word_map_.size());
    EXPECT_EQ(a.char_word_map_.size(), b.char_word_map_.size());
    EXPECT_EQ(a.word_id_history_map_.size(), b.word_id_history_map_.size());
    EXPECT_EQ(a.history_id_word_map_.size(), b.history_id_word_map_.size());
    EXPECT_EQ(a.history_info_map_.size(), b.history_info_map_.size());
    EXPECT_EQ(a.word_starts_map_.size(), b.word_starts_map_.size());
  }

  // Indexes a row on an arbitrary URLIndexPrivateData instance using IndexRow.
  bool IndexRowOn(URLIndexPrivateData* target, const history::URLRow& row) {
    return target->IndexRow(nullptr, nullptr, row, scheme_allowlist_, nullptr);
  }

  // Indexes a row on an arbitrary URLIndexPrivateData instance using the
  // batch-visit path.
  bool IndexRowWithPreFetchedVisitsOn(
      URLIndexPrivateData* target,
      const history::URLRow& row,
      const history::HistoryDatabase::RecentVisitsMap& batch_visits) {
    return target->IndexRowWithPreFetchedVisits(row, scheme_allowlist_,
                                                batch_visits);
  }

  // Inserts a default SearchTermCacheItem into the search term cache.
  void InsertCacheEntry(const std::u16string& term) {
    data_->search_term_cache_[term] =
        URLIndexPrivateData::SearchTermCacheItem();
  }

  scoped_refptr<URLIndexPrivateData> data_;
  std::set<std::string> scheme_allowlist_;
};

// --- Construction and basic state ---

TEST_F(URLIndexPrivateDataTest, EmptyByDefault) {
  EXPECT_TRUE(data_->Empty());
}

TEST_F(URLIndexPrivateDataTest, ClearResetsData) {
  ASSERT_TRUE(IndexRow(MakeRow("http://example.com/", 1)));
  EXPECT_FALSE(data_->Empty());

  data_->Clear();

  EXPECT_TRUE(data_->Empty());
  EXPECT_TRUE(word_list().empty());
  EXPECT_TRUE(word_map().empty());
  EXPECT_TRUE(char_word_map().empty());
  EXPECT_TRUE(word_id_history_map().empty());
  EXPECT_TRUE(history_id_word_map().empty());
  EXPECT_TRUE(history_info_map().empty());
  EXPECT_TRUE(word_starts_map().empty());
}

// --- URL scheme allowlisting ---

TEST_F(URLIndexPrivateDataTest, AllowlistedSchemes) {
  std::set<std::string> allowlist = {"http", "https", "ftp"};

  EXPECT_TRUE(URLSchemeIsAllowlisted(GURL("http://example.com"), allowlist));
  EXPECT_TRUE(URLSchemeIsAllowlisted(GURL("https://example.com"), allowlist));
  EXPECT_TRUE(URLSchemeIsAllowlisted(GURL("ftp://example.com"), allowlist));
  EXPECT_FALSE(URLSchemeIsAllowlisted(GURL("chrome://settings"), allowlist));
  EXPECT_FALSE(URLSchemeIsAllowlisted(GURL("data:text/html,hello"), allowlist));
}

// --- IndexRow ---

TEST_F(URLIndexPrivateDataTest, IndexRowWithAllowlistedScheme) {
  history::URLRow row = MakeRow("http://example.com/path", 1);
  EXPECT_TRUE(IndexRow(row));
  EXPECT_FALSE(data_->Empty());
  EXPECT_EQ(1u, history_info_map().size());

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ("http://example.com/path", it->second.url_row.url().spec());
}

TEST_F(URLIndexPrivateDataTest, IndexRowRejectsNonAllowlistedScheme) {
  history::URLRow row = MakeRow("chrome://settings", 1);
  EXPECT_FALSE(IndexRow(row));
  EXPECT_TRUE(data_->Empty());
}

TEST_F(URLIndexPrivateDataTest, IndexRowStripsCredentials) {
  history::URLRow row = MakeRow("http://user:pass@example.com/secret", 1);
  EXPECT_TRUE(IndexRow(row));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ("http://example.com/secret", it->second.url_row.url().spec());
  EXPECT_FALSE(it->second.url_row.url().has_username());
  EXPECT_FALSE(it->second.url_row.url().has_password());
}

TEST_F(URLIndexPrivateDataTest, IndexRowWithoutCredentialsPreservesURL) {
  history::URLRow row = MakeRow("http://example.com/page", 1);
  EXPECT_TRUE(IndexRow(row));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ("http://example.com/page", it->second.url_row.url().spec());
}

TEST_F(URLIndexPrivateDataTest, IndexRowCopiesMetadata) {
  history::URLRow row = MakeRow("http://example.com/", 1, /*visit_count=*/5,
                                /*typed_count=*/3);
  row.set_title(u"My Page Title");
  EXPECT_TRUE(IndexRow(row));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ(5, it->second.url_row.visit_count());
  EXPECT_EQ(3, it->second.url_row.typed_count());
  EXPECT_EQ(u"My Page Title", it->second.url_row.title());
}

// --- IndexRowWithPreFetchedVisits ---

TEST_F(URLIndexPrivateDataTest, IndexRowWithPreFetchedVisits_Basic) {
  history::HistoryDatabase::RecentVisitsMap batch_visits;
  history::URLRow row = MakeRow("http://example.com/batch", 1);
  EXPECT_TRUE(IndexRowWithPreFetchedVisits(row, batch_visits));
  EXPECT_FALSE(data_->Empty());
  EXPECT_EQ(1u, history_info_map().size());
}

TEST_F(URLIndexPrivateDataTest,
       IndexRowWithPreFetchedVisits_RejectsNonAllowlistedScheme) {
  history::HistoryDatabase::RecentVisitsMap batch_visits;
  history::URLRow row = MakeRow("chrome://flags", 1);
  EXPECT_FALSE(IndexRowWithPreFetchedVisits(row, batch_visits));
  EXPECT_TRUE(data_->Empty());
}

TEST_F(URLIndexPrivateDataTest, IndexRowWithPreFetchedVisits_StoresVisitData) {
  const base::Time now = base::Time::Now();
  history::HistoryDatabase::RecentVisitsMap batch_visits;
  batch_visits[1] = {
      {now - base::Hours(1), ui::PAGE_TRANSITION_TYPED},
      {now - base::Hours(2), ui::PAGE_TRANSITION_LINK},
  };

  history::URLRow row = MakeRow("http://example.com/visited", 1);
  EXPECT_TRUE(IndexRowWithPreFetchedVisits(row, batch_visits));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  const VisitInfoVector& visits = it->second.visits;
  ASSERT_EQ(2u, visits.size());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      visits[0].second, ui::PAGE_TRANSITION_TYPED));
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      visits[1].second, ui::PAGE_TRANSITION_LINK));
}

TEST_F(URLIndexPrivateDataTest, IndexRowWithPreFetchedVisits_NoVisitData) {
  // When the URL is not in batch_visits, visits should be empty.
  history::HistoryDatabase::RecentVisitsMap batch_visits;
  history::URLRow row = MakeRow("http://example.com/novisits", 1);
  EXPECT_TRUE(IndexRowWithPreFetchedVisits(row, batch_visits));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_TRUE(it->second.visits.empty());
}

TEST_F(URLIndexPrivateDataTest, IndexRowWithPreFetchedVisits_CapsVisitsAtMax) {
  const base::Time now = base::Time::Now();
  history::HistoryDatabase::RecentVisitsMap batch_visits;
  auto& visits_vec = batch_visits[1];
  for (size_t i = 0; i < URLIndexPrivateData::kMaxVisitsToStoreInCache + 5;
       ++i) {
    visits_vec.push_back(
        {now - base::Hours(static_cast<int>(i)), ui::PAGE_TRANSITION_TYPED});
  }

  history::URLRow row = MakeRow("http://example.com/many", 1);
  EXPECT_TRUE(IndexRowWithPreFetchedVisits(row, batch_visits));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ(URLIndexPrivateData::kMaxVisitsToStoreInCache,
            it->second.visits.size());
}

TEST_F(URLIndexPrivateDataTest,
       IndexRowWithPreFetchedVisits_StripsCredentials) {
  history::HistoryDatabase::RecentVisitsMap batch_visits;
  history::URLRow row = MakeRow("http://admin:secret@example.com/panel", 1);
  EXPECT_TRUE(IndexRowWithPreFetchedVisits(row, batch_visits));

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ("http://example.com/panel", it->second.url_row.url().spec());
}

// --- Parity between IndexRow and IndexRowWithPreFetchedVisits ---

TEST_F(URLIndexPrivateDataTest, IndexingParityBetweenBothPaths) {
  // Verify that both indexing paths produce equivalent internal index state
  // for the same input row (excluding visit data, which differs by design).
  history::URLRow row = MakeRow("http://parity.test.com/path", 1);
  row.set_title(u"Parity Test Page");

  auto data_a = base::MakeRefCounted<URLIndexPrivateData>();
  IndexRowOn(data_a.get(), row);

  auto data_b = base::MakeRefCounted<URLIndexPrivateData>();
  history::HistoryDatabase::RecentVisitsMap empty_visits;
  IndexRowWithPreFetchedVisitsOn(data_b.get(), row, empty_visits);

  ExpectIndexDataEqual(*data_a, *data_b);
}

// --- DeleteURL ---

TEST_F(URLIndexPrivateDataTest, DeleteURLRemovesEntry) {
  history::URLRow row = MakeRow("http://example.com/todelete", 1);
  ASSERT_TRUE(IndexRow(row));
  EXPECT_FALSE(data_->Empty());

  EXPECT_TRUE(data_->DeleteURL(GURL("http://example.com/todelete")));
  EXPECT_TRUE(data_->Empty());
}

TEST_F(URLIndexPrivateDataTest, DeleteURLReturnsFalseForMissing) {
  EXPECT_FALSE(data_->DeleteURL(GURL("http://nonexistent.com/")));
}

TEST_F(URLIndexPrivateDataTest, DeleteURLWithCredentialsMustUseSanitizedURL) {
  // IndexRow strips credentials, so deletion must use the sanitized URL.
  history::URLRow row = MakeRow("http://user:pass@example.com/auth", 1);
  ASSERT_TRUE(IndexRow(row));

  // Deleting with the original credentialed URL won't find the entry.
  EXPECT_FALSE(data_->DeleteURL(GURL("http://user:pass@example.com/auth")));

  // Deleting with the sanitized URL works.
  EXPECT_TRUE(data_->DeleteURL(GURL("http://example.com/auth")));
  EXPECT_TRUE(data_->Empty());
}

// --- UpdateRecentVisits ---

TEST_F(URLIndexPrivateDataTest, UpdateRecentVisits) {
  history::URLRow row = MakeRow("http://example.com/visit", 1);
  ASSERT_TRUE(IndexRow(row));

  history::VisitVector visits;
  history::VisitRow v1;
  v1.visit_time = base::Time::Now() - base::Hours(1);
  v1.transition = ui::PAGE_TRANSITION_TYPED;
  visits.push_back(v1);

  history::VisitRow v2;
  v2.visit_time = base::Time::Now() - base::Hours(2);
  v2.transition = ui::PAGE_TRANSITION_LINK;
  visits.push_back(v2);

  data_->UpdateRecentVisits(1, visits);

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  const VisitInfoVector& stored = it->second.visits;
  ASSERT_EQ(2u, stored.size());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      stored[0].second, ui::PAGE_TRANSITION_TYPED));
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      stored[1].second, ui::PAGE_TRANSITION_LINK));
}

TEST_F(URLIndexPrivateDataTest, UpdateRecentVisitsIgnoresMissingURL) {
  // Updating visits for a non-existent URL ID should not crash or add entries.
  history::VisitVector visits;
  history::VisitRow v;
  v.visit_time = base::Time::Now();
  v.transition = ui::PAGE_TRANSITION_TYPED;
  visits.push_back(v);

  data_->UpdateRecentVisits(999, visits);
  EXPECT_TRUE(data_->Empty());
}

TEST_F(URLIndexPrivateDataTest, UpdateRecentVisitsCapsAtMax) {
  history::URLRow row = MakeRow("http://example.com/cap", 1);
  ASSERT_TRUE(IndexRow(row));

  history::VisitVector visits;
  for (size_t i = 0; i < URLIndexPrivateData::kMaxVisitsToStoreInCache + 5;
       ++i) {
    history::VisitRow v;
    v.visit_time = base::Time::Now() - base::Hours(static_cast<int>(i));
    v.transition = ui::PAGE_TRANSITION_TYPED;
    visits.push_back(v);
  }

  data_->UpdateRecentVisits(1, visits);

  auto it = history_info_map().find(1);
  ASSERT_NE(it, history_info_map().end());
  EXPECT_EQ(URLIndexPrivateData::kMaxVisitsToStoreInCache,
            it->second.visits.size());
}

// --- Duplicate ---

TEST_F(URLIndexPrivateDataTest, DuplicateCopiesAllIndexData) {
  history::URLRow row1 = MakeRow("http://example.com/one", 1);
  row1.set_title(u"First Page");
  ASSERT_TRUE(IndexRow(row1));

  history::URLRow row2 = MakeRow("http://other.com/two", 2);
  row2.set_title(u"Second Page");
  ASSERT_TRUE(IndexRow(row2));

  scoped_refptr<URLIndexPrivateData> copy = data_->Duplicate();
  ExpectIndexDataEqual(*data_, *copy);
}

// --- Word indexing with duplicates ---

TEST_F(URLIndexPrivateDataTest, DuplicateWordsInURLAndTitle) {
  // When the same word appears in both URL and title, the word should only
  // appear once in word_map_, but the word_id_history_map_ should correctly
  // link the word to the history entry.
  history::URLRow row = MakeRow("http://google.com/search", 1);
  row.set_title(u"Google Search Engine");
  ASSERT_TRUE(IndexRow(row));

  // "google" appears in both URL and title (lowercased). It should have
  // exactly one entry in word_map_.
  EXPECT_EQ(1u, word_map().count(u"google"));

  auto word_it = word_map().find(u"google");
  ASSERT_NE(word_it, word_map().end());
  WordID google_word_id = word_it->second;

  auto hist_it = word_id_history_map().find(google_word_id);
  ASSERT_NE(hist_it, word_id_history_map().end());
  EXPECT_EQ(1u, hist_it->second.count(1));
}

TEST_F(URLIndexPrivateDataTest, RepeatedCharactersInWord) {
  // Words with repeated characters (e.g., "google" has two 'g's and two 'o's)
  // should have each unique character map to the word_id exactly once.
  history::URLRow row = MakeRow("http://google.com/", 1);
  ASSERT_TRUE(IndexRow(row));

  auto word_it = word_map().find(u"google");
  ASSERT_NE(word_it, word_map().end());
  WordID google_word_id = word_it->second;

  // 'g' should map to a set containing google_word_id (once, not twice).
  auto g_it = char_word_map().find(u'g');
  ASSERT_NE(g_it, char_word_map().end());
  EXPECT_EQ(1u, g_it->second.count(google_word_id));

  // 'o' similarly.
  auto o_it = char_word_map().find(u'o');
  ASSERT_NE(o_it, char_word_map().end());
  EXPECT_EQ(1u, o_it->second.count(google_word_id));
}

// --- Cache invalidation ---

TEST_F(URLIndexPrivateDataTest, AddRowWordsToIndexSkipsCacheClearWhenEmpty) {
  EXPECT_TRUE(search_term_cache().empty());

  history::URLRow row = MakeRow("http://example.com/", 1);
  ASSERT_TRUE(IndexRow(row));

  EXPECT_TRUE(search_term_cache().empty());
}

TEST_F(URLIndexPrivateDataTest, AddRowWordsToIndexClearsCacheWhenNonEmpty) {
  // Populate the cache with a fake entry.
  InsertCacheEntry(u"test");
  EXPECT_FALSE(search_term_cache().empty());

  history::URLRow row = MakeRow("http://example.com/", 1);
  ASSERT_TRUE(IndexRow(row));

  // The cache should now be cleared.
  EXPECT_TRUE(search_term_cache().empty());
}

// --- GetTermsAndWordStartsOffsets ---

TEST_F(URLIndexPrivateDataTest, GetTermsAndWordStartsOffsets_Basic) {
  auto [terms, offsets] =
      URLIndexPrivateData::GetTermsAndWordStartsOffsets(u"hello world");
  ASSERT_EQ(2u, terms.size());
  EXPECT_EQ(u"hello", terms[0]);
  EXPECT_EQ(u"world", terms[1]);
  ASSERT_EQ(2u, offsets.size());
  EXPECT_EQ(0u, offsets[0]);
  EXPECT_EQ(0u, offsets[1]);
}

TEST_F(URLIndexPrivateDataTest, GetTermsAndWordStartsOffsets_Empty) {
  auto [terms, offsets] =
      URLIndexPrivateData::GetTermsAndWordStartsOffsets(u"");
  EXPECT_TRUE(terms.empty());
  EXPECT_TRUE(offsets.empty());
}

TEST_F(URLIndexPrivateDataTest,
       GetTermsAndWordStartsOffsets_LeadingPunctuation) {
  auto [terms, offsets] =
      URLIndexPrivateData::GetTermsAndWordStartsOffsets(u".net framework");
  ASSERT_EQ(2u, terms.size());
  EXPECT_EQ(u".net", terms[0]);
  EXPECT_EQ(u"framework", terms[1]);
  ASSERT_EQ(2u, offsets.size());
  // ".net" has a word start at offset 1 (past the dot).
  EXPECT_EQ(1u, offsets[0]);
  EXPECT_EQ(0u, offsets[1]);
}

// --- EstimateMemoryUsage ---

TEST_F(URLIndexPrivateDataTest, EstimateMemoryUsageIsNonZeroAfterIndexing) {
  history::URLRow row = MakeRow("http://example.com/memory", 1);
  ASSERT_TRUE(IndexRow(row));

  EXPECT_GT(data_->EstimateMemoryUsage(), 0u);
}

// --- Multiple rows ---

TEST_F(URLIndexPrivateDataTest, IndexMultipleRows) {
  ASSERT_TRUE(IndexRow(MakeRow("http://alpha.com/", 1)));
  ASSERT_TRUE(IndexRow(MakeRow("http://beta.com/", 2)));
  ASSERT_TRUE(IndexRow(MakeRow("http://gamma.com/", 3)));

  EXPECT_EQ(3u, history_info_map().size());

  // Delete one and verify the rest remain.
  EXPECT_TRUE(data_->DeleteURL(GURL("http://beta.com/")));
  EXPECT_EQ(2u, history_info_map().size());
  EXPECT_NE(history_info_map().end(), history_info_map().find(1));
  EXPECT_EQ(history_info_map().end(), history_info_map().find(2));
  EXPECT_NE(history_info_map().end(), history_info_map().find(3));
}

// --- Word starts ---

TEST_F(URLIndexPrivateDataTest, WordStartsArePopulated) {
  history::URLRow row = MakeRow("http://example.com/page", 1);
  row.set_title(u"My Example Page");
  ASSERT_TRUE(IndexRow(row));

  auto it = word_starts_map().find(1);
  ASSERT_NE(it, word_starts_map().end());
  EXPECT_FALSE(it->second.url_word_starts_.empty());
  EXPECT_FALSE(it->second.title_word_starts_.empty());
}
