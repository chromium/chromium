// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <queue>

namespace history_embeddings {

// Standard normalized magnitude for all embeddings.
constexpr float kUnitLength = 1.0f;

// Close enough to be considered near zero.
constexpr float kEpsilon = 0.01f;

Embedding::Embedding(std::vector<float> data) : data(std::move(data)) {}
Embedding::~Embedding() = default;
Embedding::Embedding(Embedding&&) = default;

size_t Embedding::Dimensions() const {
  return data.size();
}

float Embedding::Magnitude() const {
  float sum = 0.0f;
  for (float s : data) {
    sum += s * s;
  }
  return std::sqrt(sum);
}

void Embedding::Normalize() {
  float magnitude = Magnitude();
  CHECK_GT(magnitude, kEpsilon);
  for (float& s : data) {
    s /= magnitude;
  }
}

float Embedding::ScoreWith(const Embedding& other) const {
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
  if (!data_.empty()) {
    for (const Embedding& embedding : url_embeddings.embeddings) {
      // All embeddings in the database must have equal dimensions.
      CHECK_EQ(embedding.Dimensions(), data_[0].embeddings[0].Dimensions());
      // All embeddings in the database are expected to be normalized.
      CHECK_LT(std::abs(embedding.Magnitude() - kUnitLength), kEpsilon);
    }
  }

  data_.push_back(std::move(url_embeddings));
}

std::vector<ScoredUrl> VectorDatabase::FindNearest(size_t count,
                                                   const Embedding& query) {
  if (count == 0 || data_.empty()) {
    return {};
  }

  // Dimensions are always equal.
  CHECK_EQ(query.Dimensions(), data_[0].embeddings[0].Dimensions());

  // Magnitudes are also assumed equal; they are provided normalized by design.
  CHECK_LT(std::abs(query.Magnitude() - kUnitLength), kEpsilon);

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
