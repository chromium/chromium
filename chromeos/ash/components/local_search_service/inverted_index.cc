// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/inverted_index.h"

#include <numeric>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/local_search_service/search_utils.h"

namespace ash::local_search_service {

namespace {

// (document-score, posting-of-all-matching-terms).
using ScoreWithPosting = std::pair<double, Posting>;

// Calculates TF-IDF scores for a term
std::vector<TfidfResult> CalculateTfidf(const std::u16string& term,
                                        const DocLength& doc_length,
                                        const Dictionary& dictionary) {
  std::vector<TfidfResult> results;
  // We don't apply weights to idf because the effect is likely small.
  const float idf =
      1.0 + log((1.0 + doc_length.size()) / (1.0 + dictionary.at(term).size()));

  for (const auto& item : dictionary.at(term)) {
    // If a term has a very low content weight in a doc, its effective number of
    // occurrences in the doc should be lower. Strictly speaking, the effective
    // length of the doc should be smaller too. However, for performance
    // reasons, we only apply the weight to the term occurrences but not doc
    // length.
    // TODO(jiameng): this is an expensive operation, we will need to monitor
    // its performance and optimize it.
    const double effective_term_occ = std::accumulate(
        item.second.begin(), item.second.end(), 0.0,
        [](double sum, const WeightedPosition& weighted_position) {
          return sum + weighted_position.weight;
        });
    const float tf = effective_term_occ / doc_length.at(item.first);
    results.push_back({item.first, item.second, tf * idf});
  }
  return results;
}

// Builds TF-IDF cache given the data. Since this function is expensive, it
// should run on a non-blocking thread that is different than the main thread.
TfidfCache BuildTfidf(uint32_t num_docs_from_last_update,
                      const DocLength& doc_length,
                      const Dictionary& dictionary,
                      const TermSet& terms_to_be_updated,
                      const TfidfCache& tfidf_cache) {
  // TODO(crbug.com/40152719): consider moving the helper functions inside the
  // class so that we can use SequenceChecker.
  TfidfCache new_cache(tfidf_cache);
  // If number of documents doesn't change from the last time index was built,
  // we only need to update terms in |terms_to_be_updated|. Otherwise we need
  // to rebuild the index.
  if (num_docs_from_last_update == doc_length.size()) {
    for (const auto& term : terms_to_be_updated) {
      if (dictionary.find(term) != dictionary.end()) {
        new_cache[term] = CalculateTfidf(term, doc_length, dictionary);
      } else {
        new_cache.erase(term);
      }
    }
  } else {
    new_cache.clear();
    for (const auto& item : dictionary) {
      new_cache[item.first] =
          CalculateTfidf(item.first, doc_length, dictionary);
    }
  }
  return new_cache;
}

// Removes a document from document state variables given it's ID. Don't do
// anything if the ID doesn't exist. Return true if the document is removed.
bool RemoveDocumentIfExist(const std::string& document_id,
                           DocLength* doc_length,
                           Dictionary* dictionary,
                           TermSet* terms_to_be_updated) {
  CHECK(doc_length);
  CHECK(dictionary);
  CHECK(terms_to_be_updated);
  bool document_removed = false;
  if (doc_length->find(document_id) == doc_length->end())
    return document_removed;
  doc_length->erase(document_id);
  for (auto it = dictionary->begin(); it != dictionary->end();) {
    if (it->second.find(document_id) != it->second.end()) {
      terms_to_be_updated->insert(it->first);
      it->second.erase(document_id);
      document_removed = true;
    }

    // Removes term from the dictionary if its posting list is empty.
    if (it->second.empty()) {
      it = dictionary->erase(it);
    } else {
      it++;
    }
  }
  return document_removed;
}

// Given list of documents to update and document state variables, returns new
// document state variables and number of deleted documents.
std::pair<DocumentStateVariables, uint32_t> UpdateDocumentStateVariables(
    DocumentToUpdate&& documents_to_update,
    const DocLength& doc_length,
    Dictionary&& dictionary,
    TermSet&& terms_to_be_updated) {
  DocLength new_doc_length(doc_length);
  uint32_t num_deleted = 0u;
  for (const auto& document : documents_to_update) {
    const std::string document_id(document.first);
    bool is_deleted = RemoveDocumentIfExist(document_id, &new_doc_length,
                                            &dictionary, &terms_to_be_updated);

    // Update the document if necessary.
    if (!document.second.empty()) {
      // If document content is not empty, it is being updated but not
      // deleted.
      is_deleted = false;
      for (const auto& token : document.second) {
        dictionary[token.content][document_id] = token.positions;
        new_doc_length[document_id] += token.positions.size();
        terms_to_be_updated.insert(token.content);
      }
    }
    num_deleted += (is_deleted) ? 1 : 0;
  }

  return std::make_pair(
      std::make_tuple(std::move(new_doc_length), std::move(dictionary),
                      std::move(terms_to_be_updated)),
      num_deleted);
}

// Given the index variables, clear all the data.
std::pair<DocumentStateVariables, TfidfCache> ClearData(
    DocumentToUpdate&& documents_to_update,
    const DocLength& doc_length,
    Dictionary&& dictionary,
    TermSet&& terms_to_be_updated,
    TfidfCache&& tfidf_cache) {
  DocLength new_doc_length;
  documents_to_update.clear();
  dictionary.clear();
  terms_to_be_updated.clear();
  tfidf_cache.clear();
  return std::make_pair(
      std::make_tuple(std::move(new_doc_length), std::move(dictionary),
                      std::move(terms_to_be_updated)),
      std::move(tfidf_cache));
}

}  // namespace

InvertedIndex::InvertedIndex() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
}
InvertedIndex::~InvertedIndex() = default;

