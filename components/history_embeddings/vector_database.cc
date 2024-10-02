// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <queue>

#include "base/ranges/algorithm.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "components/history_embeddings/history_embeddings_features.h"

namespace history_embeddings {

// Standard normalized magnitude for all embeddings.
constexpr float kUnitLength = 1.0f;

// Close enough to be considered near zero.
constexpr float kEpsilon = 0.01f;

namespace {

// Increases occurrence counts for each element of `query_terms` as they are
// found in `passage`, ranging from zero up to `max_count` inclusive. The
// `term_counts` vector is modified while counting, corresponding 1:1 with the
// terms, so its size must exactly match that of `query_terms`. Each term is
// already-folded ASCII, and `passage` is pure ASCII, so it can be folded
// efficiently during search.  Note: This can be simplified to gain performance
// boost if we do text cleaning and folding of passages in advance.
void CountTermsInPassage(std::vector<size_t>& term_counts,
                         const std::vector<std::string>& query_terms,
                         std::string_view passage,
                         const size_t max_count) {
  DCHECK_EQ(term_counts.size(), query_terms.size());
  DCHECK(base::IsStringASCII(passage));
  DCHECK(std::ranges::all_of(
      query_terms, [](std::string_view term) { return !term.empty(); }));
  DCHECK(std::ranges::all_of(query_terms, [](std::string_view term) {
    return base::IsStringASCII(term);
  }));
  DCHECK(std::ranges::all_of(query_terms, [](std::string_view term) {
    return base::ToLowerASCII(term) == term;
  }));

  base::StringViewTokenizer tokenizer(passage, ",;. ");
  while (tokenizer.GetNext()) {
    const std::string_view token = tokenizer.token();
    for (size_t term_index = 0; term_index < query_terms.size(); term_index++) {
      if (term_counts[term_index] >= max_count) {
        continue;
      }
      const std::string_view query_term = query_terms[term_index];
      if (query_term.size() != token.size()) {
        continue;
      }
      size_t char_index;
      for (char_index = 0; char_index < token.size(); char_index++) {
        if (query_term[char_index] != base::ToLowerASCII(token[char_index])) {
          break;
        }
      }
      if (char_index == token.size()) {
        term_counts[term_index]++;
      }
    }
  }
}

}  // namespace

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

float Embedding::ScoreWith(const Embedding& other_embedding) const {
  // This check is redundant since the database layers ensure embeddings
  // always have a fixed consistent size, but code can change with time,
  // and being sure directly before use may eventually catch a bug.
  CHECK_EQ(data_.size(), other_embedding.data_.size());

  float embedding_score = 0.0f;
  for (size_t i = 0; i < data_.size(); i++) {
    embedding_score += data_[i] * other_embedding.data_[i];
  }
  return embedding_score;
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
                                   const SearchParams& search_params,
                                   const Embedding& query_embedding,
                                   const proto::PassagesValue& passages,
                                   size_t min_passage_word_count) const {
  std::vector<size_t> term_counts(search_params.query_terms.size(), 0);
  float best = 0.0f;
  for (size_t i = 0; i < embeddings.size(); i++) {
    const Embedding& embedding = embeddings[i];
    const std::string& passage = passages.passages(i);

    // Skip non-ASCII strings to avoid scoring problems with the model.
    if (!base::IsStringASCII(passage)) {
      search_info.skipped_nonascii_passage_count++;
      continue;
    }

    float score = embedding.GetPassageWordCount() < min_passage_word_count
                      ? 0.0f
                      : query_embedding.ScoreWith(embedding);

    if (score >= search_params.word_match_minimum_embedding_score) {
      // Since the ASCII check above processed the whole passage string, it is
      // likely ready in CPU cache. Scan text again to count terms in passage.
      base::ElapsedTimer timer;
      CountTermsInPassage(term_counts, search_params.query_terms, passage,
                          search_params.word_match_limit);
      search_info.passage_scanning_time += timer.Elapsed();
    }

    best = std::max(best, score);
  }

  // Calculate total boost from term counts across all passages.
  float word_match_boost = 0.0f;
  for (size_t term_count : term_counts) {
    float term_boost = search_params.word_match_score_boost_factor *
                       term_count / search_params.word_match_limit;
    // Boost factor is applied per term such that longer queries boost more.
    word_match_boost += term_boost;
  }
  // Normalize to avoid over-boosting long queries with many words.
  word_match_boost /=
      std::max<size_t>(1, search_params.query_terms.size() +
                              search_params.word_match_smoothing_factor);

  return best + word_match_boost;
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

SearchParams::SearchParams() = default;
SearchParams::SearchParams(SearchParams&&) = default;
SearchParams::~SearchParams() = default;

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
UrlPassagesEmbeddings::UrlPassagesEmbeddings(UrlPassagesEmbeddings&&) = default;
UrlPassagesEmbeddings& UrlPassagesEmbeddings::operator=(
    const UrlPassagesEmbeddings&) = default;
UrlPassagesEmbeddings& UrlPassagesEmbeddings::operator=(
    UrlPassagesEmbeddings&&) = default;
UrlPassagesEmbeddings::~UrlPassagesEmbeddings() = default;
bool UrlPassagesEmbeddings::operator==(const UrlPassagesEmbeddings&) const =
    default;

////////////////////////////////////////////////////////////////////////////////

SearchInfo VectorDatabase::FindNearest(
    std::optional<base::Time> time_range_start,
    size_t count,
    const SearchParams& search_params,
    const Embedding& query_embedding,
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
  CHECK_EQ(query_embedding.Dimensions(), GetEmbeddingDimensions());

  // Magnitudes are also assumed equal; they are provided normalized by design.
  CHECK_LT(std::abs(query_embedding.Magnitude() - kUnitLength), kEpsilon);

  // Embeddings must have source passages with at least this many words in order
  // to be considered during the search. Insufficient word count embeddings
  // will score zero against the query_embedding.
  size_t min_passage_word_count = kSearchPassageMinimumWordCount.Get();

  struct Compare {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.score > b.score;
    }
  };
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, Compare> q;

  SearchInfo search_info;
  search_info.completed = true;
  base::ElapsedTimer total_timer;
  while (const UrlPassagesEmbeddings* url_data = iterator->Next()) {
    const UrlEmbeddings& item = url_data->url_embeddings;
    if (is_search_halted.Run()) {
      search_info.completed = false;
      break;
    }
    search_info.searched_url_count++;
    search_info.searched_embedding_count += item.embeddings.size();

    base::ElapsedTimer scoring_timer;
    const float score = item.BestScoreWith(
        search_info, search_params, query_embedding,
        url_data->url_passages.passages, min_passage_word_count);
    q.emplace(item.url_id, item.visit_id, item.visit_time, score);
    while (q.size() > count) {
      q.pop();
    }

    search_info.scoring_time += scoring_timer.Elapsed();
  }
  search_info.total_search_time = total_timer.Elapsed();

  // TODO(b/363083815): Log histograms and rework caller time histogram.
  if (search_info.total_search_time.is_zero()) {
    VLOG(1) << "Inner search total (μs): "
            << search_info.total_search_time.InMicroseconds();
  } else {
    VLOG(1) << "Inner search total (μs): "
            << search_info.total_search_time.InMicroseconds()
            << " ; scoring (μs): " << search_info.scoring_time.InMicroseconds()
            << " ; scoring %: "
            << search_info.scoring_time * 100 / search_info.total_search_time
            << " ; passage scanning (μs): "
            << search_info.passage_scanning_time.InMicroseconds()
            << " ; passage scanning %: "
            << search_info.passage_scanning_time * 100 /
                   search_info.total_search_time;
  }

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
