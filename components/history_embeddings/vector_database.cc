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
Embedding::Embedding(const Embedding&) = default;

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

////////////////////////////////////////////////////////////////////////////////

std::vector<ScoredUrl> VectorDatabase::FindNearest(size_t count,
                                                   const Embedding& query) {
  if (count == 0) {
    return {};
  }

  std::unique_ptr<EmbeddingsIterator> iterator = MakeEmbeddingsIterator();
  if (!iterator) {
    return {};
  }

  // Dimensions are always equal.
  CHECK_EQ(query.Dimensions(), GetEmbeddingDimensions());

  // Magnitudes are also assumed equal; they are provided normalized by design.
  CHECK_LT(std::abs(query.Magnitude() - kUnitLength), kEpsilon);

  // TODO(orinj): Manage a heap for speed.
  struct Compare {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.score < b.score;
    }
  };
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, Compare> q;

  while (const UrlEmbeddings* item = iterator->Next()) {
    while (q.size() > count) {
      q.pop();
    }
    q.push(ScoredUrl{
        .url = item->url,
        .score = item->BestScoreWith(query),
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

////////////////////////////////////////////////////////////////////////////////

VectorDatabaseInMemory::VectorDatabaseInMemory() = default;
VectorDatabaseInMemory::~VectorDatabaseInMemory() = default;

void VectorDatabaseInMemory::SaveTo(VectorDatabase* database) {
  for (UrlEmbeddings& url_embeddings : data_) {
    database->AddUrlEmbeddings(std::move(url_embeddings));
  }
  data_.clear();
}

size_t VectorDatabaseInMemory::GetEmbeddingDimensions() const {
  return data_.empty() ? 0 : data_[0].embeddings[0].Dimensions();
}

void VectorDatabaseInMemory::AddUrlEmbeddings(UrlEmbeddings url_embeddings) {
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

std::unique_ptr<VectorDatabase::EmbeddingsIterator>
VectorDatabaseInMemory::MakeEmbeddingsIterator() const {
  struct SimpleEmbeddingsIterator : public EmbeddingsIterator {
    explicit SimpleEmbeddingsIterator(const std::vector<UrlEmbeddings>& source)
        : iterator_(source.cbegin()), end_(source.cend()) {}
    ~SimpleEmbeddingsIterator() override = default;

    const UrlEmbeddings* Next() override {
      if (iterator_ == end_) {
        return nullptr;
      }
      return &(*iterator_++);
    }

    std::vector<UrlEmbeddings>::const_iterator iterator_;
    std::vector<UrlEmbeddings>::const_iterator end_;
  };

  if (data_.empty()) {
    return nullptr;
  }

  return std::make_unique<SimpleEmbeddingsIterator>(data_);
}

}  // namespace history_embeddings