PostingList InvertedIndex::FindTerm(const std::u16string& term) const {
  auto it = dictionary_.find(term);
  if (it != dictionary_.end()) {
    return it->second;
  }

  return {};
}

std::vector<Result> InvertedIndex::FindMatchingDocumentsApproximately(
    const std::unordered_set<std::u16string>& terms,
    double prefix_threshold,
    double block_threshold) const {
  // For each document, its score is the sum of the scores of its terms that
  // match one of more query term. Each term's score is the product of its
  // TF-IDF score and its match relevance score.
  // The map is keyed by the document id.
  std::unordered_map<std::string, ScoreWithPosting> matching_docs;
  for (const auto& kv : tfidf_cache_) {
    const std::u16string& index_term = kv.first;
    const std::vector<TfidfResult>& tfidf_results = kv.second;
    for (const auto& term : terms) {
      const float relevance = RelevanceCoefficient(
          term, index_term, prefix_threshold, block_threshold);
      if (relevance > 0) {
        // If the |index_term| is relevant, all of the enclosing documents will
        // have their ranking scores updated.
        for (const auto& docid_tfidf : tfidf_results) {
          const std::string& docid = std::get<0>(docid_tfidf);
          const Posting& posting = std::get<1>(docid_tfidf);
          const float tfidf = std::get<2>(docid_tfidf);
          auto it = matching_docs.find(docid);
          if (it == matching_docs.end()) {
            it = matching_docs.emplace(docid, ScoreWithPosting(0.0, {})).first;
          }

          auto& score_posting = it->second;
          // TODO(jiameng): add position penalty.
          score_posting.first += tfidf * relevance;
          // Also update matching positions.
          auto& existing_posting = score_posting.second;
          existing_posting.insert(existing_posting.end(), posting.begin(),
                                  posting.end());
        }
        // Break out from inner loop, i.e. no need to check other query terms.
        break;
      }
    }
  }

  std::vector<Result> sorted_matching_docs;
  for (const auto& kv : matching_docs) {
    // We don't need to include weights in the search results.
    std::vector<Position> positions;
    for (const auto& weighted_position : kv.second.second) {
      positions.emplace_back(weighted_position.position);
    }
    sorted_matching_docs.emplace_back(
        Result(kv.first, kv.second.first, positions));
  }
  std::sort(sorted_matching_docs.begin(), sorted_matching_docs.end(),
            CompareResults);
  return sorted_matching_docs;
}

