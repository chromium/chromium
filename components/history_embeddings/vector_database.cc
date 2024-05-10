// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <queue>

#include "base/timer/elapsed_timer.h"

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
Embedding::Embedding() = default;
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

ScoredUrl::ScoredUrl(history::URLID url_id,
                     history::VisitID visit_id,
                     base::Time visit_time,
                     float score,
                     size_t index,
                     Embedding passage_embedding)
    : url_id(url_id),
      visit_id(visit_id),
      visit_time(visit_time),
      score(score),
      index(index),
      passage_embedding(std::move(passage_embedding)) {}
ScoredUrl::~ScoredUrl() = default;
ScoredUrl::ScoredUrl(ScoredUrl&&) = default;
ScoredUrl& ScoredUrl::operator=(ScoredUrl&&) = default;
ScoredUrl::ScoredUrl(const ScoredUrl&) = default;
ScoredUrl& ScoredUrl::operator=(ScoredUrl&) = default;

////////////////////////////////////////////////////////////////////////////////

SearchInfo::SearchInfo() = default;
SearchInfo::SearchInfo(SearchInfo&&) = default;
SearchInfo::~SearchInfo() = default;

////////////////////////////////////////////////////////////////////////////////

SearchInfo VectorDatabase::FindNearest(
    std::optional<base::Time> time_range_start,
    size_t count,
    const Embedding& query,
    base::RepeatingCallback<bool()> is_search_halted) {
  if (count == 0) {
    return {};
  }

  std::unique_ptr<EmbeddingsIterator> iterator =
      MakeEmbeddingsIterator(time_range_start);
  if (!iterator) {
    return {};
  }

  // Dimensions are always equal.
  CHECK_EQ(query.Dimensions(), GetEmbeddingDimensions());

  // Magnitudes are also assumed equal; they are provided normalized by design.
  CHECK_LT(std::abs(query.Magnitude() - kUnitLength), kEpsilon);

  struct Compare {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.score < b.score;
    }
  };
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, Compare> q;

  SearchInfo search_info;
  search_info.completed = true;

  base::ElapsedTimer total_timer;
  base::TimeDelta scoring_elapsed;
  while (const UrlEmbeddings* item = iterator->Next()) {
    if (is_search_halted.Run()) {
      search_info.completed = false;
      break;
    }
    search_info.searched_url_count++;
    search_info.searched_embedding_count += item->embeddings.size();

    base::ElapsedTimer scoring_timer;
    while (q.size() > count) {
      q.pop();
    }
    const auto [score, score_index] = item->BestScoreWith(query);
    q.emplace(item->url_id, item->visit_id, item->visit_time, score,
              score_index, std::move(item->embeddings[score_index]));
    scoring_elapsed += scoring_timer.Elapsed();
  }

  base::TimeDelta total_elapsed = total_timer.Elapsed();
  if (total_elapsed.is_zero()) {
    // Note, base::Nanoseconds(1) is still treated as zero by the time code,
    // so at least milliseconds are required here.
    scoring_elapsed = base::Milliseconds(0);
    total_elapsed = base::Milliseconds(1);
  }
  VLOG(1) << "Inner search total (ns): " << total_elapsed.InNanoseconds()
          << " ; scoring (ns): " << scoring_elapsed.InNanoseconds()
          << " ; scoring %: " << scoring_elapsed * 100 / total_elapsed;

  // Empty queue into vector and return result.
  while (!q.empty()) {
    search_info.scored_urls.push_back(q.top());
    q.pop();
  }
  return search_info;
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
VectorDatabaseInMemory::MakeEmbeddingsIterator(
    std::optional<base::Time> time_range_start) {
  struct SimpleEmbeddingsIterator : public EmbeddingsIterator {
    explicit SimpleEmbeddingsIterator(
        const std::vector<UrlEmbeddings>& source,
        std::optional<base::Time> time_range_start)
        : iterator_(source.cbegin()),
          end_(source.cend()),
          time_range_start_(time_range_start) {}
    ~SimpleEmbeddingsIterator() override = default;

    const UrlEmbeddings* Next() override {
      if (time_range_start_.has_value()) {
        while (iterator_ != end_) {
          if (iterator_->visit_time >= time_range_start_.value()) {
            break;
          }
          iterator_++;
        }
      }

      if (iterator_ == end_) {
        return nullptr;
      }
      return &(*iterator_++);
    }

    std::vector<UrlEmbeddings>::const_iterator iterator_;
    std::vector<UrlEmbeddings>::const_iterator end_;
    const std::optional<base::Time> time_range_start_;
  };

  if (data_.empty()) {
    return nullptr;
  }

  return std::make_unique<SimpleEmbeddingsIterator>(data_, time_range_start);
}

}  // namespace history_embeddings
