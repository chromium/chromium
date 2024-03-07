// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_

#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace history_embeddings {

class Embedding {
 public:
  explicit Embedding(std::vector<float> data);
  ~Embedding();
  Embedding(Embedding&&);
  Embedding(const Embedding&);

  // The number of elements in the data vector.
  size_t Dimensions() const;

  // The length of the vector.
  float Magnitude() const;

  // Scale the vector to unit length.
  void Normalize();

  // Compares one embedding with another and returns a similarity measure.
  float ScoreWith(const Embedding& other) const;

 private:
  std::vector<float> data;
};

struct UrlEmbeddings {
  UrlEmbeddings();
  ~UrlEmbeddings();
  UrlEmbeddings(UrlEmbeddings&&);
  UrlEmbeddings(const UrlEmbeddings&) = delete;

  float BestScoreWith(const Embedding& query) const;

  GURL url;
  std::vector<Embedding> embeddings;
};

struct ScoredUrl {
  GURL url;
  float score;
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
  virtual void AddUrlEmbeddings(UrlEmbeddings url_embeddings) = 0;

  // Create an iterator that steps through database items.
  // Null may be returned if there are none.
  virtual std::unique_ptr<EmbeddingsIterator> MakeEmbeddingsIterator()
      const = 0;

  // Searches the database for embeddings near given `query` and returns
  // information about where they were found and how nearly the query matched.
  std::vector<ScoredUrl> FindNearest(size_t count, const Embedding& query);
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
  void AddUrlEmbeddings(UrlEmbeddings url_embeddings) override;
  std::unique_ptr<EmbeddingsIterator> MakeEmbeddingsIterator() const override;

 private:
  std::vector<UrlEmbeddings> data_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
