// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <atomic>
#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

Embedding RandomEmbedding() {
  constexpr size_t kSize = 768u;
  std::vector<float> random_vector(kSize, 0.0f);
  for (float& v : random_vector) {
    v = base::RandFloat();
  }
  Embedding embedding(std::move(random_vector));
  embedding.Normalize();
  return embedding;
}

Embedding DeterministicEmbedding(float value) {
  constexpr size_t kSize = 768u;
  std::vector<float> vector(kSize, 0.0f);
  vector[0] = 1;
  vector[1] = value;
  Embedding embedding(std::move(vector));
  embedding.Normalize();
  embedding.SetPassageWordCount(10);
  return embedding;
}

}  // namespace

TEST(HistoryEmbeddingsVectorDatabaseTest, Constructs) {
  std::make_unique<VectorDatabaseInMemory>();
}

TEST(HistoryEmbeddingsVectorDatabaseTest, EraseNonAsciiCharacters) {
  {
    std::string s = "passage with non-ASCII∅character";
    EraseNonAsciiCharacters(s);
    EXPECT_EQ(s, "passage with non-ASCII character");
  }
  {
    std::string s = "passage with consecutive non-ASCII    spaces";
    EraseNonAsciiCharacters(s);
    EXPECT_EQ(s, "passage with consecutive non-ASCII spaces");
  }
  {
    // Only non-ASCII spaces -> blank.
    std::string s = "    ";
    EraseNonAsciiCharacters(s);
    EXPECT_EQ(s, "");
  }
  {
    std::string s = "a    ";
    EraseNonAsciiCharacters(s);
    EXPECT_EQ(s, "a ");
  }
  {
    std::string s = "    a";
    EraseNonAsciiCharacters(s);
    EXPECT_EQ(s, "a");
  }
  {
    std::string s = "  a  ";
    EraseNonAsciiCharacters(s);
    EXPECT_EQ(s, "a ");
  }
}

TEST(HistoryEmbeddingsVectorDatabaseTest, EmbeddingOperations) {
  Embedding a({1, 1, 1});
  EXPECT_FLOAT_EQ(a.Magnitude(), std::sqrt(3));
  a.Normalize();
  EXPECT_FLOAT_EQ(a.Magnitude(), 1.0f);

  Embedding b({2, 2, 2});
  b.Normalize();
  EXPECT_FLOAT_EQ(a.ScoreWith(b), 1.0f);

  // Verify more similar embeddings have higher scores.
  EXPECT_GT(DeterministicEmbedding(5).ScoreWith(DeterministicEmbedding(4)),
            DeterministicEmbedding(5).ScoreWith(DeterministicEmbedding(3)));

  EXPECT_GT(DeterministicEmbedding(5).ScoreWith(DeterministicEmbedding(6)),
            DeterministicEmbedding(5).ScoreWith(DeterministicEmbedding(7)));
}

TEST(HistoryEmbeddingsVectorDatabaseTest, BestScoreWith) {
  SearchInfo search_info;
  SearchParams search_params;
  search_params.word_match_required_term_ratio = 0.0f;

  UrlPassagesEmbeddings url_data(1, 1, base::Time::Now());
  url_data.url_passages.passages.add_passages("some deterministic passage");
  url_data.url_passages.passages.add_passages("more text in another passage");
  url_data.url_passages.passages.add_passages(
      "some deterministic passage with non-ASCII ∅ character");
  url_data.url_embeddings.embeddings.push_back(DeterministicEmbedding(0));
  url_data.url_embeddings.embeddings.push_back(DeterministicEmbedding(1));
  url_data.url_embeddings.embeddings.push_back(DeterministicEmbedding(2));

  Embedding query_embedding = DeterministicEmbedding(0);
  float score = url_data.url_embeddings.BestScoreWith(
      search_info, search_params, query_embedding,
      url_data.url_passages.passages, 0);
  EXPECT_EQ(search_info.skipped_nonascii_passage_count, 1u);
  EXPECT_FLOAT_EQ(score, 1.0f);

  // This test checks basic properties of score boosting, for example that
  // query terms can be spread across multiple separate passages.
  // Boost scoring is tested further in FindNearestWordMatchBoosting test below.
  search_params.query_terms = {
      "some",
      "passage",
      "absent",
  };
  float boosted_score = url_data.url_embeddings.BestScoreWith(
      search_info, search_params, query_embedding,
      url_data.url_passages.passages, 0);
  EXPECT_LT(score, boosted_score);

  search_params.word_match_max_term_count = 5;
  search_params.query_terms = {
      "some", "passage", "more", "another", "absent",
  };
  float across_score = url_data.url_embeddings.BestScoreWith(
      search_info, search_params, query_embedding,
      url_data.url_passages.passages, 0);
  EXPECT_LT(boosted_score, across_score);
}

