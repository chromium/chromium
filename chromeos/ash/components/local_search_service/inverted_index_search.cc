// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/inverted_index_search.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/local_search_service/content_extraction_utils.h"
#include "chromeos/ash/components/local_search_service/inverted_index.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::local_search_service {

namespace {

using string_matching::TokenizedString;
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
  ExtractedContent documents;
  for (const Data& d : data) {
    const std::vector<Token> document_tokens = ExtractDocumentTokens(d);
    documents.push_back({d.id, document_tokens});
  }

  return documents;
}

std::unordered_set<std::u16string> GetTokenizedQuery(
    const std::u16string& query) {
  // TODO(jiameng): actual input query may not be the same as default locale.
  // Need another way to determine actual language of the query.
  const TokenizedString::Mode mode =
      IsNonLatinLocale(base::i18n::GetConfiguredLocale())
          ? TokenizedString::Mode::kCamelCase
          : TokenizedString::Mode::kWords;

  const TokenizedString tokenized_query(query, mode);
  std::unordered_set<std::u16string> tokens;
  for (const auto& token : tokenized_query.tokens()) {
    // TODO(jiameng): we are not removing stopword because they shouldn't exist
    // in the index. However, for performance reason, it may be worth to be
    // removed.
    tokens.insert(token);
  }
  return tokens;
}

}  // namespace

InvertedIndexSearch::InvertedIndexSearch(IndexId index_id)
    : Index(index_id, Backend::kInvertedIndex),
      inverted_index_(std::make_unique<InvertedIndex>()),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

InvertedIndexSearch::~InvertedIndexSearch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InvertedIndexSearch::GetSize(GetSizeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(inverted_index_->NumberDocuments());
}

void InvertedIndexSearch::AddOrUpdate(const std::vector<Data>& data,
                                      AddOrUpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!data.empty());
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ExtractDocumentsContent, data),
      base::BindOnce(
          &InvertedIndexSearch::FinalizeAddOrUpdate,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&InvertedIndexSearch::AddOrUpdateCallbackWithTime,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         base::Time::Now())));
}

void InvertedIndexSearch::Delete(const std::vector<std::string>& ids,
                                 DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ids.empty());
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(
          &InvertedIndexSearch::FinalizeDelete, weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&InvertedIndexSearch::DeleteCallbackWithTime,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         base::Time::Now()),
          ids));
}

void InvertedIndexSearch::UpdateDocuments(const std::vector<Data>& data,
                                          UpdateDocumentsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!data.empty());
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ExtractDocumentsContent, data),
      base::BindOnce(
          &InvertedIndexSearch::FinalizeUpdateDocuments,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&InvertedIndexSearch::UpdateDocumentsCallbackWithTime,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         base::Time::Now())));
}

void InvertedIndexSearch::Find(const std::u16string& query,
                               uint32_t max_results,
                               FindCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks start = base::TimeTicks::Now();
  if (query.empty()) {
    const ResponseStatus status = ResponseStatus::kEmptyQuery;
    MaybeLogSearchResultsStats(status, 0u, base::TimeDelta());
    std::move(callback).Run(status, std::nullopt);
    return;
  }
  if (inverted_index_->NumberDocuments() == 0u) {
    const ResponseStatus status = ResponseStatus::kEmptyIndex;
    MaybeLogSearchResultsStats(status, 0u, base::TimeDelta());
    std::move(callback).Run(status, std::nullopt);
    return;
  }

  std::vector<Result> results =
      inverted_index_->FindMatchingDocumentsApproximately(
          GetTokenizedQuery(query), search_params_.prefix_threshold,
          search_params_.fuzzy_threshold);

  if (results.size() > max_results && max_results > 0u)
    results.resize(max_results);

  const ResponseStatus status = ResponseStatus::kSuccess;
  const base::TimeTicks end = base::TimeTicks::Now();
  MaybeLogSearchResultsStats(status, results.size(), end - start);
  std::move(callback).Run(status, results);
}

void InvertedIndexSearch::ClearIndex(ClearIndexCallback callback) {
  inverted_index_->ClearInvertedIndex(base::BindOnce(
      &InvertedIndexSearch::ClearIndexCallbackWithTime,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), base::Time::Now()));
}

uint32_t InvertedIndexSearch::GetIndexSize() const {
  return inverted_index_->NumberDocuments();
}

std::vector<std::pair<std::string, uint32_t>>
InvertedIndexSearch::FindTermForTesting(const std::u16string& term) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const PostingList posting_list = inverted_index_->FindTerm(term);
  std::vector<std::pair<std::string, uint32_t>> doc_with_freq;
  for (const auto& kv : posting_list) {
    doc_with_freq.push_back({kv.first, kv.second.size()});
  }

  return doc_with_freq;
}

void InvertedIndexSearch::FinalizeAddOrUpdate(
    AddOrUpdateCallback callback,
    const ExtractedContent& documents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inverted_index_->AddDocuments(documents, std::move(callback));
}

void InvertedIndexSearch::FinalizeDelete(DeleteCallback callback,
                                         const std::vector<std::string>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inverted_index_->RemoveDocuments(ids, std::move(callback));
}

void InvertedIndexSearch::FinalizeUpdateDocuments(
    UpdateDocumentsCallback callback,
    const ExtractedContent& documents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inverted_index_->UpdateDocuments(documents, std::move(callback));
}

}  // namespace ash::local_search_service
