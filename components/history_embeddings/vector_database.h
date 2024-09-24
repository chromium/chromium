// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace history_embeddings {

struct ScoredUrl {
  ScoredUrl(history::URLID url_id,
            history::VisitID visit_id,
            base::Time visit_time,
            float score);
  ~ScoredUrl();
  ScoredUrl(ScoredUrl&&);
  ScoredUrl& operator=(ScoredUrl&&);
  ScoredUrl(const ScoredUrl&);
  ScoredUrl& operator=(const ScoredUrl&);

  // Basic data about the found URL/visit.
  history::URLID url_id;
  history::VisitID visit_id;
  base::Time visit_time;

  // A measure of how closely the query matched the found data. This includes
  // the single best embedding score plus a word match boost from text search
  // across all passages.
  float score;
};

struct SearchParams {
  SearchParams();
  SearchParams(SearchParams&&);
  ~SearchParams();

  // Portions of lower-cased query representing terms usable for text search.
  // Owned std::string instances are used instead of std::string_view into
  // an owned query instance because this struct can move, and view data
  // pointers are not guaranteed valid after source string moves.
  std::vector<std::string> query_terms;

  // Embedding similarity score below which no word matching takes place.
  float word_match_minimum_embedding_score = 0.0f;

  // Raw score boost, applied per word.
  float word_match_score_boost_factor = 0.2f;

  // Divides and caps a word match boost. Finding the word more than this many
  // times won't increase the boost for the word.
  size_t word_match_limit = 5;

  // Used as a term in final score boost divide to normalize for long queries.
  size_t word_match_smoothing_factor = 1;
};

struct SearchInfo {
  SearchInfo();
  SearchInfo(SearchInfo&&);
  ~SearchInfo();

  // Result of the search, the best scored URLs.
  std::vector<ScoredUrl> scored_urls;

  // The number of URLs searched to find this result.
  size_t searched_url_count = 0u;

  // The number of embeddings searched to find this result.
  size_t searched_embedding_count = 0u;

  // The number of embeddings scored zero due to having a source passage
  // containing non-ASCII characters.
  size_t skipped_nonascii_passage_count = 0u;

  // Whether the search completed without interruption. Starting a new search
  // may cause a search to halt, and in that case this member will be false.
  bool completed = false;

  // Time breakdown for metrics: total > scoring > passage_scanning as each
  // succeeding time value is a portion of the last.
  base::TimeDelta total_search_time;
  base::TimeDelta scoring_time;
  base::TimeDelta passage_scanning_time;
};

struct UrlPassages {
  UrlPassages(history::URLID url_id,
              history::VisitID visit_id,
              base::Time visit_time);
  ~UrlPassages();
  UrlPassages(const UrlPassages&);
  UrlPassages& operator=(const UrlPassages&);
  UrlPassages(UrlPassages&&);
  UrlPassages& operator=(UrlPassages&&);
  bool operator==(const UrlPassages&) const;

  history::URLID url_id;
  history::VisitID visit_id;
  base::Time visit_time;
  proto::PassagesValue passages;
};

class Embedding {
 public:
  explicit Embedding(std::vector<float> data);
  Embedding(std::vector<float> data, size_t passage_word_count);
  Embedding();
  ~Embedding();
  Embedding(const Embedding&);
  Embedding& operator=(const Embedding&);
  Embedding(Embedding&&);
  Embedding& operator=(Embedding&&);
  bool operator==(const Embedding&) const;

  // The number of elements in the data vector.
  size_t Dimensions() const;

  // The length of the vector.
  float Magnitude() const;

  // Scale the vector to unit length.
  void Normalize();

  // Compares one embedding with another and returns a similarity measure.
  float ScoreWith(const Embedding& other_embedding) const;

  // Const accessor used for storage.
  const std::vector<float>& GetData() const { return data_; }

  // Used for search filtering of passages with low word count.
  size_t GetPassageWordCount() const { return passage_word_count_; }
  void SetPassageWordCount(size_t passage_word_count) {
    passage_word_count_ = passage_word_count;
  }