TEST(HistoryEmbeddingsVectorDatabaseTest, FindNearest) {
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 10; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, base::Time::Now());
    url_data.url_passages.passages.add_passages("some deterministic passage");
    url_data.url_embeddings.embeddings.push_back(DeterministicEmbedding(i));
    database.AddUrlData(url_data);
  }
  SearchParams search_params;
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, search_params, DeterministicEmbedding(0),
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_THAT(scored_urls,
                testing::ElementsAre(testing::Field(&ScoredUrl::url_id, 1),
                                     testing::Field(&ScoredUrl::url_id, 2),
                                     testing::Field(&ScoredUrl::url_id, 3)));
  }

  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, search_params, DeterministicEmbedding(20),
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_THAT(scored_urls,
                testing::ElementsAre(testing::Field(&ScoredUrl::url_id, 10),
                                     testing::Field(&ScoredUrl::url_id, 9),
                                     testing::Field(&ScoredUrl::url_id, 8)));
  }
}

TEST(HistoryEmbeddingsVectorDatabaseTest, FindNearestWordMatchBoosting) {
  auto no = base::BindRepeating([]() { return false; });
  VectorDatabaseInMemory database;

  UrlPassagesEmbeddings url_data1(1, 1, base::Time::Now());
  url_data1.url_passages.passages.add_passages("some deterministic passage");
  url_data1.url_embeddings.embeddings.push_back(DeterministicEmbedding(0));
  database.AddUrlData(url_data1);

  UrlPassagesEmbeddings url_data2(2, 2, base::Time::Now());
  url_data2.url_passages.passages.add_passages(
      "hello hello world world world world world");
  url_data2.url_embeddings.embeddings.push_back(DeterministicEmbedding(0));
  database.AddUrlData(url_data2);

  // Including a non-ASCII passage to demonstrate safe internal CHECKs.
  UrlPassagesEmbeddings url_data3(3, 3, base::Time::Now());
  url_data3.url_passages.passages.add_passages(
      "this is some deterministic non-ASCII passage, scores ∅, gets skipped");
  url_data3.url_embeddings.embeddings.push_back(DeterministicEmbedding(0));
  database.AddUrlData(url_data3);

  SearchParams search_params;
  search_params.word_match_minimum_embedding_score = 0.0f;
  search_params.word_match_limit = 4;
  search_params.word_match_score_boost_factor = 0.1;
  search_params.word_match_max_term_count = 8;
  search_params.word_match_required_term_ratio = 0.0f;

  // Basic embedding search with no query terms produces flat embedding score.
  Embedding query_embedding = DeterministicEmbedding(0);
  std::vector<ScoredUrl> scored_urls =
      database.FindNearest({}, 3, search_params, query_embedding, no)
          .scored_urls;
  EXPECT_EQ(scored_urls.size(), 3u);
  EXPECT_FLOAT_EQ(scored_urls[0].score, 1.0f);
  EXPECT_FLOAT_EQ(scored_urls[1].score, 1.0f);
  EXPECT_FLOAT_EQ(scored_urls[2].score, 0.0f);

  // Set up some query terms to boost score with word matches against passage.
  // Additional unmatched terms provide no boost. N occurrences of a matching
  // term will independently yield an extra (0.1 * N / 4) with N
  // capped at denominator so that each term's max boost is the boost_factor.
  // But there's an overall normalizing divide with smoothing factor, so
  // the final value will be slightly less.

  // Here we have (0.1 * 1 / 4) * 3 terms, for a total boost of 0.075.
  // Normalized by dividing by (smooth + query-terms-length)
  //  -> 0.075 / (1 + 8) = 0.008333333
  search_params.query_terms = {"some",  "deterministic", "passage", "and",
                               "other", "nonboosting",   "query",   "terms"};
  scored_urls = database.FindNearest({}, 3, search_params, query_embedding, no)
                    .scored_urls;
  EXPECT_EQ(scored_urls[0].url_id, 1);
  EXPECT_EQ(scored_urls[1].url_id, 2);
  EXPECT_EQ(scored_urls[2].url_id, 3);
  EXPECT_FLOAT_EQ(scored_urls[0].score, 1.008333333f);
  EXPECT_FLOAT_EQ(scored_urls[1].score, 1.0f);
  EXPECT_FLOAT_EQ(scored_urls[2].score, 0.0f);

  // Here we have one too many terms, so there's no boost at all.
  search_params.query_terms = {"some",  "deterministic", "passage",
                               "and",   "other",         "nonboosting",
                               "query", "terms",         "extra"};
  scored_urls = database.FindNearest({}, 3, search_params, query_embedding, no)
                    .scored_urls;
  EXPECT_EQ(scored_urls[0].url_id, 1);
  EXPECT_EQ(scored_urls[1].url_id, 2);
  EXPECT_EQ(scored_urls[2].url_id, 3);
  EXPECT_FLOAT_EQ(scored_urls[0].score, 1.0f);
  EXPECT_FLOAT_EQ(scored_urls[1].score, 1.0f);
  EXPECT_FLOAT_EQ(scored_urls[2].score, 0.0f);

  // Here we have (0.1 * 2 / 4) + (0.1 * 4 / 4) even though "world" appears 5
  // times in passage, because the occurrence count is capped by denominator.
  // And then also divided to normalize with smoothing: 0.15 / (1 + 2) = 0.05
  search_params.query_terms = {"hello", "world"};
  scored_urls = database.FindNearest({}, 3, search_params, query_embedding, no)
                    .scored_urls;
  EXPECT_EQ(scored_urls[0].url_id, 2);
  EXPECT_EQ(scored_urls[1].url_id, 1);
  EXPECT_EQ(scored_urls[2].url_id, 3);
  EXPECT_FLOAT_EQ(scored_urls[0].score, 1.05f);
  EXPECT_FLOAT_EQ(scored_urls[1].score, 1.0f);
  EXPECT_FLOAT_EQ(scored_urls[2].score, 0.0f);
}

