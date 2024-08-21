// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <queue>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "components/history_embeddings/history_embeddings_features.h"

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
bool UrlPassages::operator==(const UrlPassages& other) const {
  if (other.url_id == url_id && other.visit_id == visit_id &&
      other.visit_time == visit_time) {
    std::string a, b;
    if (other.passages.SerializeToString(&a) &&
        passages.SerializeToString(&b)) {
      return a == b;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////

Embedding::Embedding(std::vector<float> data) : data_(std::move(data)) {}
Embedding::Embedding() = default;
Embedding::Embedding(std::vector<float> data, size_t passage_word_count)
    : Embedding(data) {
  passage_word_count_ = passage_word_count;
}
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

float Embedding::ScoreWith(SearchInfo& search_info,
                           const std::string& other_passage,
                           const Embedding& other_embedding) const {
  // This check is redundant since the database layers ensure embeddings
  // always have a fixed consistent size, but code can change with time,
  // and being sure directly before use may eventually catch a bug.
  CHECK_EQ(data_.size(), other_embedding.data_.size());

  float score = 0.0f;
  // Skip non-ASCII strings to avoid scoring problems with the model.
  if (base::IsStringASCII(other_passage)) {
    for (size_t i = 0; i < data_.size(); i++) {
      score += data_[i] * other_embedding.data_[i];
    }
  } else {
    search_info.skipped_nonascii_passage_count++;
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
UrlEmbeddings& UrlEmbeddings::operator=(const UrlEmbeddings&) = default;
bool UrlEmbeddings::operator==(const UrlEmbeddings&) const = default;

float UrlEmbeddings::BestScoreWith(SearchInfo& search_info,
                                   const Embedding& query,
                                   const proto::PassagesValue& passages,
                                   size_t search_minimum_word_count) const {
  float best = std::numeric_limits<float>::min();
  for (size_t i = 0; i < embeddings.size(); i++) {
    const Embedding& embedding = embeddings[i];
    float score =
        embedding.GetPassageWordCount() < search_minimum_word_count
            ? 0.0f
            : query.ScoreWith(search_info, passages.passages(i), embedding);
    if (score > best) {
      best = score;
    }
  }
  return best;
}

////////////////////////////////////////////////////////////////////////////////

ScoredUrl::ScoredUrl(history::URLID url_id,
                     history::VisitID visit_id,
                     base::Time visit_time,
                     float score)
    : url_id(url_id),
      visit_id(visit_id),
      visit_time(visit_time),
      score(score) {}
ScoredUrl::~ScoredUrl() = default;
ScoredUrl::ScoredUrl(ScoredUrl&&) = default;
ScoredUrl& ScoredUrl::operator=(ScoredUrl&&) = default;
ScoredUrl::ScoredUrl(const ScoredUrl&) = default;
ScoredUrl& ScoredUrl::operator=(const ScoredUrl&) = default;

////////////////////////////////////////////////////////////////////////////////

SearchInfo::SearchInfo() = default;
SearchInfo::SearchInfo(SearchInfo&&) = default;
SearchInfo::~SearchInfo() = default;

////////////////////////////////////////////////////////////////////////////////

UrlPassagesEmbeddings::UrlPassagesEmbeddings(history::URLID url_id,
                                             history::VisitID visit_id,
                                             base::Time visit_time)
    : url_passages(url_id, visit_id, visit_time),
      url_embeddings(url_id, visit_id, visit_time) {}
UrlPassagesEmbeddings::UrlPassagesEmbeddings(const UrlPassagesEmbeddings&) =
    default;
UrlPassagesEmbeddings& UrlPassagesEmbeddings::operator=(
    const UrlPassagesEmbeddings&) = default;
bool UrlPassagesEmbeddings::operator==(const UrlPassagesEmbeddings&) const =
    default;

////////////////////////////////////////////////////////////////////////////////

SearchInfo VectorDatabase::FindNearest(
    std::optional<base::Time> time_range_start,
    size_t count,
    const Embedding& query,
    base::RepeatingCallback<bool()> is_search_halted) {
  if (count == 0) {
    return {};
  }

  std::unique_ptr<UrlDataIterator> iterator =
      MakeUrlDataIterator(time_range_start);
  if (!iterator) {
    return {};
  }

  // Dimensions are always equal.
  CHECK_EQ(query.Dimensions(), GetEmbeddingDimensions());

  // Magnitudes are also assumed equal; they are provided normalized by design.
  CHECK_LT(std::abs(query.Magnitude() - kUnitLength), kEpsilon);

  // Embeddings must have source passages with at least this many words in order
  // to be considered during the search. Insufficient word count embeddings
  // will score zero against the query.
  size_t search_minimum_word_count = kSearchPassageMinimumWordCount.Get();

  struct Compare {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.score > b.score;
    }
  };
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, Compare> q;

  SearchInfo search_info;
  search_info.completed = true;
  base::ElapsedTimer total_timer;
  base::TimeDelta scoring_elapsed;
  while (const UrlPassagesEmbeddings* url_data = iterator->Next()) {
    const UrlEmbeddings& item = url_data->url_embeddings;
    if (is_search_halted.Run()) {
      search_info.completed = false;
      break;
    }
    search_info.searched_url_count++;
    search_info.searched_embedding_count += item.embeddings.size();

    base::ElapsedTimer scoring_timer;
    const float score =
        item.BestScoreWith(search_info, query, url_data->url_passages.passages,
                           search_minimum_word_count);
    q.emplace(item.url_id, item.visit_id, item.visit_time, score);
    while (q.size() > count) {
      q.pop();
    }

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

  // Empty queue into vector and return result sorted with descending scores.
  while (!q.empty()) {
    search_info.scored_urls.push_back(q.top());
    q.pop();
  }
  base::ranges::reverse(search_info.scored_urls);
  return search_info;
}

////////////////////////////////////////////////////////////////////////////////

VectorDatabaseInMemory::VectorDatabaseInMemory() = default;
VectorDatabaseInMemory::~VectorDatabaseInMemory() = default;

void VectorDatabaseInMemory::SaveTo(VectorDatabase* database) {
  for (UrlPassagesEmbeddings& url_data : data_) {
    database->AddUrlData(std::move(url_data));
  }
  data_.clear();
}

size_t VectorDatabaseInMemory::GetEmbeddingDimensions() const {
  return data_.empty() ? 0 : data_[0].url_embeddings.embeddings[0].Dimensions();
}

bool VectorDatabaseInMemory::AddUrlData(UrlPassagesEmbeddings url_data) {
  CHECK_EQ(static_cast<size_t>(url_data.url_passages.passages.passages_size()),
           url_data.url_embeddings.embeddings.size());
  if (!data_.empty()) {
    for (const Embedding& embedding : url_data.url_embeddings.embeddings) {
      // All embeddings in the database must have equal dimensions.
      CHECK_EQ(embedding.Dimensions(),
               data_[0].url_embeddings.embeddings[0].Dimensions());
      // All embeddings in the database are expected to be normalized.
      CHECK_LT(std::abs(embedding.Magnitude() - kUnitLength), kEpsilon);
    }
  }

  data_.push_back(std::move(url_data));
  return true;
}

std::unique_ptr<VectorDatabase::UrlDataIterator>
VectorDatabaseInMemory::MakeUrlDataIterator(
    std::optional<base::Time> time_range_start) {
  struct SimpleIterator : public UrlDataIterator {
    explicit SimpleIterator(const std::vector<UrlPassagesEmbeddings>& source,
                            std::optional<base::Time> time_range_start)
        : iterator_(source.cbegin()),
          end_(source.cend()),
          time_range_start_(time_range_start) {}
    ~SimpleIterator() override = default;

    const UrlPassagesEmbeddings* Next() override {
      if (time_range_start_.has_value()) {
        while (iterator_ != end_) {
          if (iterator_->url_embeddings.visit_time >=
              time_range_start_.value()) {
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

    std::vector<UrlPassagesEmbeddings>::const_iterator iterator_;
    std::vector<UrlPassagesEmbeddings>::const_iterator end_;
    const std::optional<base::Time> time_range_start_;
  };

  if (data_.empty()) {
    return nullptr;
  }

  return std::make_unique<SimpleIterator>(data_, time_range_start);
}

}  // namespace history_embeddings
