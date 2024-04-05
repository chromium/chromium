// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_

#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/keyed_service/core/keyed_service.h"

namespace history_embeddings {

struct UrlPassages {
  UrlPassages(history::URLID url_id,
              history::VisitID visit_id,
              base::Time visit_time);
  ~UrlPassages();
  UrlPassages(const UrlPassages&);
  UrlPassages& operator=(const UrlPassages&);
  UrlPassages(UrlPassages&&);
  UrlPassages& operator=(UrlPassages&&);

  history::URLID url_id;
  history::VisitID visit_id;
  base::Time visit_time;
  proto::PassagesValue passages;
};

class Embedding {
 public:
  explicit Embedding(std::vector<float> data);
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
  float ScoreWith(const Embedding& other) const;

  // Const accessor used for storage.
  const std::vector<float>& GetData() const { return data_; }

 private:
  std::vector<float> data_;
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
  UrlEmbeddings& operator=(UrlEmbeddings&);
  bool operator==(const UrlEmbeddings&) const;

  // Finds score of embedding nearest to query, and also outputs its index.
  std::pair<float, size_t> BestScoreWith(const Embedding& query) const;

  history::URLID url_id;
  history::VisitID visit_id;
  base::Time visit_time;
  std::vector<Embedding> embeddings;
};

struct ScoredUrl {
  // Basic data about the found URL/visit.
  history::URLID url_id;
  history::VisitID visit_id;
  base::Time visit_time;

  // A measure of how closely the query matched the found data.
  float score;

  // Index of the embedding, which also corresponds to the index of the source
  // passage used to compute the embedding.
  size_t index;

  // Source passage; may not be populated during search, but kept in this
  // struct for convenience when passing finished results to service callers.
  std::string passage;
};

// This base class decouples storage classes and inverts the dependency so that
// a vector database can work with a SQLite database, simple in-memory storage,
// flat files, or whatever kinds of storage will work efficiently.
class VectorDatabase {
 public:
  struct EmbeddingsIterator {
    virtual ~EmbeddingsIterator() = default;

    // Returns nullptr if none remain; otherwise advances the iterator
    // and returns a pointer to the next instance (which may be owned
    // by the iterator itself).
    virtual const UrlEmbeddings* Next() = 0;
  };

  virtual ~VectorDatabase() = default;

  // Returns the expected number of dimensions for an embedding.
  virtual size_t GetEmbeddingDimensions() const = 0;

  // Insert or update all embeddings for a URL's full set of passages.
  // Returns true on success.
  virtual bool AddUrlEmbeddings(const UrlEmbeddings& url_embeddings) = 0;

  // Create an iterator that steps through database items.
  // Null may be returned if there are none.
  virtual std::unique_ptr<EmbeddingsIterator> MakeEmbeddingsIterator() = 0;

  // Searches the database for embeddings near given `query` and returns
  // information about where they were found and how nearly the query matched.
  std::vector<ScoredUrl> FindNearest(
      size_t count,
      const Embedding& query,
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
  bool AddUrlEmbeddings(const UrlEmbeddings& url_embeddings) override;
  std::unique_ptr<EmbeddingsIterator> MakeEmbeddingsIterator() override;

 private:
  std::vector<UrlEmbeddings> data_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
