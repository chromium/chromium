// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <queue>

namespace history_embeddings {

namespace {}  // namespace

Embedding::Embedding(std::vector<float> data) : data(std::move(data)) {}
Embedding::~Embedding() = default;
Embedding::Embedding(Embedding&&) = default;

float Embedding::Magnitude() const {
  float sum = 0.0f;
  for (float s : data) {
    sum += s * s;
  }
  return std::sqrt(sum);
}

void Embedding::Normalize() {
  float magnitude = Magnitude();
  for (float& s : data) {
    s /= magnitude;
  }
}

float Embedding::ScoreWith(const Embedding& other) const {
  // Cardinality is always equal.
  DCHECK_EQ(data.size(), other.data.size());

  // Magnitudes are also assumed equal; they are provided normalized by design.
  DCHECK_LT(std::abs(Magnitude() - other.Magnitude()), 0.01f);

  float score = 0.0f;
  for (size_t i = 0; i < data.size(); i++) {
    score += data[i] * other.data[i];
  }
  return score;
}

UrlEmbeddings::UrlEmbeddings() = default;
UrlEmbeddings::~UrlEmbeddings() = default;
UrlEmbeddings::UrlEmbeddings(UrlEmbeddings&&) = default;

float UrlEmbeddings::BestScoreWith(const Embedding& query) const {
  float best = std::numeric_limits<float>::min();
  for (const Embedding& embedding : embeddings) {
    best = std::max(best, query.ScoreWith(embedding));
  }
  return best;
}

VectorDatabase::VectorDatabase() = default;
VectorDatabase::~VectorDatabase() = default;

void VectorDatabase::Add(UrlEmbeddings url_embeddings) {
  data_.push_back(std::move(url_embeddings));
}

std::vector<ScoredUrl> VectorDatabase::FindNearest(size_t count,
                                                   const Embedding& query) {
  // TODO(orinj): Manage a heap for speed.
  struct Compare {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.score < b.score;
    }
  };
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, Compare> q;

  for (const UrlEmbeddings& item : data_) {
    while (q.size() > count) {
      q.pop();
    }
    q.push(ScoredUrl{
        .url = item.url,
        .score = item.BestScoreWith(query),
    });
  }

  // Empty queue into vector and return result.
  std::vector<ScoredUrl> nearest;
  while (!q.empty()) {
    nearest.push_back(q.top());
    q.pop();
  }
  return nearest;
}

}  // namespace history_embeddings