void InvertedIndex::AddDocuments(const DocumentToUpdate& documents,
                                 base::OnceCallback<void()> callback) {
  if (documents.empty())
    return;

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UpdateDocumentStateVariables, documents,
                     std::move(doc_length_), std::move(dictionary_),
                     std::move(terms_to_be_updated_)),
      base::BindOnce(&InvertedIndex::OnAddDocumentsComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InvertedIndex::RemoveDocuments(
    const std::vector<std::string>& document_ids,
    base::OnceCallback<void(uint32_t)> callback) {
  DocumentToUpdate documents;
  for (const auto& id : document_ids) {
    documents.push_back({id, std::vector<Token>()});
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UpdateDocumentStateVariables, documents,
                     std::move(doc_length_), std::move(dictionary_),
                     std::move(terms_to_be_updated_)),
      base::BindOnce(&InvertedIndex::OnUpdateDocumentsComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InvertedIndex::UpdateDocuments(
    const DocumentToUpdate& documents,
    base::OnceCallback<void(uint32_t)> callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&UpdateDocumentStateVariables, documents,
                     std::move(doc_length_), std::move(dictionary_),
                     std::move(terms_to_be_updated_)),
      base::BindOnce(&InvertedIndex::OnUpdateDocumentsComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::vector<TfidfResult> InvertedIndex::GetTfidf(
    const std::u16string& term) const {
  auto it = tfidf_cache_.find(term);
  if (it != tfidf_cache_.end()) {
    return it->second;
  }

  return {};
}

void InvertedIndex::BuildInvertedIndex(base::OnceCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BuildTfidf, num_docs_from_last_update_, doc_length_,
                     dictionary_, std::move(terms_to_be_updated_),
                     tfidf_cache_),
      base::BindOnce(&InvertedIndex::OnBuildTfidfComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InvertedIndex::ClearInvertedIndex(base::OnceCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ClearData, std::move(documents_to_update_), doc_length_,
                     std::move(dictionary_), std::move(terms_to_be_updated_),
                     std::move(tfidf_cache_)),
      base::BindOnce(&InvertedIndex::OnDataCleared,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InvertedIndex::OnBuildTfidfComplete(base::OnceCallback<void()> callback,
                                         TfidfCache&& new_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_docs_from_last_update_ = doc_length_.size();
  tfidf_cache_ = std::move(new_cache);

  std::move(callback).Run();
}

void InvertedIndex::OnUpdateDocumentsComplete(
    base::OnceCallback<void(uint32_t)> callback,
    std::pair<DocumentStateVariables, uint32_t>&&
        document_state_variables_and_num_deleted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  doc_length_ =
      std::move(std::get<0>(document_state_variables_and_num_deleted.first));
  dictionary_ =
      std::move(std::get<1>(document_state_variables_and_num_deleted.first));
  terms_to_be_updated_ =
      std::move(std::get<2>(document_state_variables_and_num_deleted.first));

  BuildInvertedIndex(base::BindOnce(
      [](base::OnceCallback<void(uint32_t)> callback, uint32_t num_deleted) {
        std::move(callback).Run(num_deleted);
      },
      std::move(callback), document_state_variables_and_num_deleted.second));
}

void InvertedIndex::OnAddDocumentsComplete(
    base::OnceCallback<void()> callback,
    std::pair<DocumentStateVariables, uint32_t>&&
        document_state_variables_and_num_deleted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(document_state_variables_and_num_deleted.second, 0u);
  doc_length_ =
      std::move(std::get<0>(document_state_variables_and_num_deleted.first));
  dictionary_ =
      std::move(std::get<1>(document_state_variables_and_num_deleted.first));
  terms_to_be_updated_ =
      std::move(std::get<2>(document_state_variables_and_num_deleted.first));

  BuildInvertedIndex(std::move(callback));
}

void InvertedIndex::OnDataCleared(
    base::OnceCallback<void()> callback,
    std::pair<DocumentStateVariables, TfidfCache>&& inverted_index_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  doc_length_ = std::move(std::get<0>(inverted_index_data.first));
  dictionary_ = std::move(std::get<1>(inverted_index_data.first));
  terms_to_be_updated_ = std::move(std::get<2>(inverted_index_data.first));
  tfidf_cache_ = std::move(inverted_index_data.second);
  num_docs_from_last_update_ = 0;

  std::move(callback).Run();
}

}  // namespace ash::local_search_service
