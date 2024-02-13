// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_

#include "components/keyed_service/core/keyed_service.h"

#include <vector>

#include "url/gurl.h"

namespace history_embeddings {

class Embedding {
 public:
  Embedding(std::vector<float> data);
  ~Embedding();
  Embedding(Embedding&&);
  Embedding(const Embedding&) = delete;

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

class VectorDatabase {
 public:
  VectorDatabase();
  ~VectorDatabase();

  void Add(UrlEmbeddings url_embeddings);
  std::vector<ScoredUrl> FindNearest(size_t count, const Embedding& query);

 private:
  std::vector<UrlEmbeddings> data_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_VECTOR_DATABASE_H_
