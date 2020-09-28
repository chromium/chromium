// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/inverted_index_search.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/components/local_search_service/content_extraction_utils.h"
#include "chromeos/components/local_search_service/inverted_index.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace local_search_service {

namespace {

using chromeos::string_matching::TokenizedString;
using ExtractedContent =
    std::vector<std::pair<std::string, std::vector<Token>>>;

std::vector<Token> ExtractDocumentTokens(const Data& data) {
  // Use input locale unless it's empty. In this case we will use system
  // default locale.
  const std::string locale =
      data.locale.empty() ? base::i18n::GetConfiguredLocale() : data.locale;
  std::vector<Token> document_tokens;
  for (const Content& content : data.contents) {
    DCHECK_GE(content.weight, 0);
    DCHECK_LE(content.weight, 1);
    const std::vector<Token> content_tokens =
        ExtractContent(content.id, content.content, content.weight, locale);
    document_tokens.insert(document_tokens.end(), content_tokens.begin(),
                           content_tokens.end());
  }
  return ConsolidateToken(document_tokens);
}

ExtractedContent ExtractDocumentsContent(const std::vector<Data>& data) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ExtractedContent documents;
  for (const Data& d : data) {
    const std::vector<Token> document_tokens = ExtractDocumentTokens(d);
    DCHECK(!document_tokens.empty());
    documents.push_back({d.id, document_tokens});
  }

  return documents;
}

}  // namespace

InvertedIndexSearch::InvertedIndexSearch(IndexId index_id,
                                         PrefService* local_state)
    : Index(index_id, Backend::kInvertedIndex, local_state),
      inverted_index_(std::make_unique<InvertedIndex>()),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

InvertedIndexSearch::~InvertedIndexSearch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

uint64_t InvertedIndexSearch::GetSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return inverted_index_->NumberDocuments();
}

void InvertedIndexSearch::AddOrUpdate(
    const std::vector<local_search_service::Data>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ExtractDocumentsContent, data),
      base::BindOnce(&InvertedIndexSearch::OnExtractDocumentsContentDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

uint32_t InvertedIndexSearch::Delete(const std::vector<std::string>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t num_deleted = inverted_index_->RemoveDocuments(ids);
  inverted_index_->BuildInvertedIndex();
  return num_deleted;
}

void InvertedIndexSearch::ClearIndex() {
  inverted_index_->ClearInvertedIndex();
}

ResponseStatus InvertedIndexSearch::Find(const base::string16& query,
                                         uint32_t max_results,
                                         std::vector<Result>* results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks start = base::TimeTicks::Now();
  DCHECK(results);
  results->clear();
  if (query.empty()) {
    const ResponseStatus status = ResponseStatus::kEmptyQuery;
    MaybeLogSearchResultsStats(status, 0u, base::TimeDelta());
    return status;
  }
  if (GetSize() == 0u) {
    const ResponseStatus status = ResponseStatus::kEmptyIndex;
    MaybeLogSearchResultsStats(status, 0u, base::TimeDelta());
    return status;
  }

  // TODO(jiameng): actual input query may not be the same as default locale.
  // Need another way to determine actual language of the query.
  const TokenizedString::Mode mode =
      IsNonLatinLocale(base::i18n::GetConfiguredLocale())
          ? TokenizedString::Mode::kCamelCase
          : TokenizedString::Mode::kWords;

  const TokenizedString tokenized_query(query, mode);
  std::unordered_set<base::string16> tokens;
  for (const auto& token : tokenized_query.tokens()) {
    // TODO(jiameng): we are not removing stopword because they shouldn't exist
    // in the index. However, for performance reason, it may be worth to be
    // removed.
    tokens.insert(token);
  }

  *results = inverted_index_->FindMatchingDocumentsApproximately(
      tokens, search_params_.prefix_threshold, search_params_.fuzzy_threshold);

  if (results->size() > max_results && max_results > 0u)
    results->resize(max_results);

  const base::TimeTicks end = base::TimeTicks::Now();
  const ResponseStatus status = ResponseStatus::kSuccess;
  MaybeLogSearchResultsStats(status, results->size(), end - start);
  return status;
}

std::vector<std::pair<std::string, uint32_t>>
InvertedIndexSearch::FindTermForTesting(const base::string16& term) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const PostingList posting_list = inverted_index_->FindTerm(term);
  std::vector<std::pair<std::string, uint32_t>> doc_with_freq;
  for (const auto& kv : posting_list) {
    doc_with_freq.push_back({kv.first, kv.second.size()});
  }

  return doc_with_freq;
}

void InvertedIndexSearch::OnExtractDocumentsContentDone(
    const ExtractedContent& documents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inverted_index_->AddDocuments(documents);
  inverted_index_->BuildInvertedIndex();
}

}  // namespace local_search_service
}  // namespace chromeos
