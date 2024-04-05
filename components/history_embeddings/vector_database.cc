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

////////////////////////////////////////////////////////////////////////////////

UrlPassages::UrlPassages(history::URLID url_id,
                         history::VisitID visit_id,
                         base::Time visit_time)
    : url_id(url_id), visit_id(visit_id), visit_time(visit_time) {}
UrlPassages::~UrlPassages() = default;
UrlPassages::UrlPassages(const UrlPassages&) = default;
UrlPassages& UrlPassages::operator=(const UrlPassages&) = default;
UrlPassages::UrlPassages(UrlPassages&&) = default;
UrlPassages& UrlPassages::operator=(UrlPassages&&) = default;

////////////////////////////////////////////////////////////////////////////////

Embedding::Embedding(std::vector<float> data) : data_(std::move(data)) {}
Embedding::~Embedding() = default;
Embedding::Embedding(const Embedding&) = default;
Embedding& Embedding::operator=(const Embedding&) = default;
Embedding::Embedding(Embedding&&) = default;
Embedding& Embedding::operator=(Embedding&&) = default;
bool Embedding::operator==(const Embedding&) const = default;

size_t Embedding::Dimensions() const {
  return data_.size();
}

float Embedding::Magnitude() const {
  float sum = 0.0f;
  for (float s : data_) {
    sum += s * s;
  }
  return std::sqrt(sum);
}

void Embedding::Normalize() {
  float magnitude = Magnitude();
  CHECK_GT(magnitude, kEpsilon);
  for (float& s : data_) {
    s /= magnitude;
  }
}

float Embedding::ScoreWith(const Embedding& other) const {
  float score = 0.0f;
  for (size_t i = 0; i < data_.size(); i++) {
    score += data_[i] * other.data_[i];
  }
  return score;
}

////////////////////////////////////////////////////////////////////////////////

UrlEmbeddings::UrlEmbeddings() : url_id(0), visit_id(0) {}
UrlEmbeddings::UrlEmbeddings(history::URLID url_id,
                             history::VisitID visit_id,
                             base::Time visit_time)
    : url_id(url_id), visit_id(visit_id), visit_time(visit_time) {}
UrlEmbeddings::UrlEmbeddings(const UrlPassages& url_passages)
    : UrlEmbeddings(url_passages.url_id,
                    url_passages.visit_id,
                    url_passages.visit_time) {}
UrlEmbeddings::~UrlEmbeddings() = default;
UrlEmbeddings::UrlEmbeddings(UrlEmbeddings&&) = default;
UrlEmbeddings& UrlEmbeddings::operator=(UrlEmbeddings&&) = default;
UrlEmbeddings::UrlEmbeddings(const UrlEmbeddings&) = default;
UrlEmbeddings& UrlEmbeddings::operator=(UrlEmbeddings&) = default;
bool UrlEmbeddings::operator==(const UrlEmbeddings&) const = default;

std::pair<float, size_t> UrlEmbeddings::BestScoreWith(
    const Embedding& query) const {
  size_t index = 0;
  float best = std::numeric_limits<float>::min();
  for (size_t i = 0; i < embeddings.size(); i++) {
    float score = query.ScoreWith(embeddings[i]);
    if (score > best) {
      best = score;
      index = i;
    }
  }
  return {best, index};
}

////////////////////////////////////////////////////////////////////////////////

std::vector<ScoredUrl> VectorDatabase::FindNearest(
    size_t count,
    const Embedding& query,
    base::RepeatingCallback<bool()> is_search_halted) {
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
    if (is_search_halted.Run()) {
      break;
    }
    while (q.size() > count) {
      q.pop();
    }
    const auto [score, score_index] = item->BestScoreWith(query);
    q.push(ScoredUrl{
        .url_id = item->url_id,
        .visit_id = item->visit_id,
        .visit_time = item->visit_time,
        .score = score,
        .index = score_index,
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

bool VectorDatabaseInMemory::AddUrlEmbeddings(
    const UrlEmbeddings& url_embeddings) {
  if (!data_.empty()) {
    for (const Embedding& embedding : url_embeddings.embeddings) {
      // All embeddings in the database must have equal dimensions.
      CHECK_EQ(embedding.Dimensions(), data_[0].embeddings[0].Dimensions());
      // All embeddings in the database are expected to be normalized.
      CHECK_LT(std::abs(embedding.Magnitude() - kUnitLength), kEpsilon);
    }
  }

  data_.push_back(url_embeddings);
  return true;
}

std::unique_ptr<VectorDatabase::EmbeddingsIterator>
VectorDatabaseInMemory::MakeEmbeddingsIterator() {
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