 private:
  std::vector<float> data_;
  size_t passage_word_count_ = 0;
};

struct UrlEmbeddings {
  UrlEmbeddings();
  UrlEmbeddings(history::URLID url_id,
                history::VisitID visit_id,
                base::Time visit_time);
  explicit UrlEmbeddings(const UrlPassages& url_passages);
  ~UrlEmbeddings();
  UrlEmbeddings(UrlEmbeddings&&);
  UrlEmbeddings& operator=(UrlEmbeddings&&);
  UrlEmbeddings(const UrlEmbeddings&);
  UrlEmbeddings& operator=(const UrlEmbeddings&);
  bool operator==(const UrlEmbeddings&) const;

  // Finds score of embedding nearest to query, also taking passages
  // into consideration since some should be skipped. The passages
  // correspond to the embeddings 1:1 by index.
  float BestScoreWith(SearchInfo& search_info,
                      const SearchParams& search_params,
                      const Embedding& query_embedding,
                      const proto::PassagesValue& passages,
                      size_t search_minimum_word_count) const;

  history::URLID url_id;
  history::VisitID visit_id;
  base::Time visit_time;
  std::vector<Embedding> embeddings;
};

struct UrlPassagesEmbeddings {
  UrlPassagesEmbeddings(history::URLID url_id,
                        history::VisitID visit_id,
                        base::Time visit_time);
  UrlPassagesEmbeddings(const UrlPassagesEmbeddings&);
  UrlPassagesEmbeddings(UrlPassagesEmbeddings&&);
  UrlPassagesEmbeddings& operator=(const UrlPassagesEmbeddings&);
  UrlPassagesEmbeddings& operator=(UrlPassagesEmbeddings&&);
  ~UrlPassagesEmbeddings();

  bool operator==(const UrlPassagesEmbeddings&) const;

  UrlPassages url_passages;
  UrlEmbeddings url_embeddings;
};

// This base class decouples storage classes and inverts the dependency so that
// a vector database can work with a SQLite database, simple in-memory storage,
// flat files, or whatever kinds of storage will work efficiently.
class VectorDatabase {
 public:
  struct UrlDataIterator {
    virtual ~UrlDataIterator() = default;

    // Returns nullptr if none remain; otherwise advances the iterator
    // and returns a pointer to the next instance (which may be owned
    // by the iterator itself).
    virtual const UrlPassagesEmbeddings* Next() = 0;
  };

  virtual ~VectorDatabase() = default;

  // Returns the expected number of dimensions for an embedding.
  virtual size_t GetEmbeddingDimensions() const = 0;

  // Insert or update all embeddings for a URL's full set of passages.
  // Returns true on success.
  virtual bool AddUrlData(UrlPassagesEmbeddings url_passages_embeddings) = 0;

  // Create an iterator that steps through database items.
  // Null may be returned if there are none.
  virtual std::unique_ptr<UrlDataIterator> MakeUrlDataIterator(
      std::optional<base::Time> time_range_start) = 0;

  // Searches the database for embeddings near given `query` and returns
  // information about where they were found and how nearly the query matched.
  SearchInfo FindNearest(std::optional<base::Time> time_range_start,
                         size_t count,
                         const SearchParams& search_params,
                         const Embedding& query_embedding,
                         base::RepeatingCallback<bool()> is_search_halted);
};

// This is an in-memory vector store that supports searching and saving to
// another persistent backing store.
class VectorDatabaseInMemory : public VectorDatabase {
 public:
  VectorDatabaseInMemory();
  ~VectorDatabaseInMemory() override;

  // Save this store's data to another given store. Most implementations don't
  // need this, but it's useful for an in-memory store to work with a separate
  // backing database on a worker sequence.
  void SaveTo(VectorDatabase* database);

  // VectorDatabase:
  size_t GetEmbeddingDimensions() const override;
  bool AddUrlData(UrlPassagesEmbeddings url_passages_embeddings) override;
  std::unique_ptr<UrlDataIterator> MakeUrlDataIterator(
      std::optional<base::Time> time_range_start) override;

 private:
  std::vector<UrlPassagesEmbeddings> data_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