TEST(HistoryEmbeddingsVectorDatabaseTest, SearchCanBeHaltedEarly) {
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 3; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, base::Time::Now());
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("a random passage");
      url_data.url_embeddings.embeddings.push_back(RandomEmbedding());
    }
    database.AddUrlData(url_data);
  }
  Embedding query = RandomEmbedding();
  SearchParams search_params;

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 3u);
  }

  // A halted search with fewer results:
  {
    std::atomic<size_t> counter(0u);
    base::WeakPtrFactory<std::atomic<size_t>> weak_factory(&counter);
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, search_params, query,
                         base::BindRepeating(
                             [](auto weak_counter) {
                               (*weak_counter)++;
                               return *weak_counter > 2u;
                             },
                             weak_factory.GetWeakPtr()))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 2u);
  }
}

TEST(HistoryEmbeddingsVectorDatabaseTest, TimeRangeNarrowsSearchResult) {
  const base::Time now = base::Time::Now();
  VectorDatabaseInMemory database;
  for (size_t i = 0; i < 3; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, now + base::Minutes(i));
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("some random passage");
      url_data.url_embeddings.embeddings.push_back(RandomEmbedding());
    }
    database.AddUrlData(url_data);
  }
  Embedding query = RandomEmbedding();
  SearchParams search_params;

  // An ordinary search with full results:
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest({}, 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 3u);
  }

  // Narrowed searches with time range.
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now, 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 3u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(30), 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 2u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(90), 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Minutes(2), 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 1u);
  }
  {
    std::vector<ScoredUrl> scored_urls =
        database
            .FindNearest(now + base::Seconds(121), 3, search_params, query,
                         base::BindRepeating([]() { return false; }))
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 0u);
  }
}

// Note: Disabled by default so as to not burden the bots. Enable when needed.
TEST(HistoryEmbeddingsVectorDatabaseTest, DISABLED_ManyVectorsAreFastEnough) {
  VectorDatabaseInMemory database;
  size_t count = 0;
  // Estimate for expected URL count...
  for (size_t i = 0; i < 15000; i++) {
    UrlPassagesEmbeddings url_data(i + 1, i + 1, base::Time::Now());
    // Times 3 embeddings each, on average.
    for (size_t j = 0; j < 3; j++) {
      url_data.url_passages.passages.add_passages("one of many passages");
      url_data.url_embeddings.embeddings.push_back(RandomEmbedding());
      count++;
    }
    database.AddUrlData(url_data);
  }
  Embedding query = RandomEmbedding();
  base::ElapsedTimer timer;

  // Since inner loop atomic checks can impact performance, simulate that here.
  SearchParams search_params;
  std::atomic<size_t> id(0u);
  base::WeakPtrFactory<std::atomic<size_t>> weak_factory(&id);
  database.FindNearest(
      {}, 3, search_params, query,
      base::BindRepeating(
          [](auto weak_id) { return !weak_id || *weak_id != 0u; },
          weak_factory.GetWeakPtr()));

  // This could be an assertion with an extraordinarily high threshold, but for
  // now we avoid any possibility of blowing up trybots and just need the info.
  LOG(INFO) << "Searched " << count << " embeddings in " << timer.Elapsed();
}

