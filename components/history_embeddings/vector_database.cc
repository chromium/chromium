// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/vector_database.h"

#include <algorithm>
#include <queue>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "third_party/farmhash/src/src/farmhash.h"

namespace history_embeddings {

uint32_t HashString(std::string_view str) {
  return util::Fingerprint32(str);
}

// Standard normalized magnitude for all embeddings.
constexpr float kUnitLength = 1.0f;

// Close enough to be considered near zero.
constexpr float kEpsilon = 0.01f;

// These delimiters separate queries and passages into tokens.
constexpr char kTokenDelimiters[] = " .,;";

namespace {

// Reduces and returns `term_view` with common characters trimmed from
// start and end.
inline std::string_view TrimTermView(std::string_view term_view) {
  return base::TrimString(term_view, ".?!,:;-()[]{}<>\"'/\\*&#~@^|%$`+=",
                          base::TrimPositions::TRIM_ALL);
}

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

  base::StringViewTokenizer tokenizer(passage, kTokenDelimiters);
  while (tokenizer.GetNext()) {
    const std::string_view token = TrimTermView(tokenizer.token());
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

ScoredUrl::ScoredUrl(history::URLID url_id,
                     history::VisitID visit_id,
                     base::Time visit_time,
                     float score,
                     float word_match_score)
    : url_id(url_id),
      visit_id(visit_id),
      visit_time(visit_time),
      score(score),
      word_match_score(word_match_score) {}
ScoredUrl::~ScoredUrl() = default;
ScoredUrl::ScoredUrl(ScoredUrl&&) = default;
ScoredUrl& ScoredUrl::operator=(ScoredUrl&&) = default;
ScoredUrl::ScoredUrl(const ScoredUrl&) = default;
ScoredUrl& ScoredUrl::operator=(const ScoredUrl&) = default;

////////////////////////////////////////////////////////////////////////////////

SearchParams::SearchParams() = default;
SearchParams::SearchParams(const SearchParams&) = default;
SearchParams::SearchParams(SearchParams&&) = default;
SearchParams::~SearchParams() = default;
SearchParams& SearchParams::operator=(const SearchParams&) = default;

////////////////////////////////////////////////////////////////////////////////

SearchInfo::SearchInfo() = default;
SearchInfo::SearchInfo(SearchInfo&&) = default;
SearchInfo::~SearchInfo() = default;

////////////////////////////////////////////////////////////////////////////////

UrlData::UrlData(history::URLID url_id,
                 history::VisitID visit_id,
                 base::Time visit_time)
    : url_id(url_id), visit_id(visit_id), visit_time(visit_time) {}
UrlData::UrlData(const UrlData&) = default;
UrlData::UrlData(UrlData&&) = default;
UrlData& UrlData::operator=(const UrlData&) = default;
UrlData& UrlData::operator=(UrlData&&) = default;
UrlData::~UrlData() = default;

bool UrlData::operator==(const UrlData& other) const {
  if (other.url_id == url_id && other.visit_id == visit_id &&
      other.visit_time == visit_time && embeddings == other.embeddings) {
    std::string a, b;
    if (other.passages.SerializeToString(&a) &&
        passages.SerializeToString(&b)) {
      return a == b;
    }
  }
  return false;
}

UrlScore UrlData::BestScoreWith(
    SearchInfo& search_info,
    const SearchParams& search_params,
    const passage_embeddings::Embedding& query_embedding,
    size_t min_passage_word_count) const {
  constexpr float kMaxFloat = std::numeric_limits<float>::max();
  float word_match_required_score =
      search_params.word_match_minimum_embedding_score;
  std::vector<size_t> term_counts;
  if (search_params.query_terms.size() >
      search_params.word_match_max_term_count) {
    // Disable word match boosting for this long query.
    word_match_required_score = kMaxFloat;
  } else {
    // Prepare to count terms by initializing all term counts to zero.
    // These will continue to increase for each passage until we have
    // the total for this URL's full passage set.
    term_counts.assign(search_params.query_terms.size(), 0);
  }

  float best = 0.0f;
  std::string modified_passage;
  const std::string* passage = nullptr;
  for (size_t i = 0; i < embeddings.size(); i++) {
    const passage_embeddings::Embedding& embedding = embeddings[i];
    passage = &passages.passages(i);

    // Skip non-ASCII strings to avoid scoring problems with the model.
    // Note that if `erase_non_ascii_characters` is true then the embeddings
    // have already be recomputed with non-ASCII characters excluded from the
    // source passages, and are thus usable for search. In such cases, we can
    // also modify the passage for term search.
    bool skip_similarity_scoring = false;
    if (!base::IsStringASCII(*passage)) {
      if (search_params.erase_non_ascii_characters ||
          search_params.word_match_search_non_ascii_passages) {
        search_info.modified_nonascii_passage_count++;
        if (word_match_required_score != kMaxFloat) {
          // Copy and modify the passage to exclude the non-ASCII characters.
          // Note that for efficiency this is only done when the modified
          // passage will actually be used for term counting in logic below.
          modified_passage = *passage;
          EraseNonAsciiCharacters(modified_passage);
          passage = &modified_passage;
          if (!search_params.erase_non_ascii_characters) {
            // The embedding for this passage is not valid, but the passage
            // can still be word match text searched.
            skip_similarity_scoring = true;
          }
        }
      } else {
        search_info.skipped_nonascii_passage_count++;
        continue;
      }
    }

    float score = skip_similarity_scoring || embedding.GetPassageWordCount() <
                                                 min_passage_word_count
                      ? 0.0f
                      : query_embedding.ScoreWith(embedding);

    if (score >= word_match_required_score || skip_similarity_scoring) {
      // Since the ASCII check above processed the whole passage string, it is
      // likely ready in CPU cache. Scan text again to count terms in passage.
      base::ElapsedTimer timer;
      CountTermsInPassage(term_counts, search_params.query_terms, *passage,
                          search_params.word_match_limit);
      search_info.passage_scanning_time += timer.Elapsed();
    }

    best = std::max(best, score);
  }

  // Calculate total boost from term counts across all passages.
  float word_match_boost = 0.0f;
  if (!term_counts.empty()) {
    size_t terms_found = 0;
    for (size_t term_count : term_counts) {
      float term_boost = search_params.word_match_score_boost_factor *
                         term_count / search_params.word_match_limit;
      // Boost factor is applied per term such that longer queries boost more.
      word_match_boost += term_boost;
      if (term_count > 0) {
        terms_found++;
      }
    }
    if (static_cast<float>(terms_found) /
            static_cast<float>(term_counts.size()) <
        search_params.word_match_required_term_ratio) {
      // Don't boost at all when not enough of the query terms were found.
      word_match_boost = 0.0f;
    } else {
      // Normalize to avoid over-boosting long queries with many words.
      word_match_boost /=
          std::max<size_t>(1, search_params.query_terms.size() +
                                  search_params.word_match_smoothing_factor);
    }
  }

  return UrlScore{
      .score = best + word_match_boost,
      .word_match_score = word_match_boost,
  };
}

////////////////////////////////////////////////////////////////////////////////

SearchInfo VectorDatabase::FindNearest(
    std::optional<base::Time> time_range_start,
    size_t count,
    const SearchParams& search_params,
    const passage_embeddings::Embedding& query_embedding,
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
  size_t min_passage_word_count =
      GetFeatureParameters().search_passage_minimum_word_count;

  struct CompareScore {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.score > b.score;
    }
  };
  struct CompareWordMatchScore {
    bool operator()(const ScoredUrl& a, const ScoredUrl& b) {
      return a.word_match_score > b.word_match_score;
    }
  };
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, CompareScore>
      top_by_score;
  std::priority_queue<ScoredUrl, std::vector<ScoredUrl>, CompareWordMatchScore>
      top_by_word_match_score;

  SearchInfo search_info;
  search_info.completed = true;
  base::ElapsedTimer total_timer;
  while (const UrlData* url_data = iterator->Next()) {
    if (is_search_halted.Run()) {
      search_info.completed = false;
      break;
    }
    search_info.searched_url_count++;
    search_info.searched_embedding_count += url_data->embeddings.size();

    base::ElapsedTimer scoring_timer;
    UrlScore url_score = url_data->BestScoreWith(
        search_info, search_params, query_embedding, min_passage_word_count);

    top_by_score.emplace(url_data->url_id, url_data->visit_id,
                         url_data->visit_time, url_score.score,
                         url_score.word_match_score);
    while (top_by_score.size() > count) {
      top_by_score.pop();
    }

    top_by_word_match_score.emplace(url_data->url_id, url_data->visit_id,
                                    url_data->visit_time, url_score.score,
                                    url_score.word_match_score);
    while (top_by_word_match_score.size() > count) {
      top_by_word_match_score.pop();
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

  // Empty queues into vectors and return results sorted with descending scores.
  while (!top_by_score.empty()) {
    search_info.scored_urls.push_back(top_by_score.top());
    top_by_score.pop();
  }
  while (!top_by_word_match_score.empty()) {
    search_info.word_match_scored_urls.push_back(top_by_word_match_score.top());
    top_by_word_match_score.pop();
  }
  std::ranges::reverse(search_info.scored_urls);
  std::ranges::reverse(search_info.word_match_scored_urls);
  return search_info;
}

////////////////////////////////////////////////////////////////////////////////

VectorDatabaseInMemory::VectorDatabaseInMemory() = default;
VectorDatabaseInMemory::~VectorDatabaseInMemory() = default;

void VectorDatabaseInMemory::SaveTo(VectorDatabase* database) {
  for (UrlData& url_data : data_) {
    database->AddUrlData(std::move(url_data));
  }
  data_.clear();
}

size_t VectorDatabaseInMemory::GetEmbeddingDimensions() const {
  return data_.empty() ? 0 : data_[0].embeddings[0].Dimensions();
}

bool VectorDatabaseInMemory::AddUrlData(UrlData url_data) {
  CHECK_EQ(static_cast<size_t>(url_data.passages.passages_size()),
           url_data.embeddings.size());
  if (!data_.empty()) {
    for (const passage_embeddings::Embedding& embedding : url_data.embeddings) {
      // All embeddings in the database must have equal dimensions.
      CHECK_EQ(embedding.Dimensions(), data_[0].embeddings[0].Dimensions());
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
    explicit SimpleIterator(const std::vector<UrlData>& source,
                            std::optional<base::Time> time_range_start)
        : iterator_(source.cbegin()),
          end_(source.cend()),
          time_range_start_(time_range_start) {}
    ~SimpleIterator() override = default;

    const UrlData* Next() override {
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

    std::vector<UrlData>::const_iterator iterator_;
    std::vector<UrlData>::const_iterator end_;
    const std::optional<base::Time> time_range_start_;
  };

  if (data_.empty()) {
    return nullptr;
  }

  return std::make_unique<SimpleIterator>(data_, time_range_start);
}

std::vector<std::string> SplitQueryToTerms(
    const std::unordered_set<uint32_t>& stop_words_hashes,
    std::string_view raw_query,
    size_t min_term_length) {
  // Configuration may permit zero-length terms, but empty strings
  // are never useful in search so the effective minimum then is one.
  min_term_length = min_term_length > 0 ? min_term_length : 1;
  std::string query = base::ToLowerASCII(raw_query);
  std::string_view query_view(query);
  std::vector<std::string> query_terms;

  base::StringViewTokenizer tokenizer(query_view, kTokenDelimiters);
  while (tokenizer.GetNext()) {
    const std::string_view term_view = TrimTermView(tokenizer.token());
    if (term_view.size() >= min_term_length &&
        !stop_words_hashes.contains(HashString(term_view))) {
      query_terms.emplace_back(term_view);
    }
  }

  return query_terms;
}

inline bool IsCharNonAscii(char c) {
  return (c & 0x80) != 0;
}

void EraseNonAsciiCharacters(std::string& passage) {
  // Inject spaces to avoid bridging terms. Even if this separates what
  // might have been a single term with ideal character conversions, it
  // won't create a blind spot for search because the query will be
  // converted in exactly the same way; then the separate terms match.
  // On the other hand, without the spaces, terms could be bridged and
  // become harder to find.
  for (size_t i = 1; i < passage.length(); i++) {
    if (IsCharNonAscii(passage[i]) && !IsCharNonAscii(passage[i - 1])) {
      // Note this never changes a non-ASCII character at index 0 because it
      // isn't needed. The character at index 1 is either ASCII, in which case
      // it will become the new first character; or it's non-ASCII, in which
      // case it will be removed along with the first.
      passage[i] = ' ';

      // Skip immediately following non-ASCII bytes; they will be removed
      // below after the space injection pass.
      while (i + 1 < passage.length() && IsCharNonAscii(passage[i + 1])) {
        i++;
      }
    }
  }

  // Erase all non-ASCII characters remaining.
  std::erase_if(passage, IsCharNonAscii);
}

void EraseNonAsciiCharacters(std::vector<std::string>& passages) {
  for (std::string& passage : passages) {
    EraseNonAsciiCharacters(passage);
  }
}

}  // namespace history_embeddings