base::FilePath GetWordMatchBoostTestDataPath() {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  return test_data_dir.AppendASCII(
      "components/test/data/history_embeddings/word_match_boost_test_data");
}

// This is a utility test to produce a simple test data protobuf text file.  It
// shows structure and can be enabled if we need to produce a stub or extra test
// files, but the main test file should be filled manually with real test cases.
TEST(HistoryEmbeddingsVectorDatabaseTest,
     DISABLED_GenerateWordMatchBoostProtoDataTest) {
  proto::WordMatchBoostTest test;
  proto::WordMatchBoostTestCase* test_case = test.add_cases();

  auto* params = test_case->mutable_params();
  params->set_minimum_embedding_score(0.0f);
  params->set_score_boost_factor(0.2f);
  params->set_word_match_limit(5);
  params->set_smoothing_factor(1);
  params->set_max_term_count(3);
  params->set_required_term_ratio(1.0f);

  test_case->set_query("example test query");
  test_case->mutable_passages()->add_passages("this is an example passage");
  test_case->mutable_passages()->add_passages(
      "this example passage matches the test query term 'query'");
  test_case->mutable_passages()->add_passages(
      "all of this test data is for test, test, testing!");
  test_case->set_expected_score_boost(0.080000043);
  EXPECT_TRUE(base::WriteFile(GetWordMatchBoostTestDataPath(),
                              test.SerializeAsString()));
}

TEST(HistoryEmbeddingsVectorDatabaseTest, WordMatchBoostProtoDataTest) {
  extern uint32_t HashString(std::string_view str);
  auto no = base::BindRepeating([]() { return false; });
  std::string test_proto_content;
  EXPECT_TRUE(base::ReadFileToString(GetWordMatchBoostTestDataPath(),
                                     &test_proto_content));

  history_embeddings::proto::WordMatchBoostTest test;
  EXPECT_TRUE(test.ParseFromString(test_proto_content));

  std::unordered_set<uint32_t> stop_words_hashes;
  for (const std::string& stop_word : test.stop_words()) {
    stop_words_hashes.insert(HashString(stop_word));
  }

  for (const proto::WordMatchBoostTestCase& test_case : test.cases()) {
    VectorDatabaseInMemory database;
    SearchParams search_params;
    search_params.word_match_minimum_embedding_score =
        test_case.params().minimum_embedding_score();
    search_params.word_match_limit = test_case.params().word_match_limit();
    search_params.word_match_score_boost_factor =
        test_case.params().score_boost_factor();
    search_params.word_match_smoothing_factor =
        test_case.params().smoothing_factor();
    search_params.word_match_max_term_count =
        test_case.params().max_term_count();
    search_params.word_match_required_term_ratio =
        test_case.params().required_term_ratio();
    UrlPassagesEmbeddings url_data(1, 1, base::Time::Now());
    for (const std::string& passage : test_case.passages().passages()) {
      url_data.url_passages.passages.add_passages(passage);
      url_data.url_embeddings.embeddings.push_back(DeterministicEmbedding(0));
    }
    database.AddUrlData(url_data);

    // Basic embedding search with no query terms produces flat embedding score.
    Embedding query_embedding = DeterministicEmbedding(0);
    std::vector<ScoredUrl> scored_urls =
        database.FindNearest({}, 1, /*search_params=*/{}, query_embedding, no)
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 1u);
    EXPECT_FLOAT_EQ(scored_urls[0].score, 1.0f);

    // Set up some query terms to boost score with word matches against
    // passages.
    search_params.query_terms =
        SplitQueryToTerms(stop_words_hashes, test_case.query(),
                          test_case.params().minimum_term_length());
    scored_urls =
        database.FindNearest({}, 1, search_params, query_embedding, no)
            .scored_urls;
    EXPECT_EQ(scored_urls.size(), 1u);
    // Embedding score without boost is 1.0f; subtract it to determine boost.
    float word_match_boost = scored_urls[0].score - 1.0f;
    EXPECT_FLOAT_EQ(word_match_boost, test_case.expected_score_boost());
  }
}

}  // namespace history_embeddings
