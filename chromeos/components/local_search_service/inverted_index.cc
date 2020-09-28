// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/inverted_index.h"

#include <numeric>
#include <string>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/components/local_search_service/search_utils.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace local_search_service {

namespace {

// (document-score, posting-of-all-matching-terms).
using ScoreWithPosting = std::pair<double, Posting>;

// Calculates TF-IDF scores for a term
std::vector<TfidfResult> CalculateTfidf(const base::string16& term,
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
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
// anything if the ID doesn't exist.
void RemoveDocumentIfExist(const std::string& document_id,
                           DocLength* doc_length,
                           Dictionary* dictionary,
                           TermSet* terms_to_be_updated) {
  CHECK(doc_length);
  CHECK(dictionary);
  CHECK(terms_to_be_updated);
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (doc_length->find(document_id) == doc_length->end())
    return;
  doc_length->erase(document_id);
  for (auto it = dictionary->begin(); it != dictionary->end();) {
    if (it->second.find(document_id) != it->second.end()) {
      terms_to_be_updated->insert(it->first);
      it->second.erase(document_id);
    }

    // Removes term from the dictionary if its posting list is empty.
    if (it->second.empty()) {
      it = dictionary->erase(it);
    } else {
      it++;
    }
  }
}

// Given list of documents to update and document state variables, returns new
// document state variables.
DocumentStateVariables UpdateDocuments(DocumentToUpdate&& documents_to_update,
                                       const DocLength& doc_length,
                                       Dictionary&& dictionary,
                                       TermSet&& terms_to_be_updated) {
  DCHECK(!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI));
  DocLength new_doc_length(doc_length);
  for (const auto& document : documents_to_update) {
    const std::string document_id(document.first);
    RemoveDocumentIfExist(document_id, &new_doc_length, &dictionary,
                          &terms_to_be_updated);

    // Update the document if necessary.
    if (!document.second.empty()) {
      for (const auto& token : document.second) {
        dictionary[token.content][document_id] = token.positions;
        new_doc_length[document_id] += token.positions.size();
        terms_to_be_updated.insert(token.content);
      }
    }
  }

  return std::make_tuple(std::move(new_doc_length), std::move(dictionary),
                         std::move(terms_to_be_updated));
}

// Given the index variables, clear all the data.
std::pair<DocumentStateVariables, TfidfCache> ClearData(
    DocumentToUpdate&& documents_to_update,
    const DocLength& doc_length,
    Dictionary&& dictionary,
    TermSet&& terms_to_be_updated,
    TfidfCache&& tfidf_cache) {
  DCHECK(!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI));
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

InvertedIndex::InvertedIndex() = default;
InvertedIndex::~InvertedIndex() = default;

void InvertedIndex::RegisterIndexBuiltCallback(
    base::RepeatingCallback<void()> on_index_built) {
  on_index_built_ = std::move(on_index_built);
}

PostingList InvertedIndex::FindTerm(const base::string16& term) const {
  if (dictionary_.find(term) != dictionary_.end())
    return dictionary_.at(term);

  return {};
}

std::vector<Result> InvertedIndex::FindMatchingDocumentsApproximately(
    const std::unordered_set<base::string16>& terms,
    double prefix_threshold,
    double block_threshold) const {
  // For each document, its score is the sum of TF-IDF scores of its terms
  // that match one of more query term.
  // The map is keyed by the document id.
  std::unordered_map<std::string, ScoreWithPosting> matching_docs;
  for (const auto& kv : tfidf_cache_) {
    const base::string16& index_term = kv.first;
    const std::vector<TfidfResult>& tfidf_results = kv.second;
    for (const auto& term : terms) {
      if (IsRelevantApproximately(term, index_term, prefix_threshold,
                                  block_threshold)) {
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
          score_posting.first += tfidf;
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

void InvertedIndex::AddDocuments(const DocumentToUpdate& documents) {
  if (documents.empty())
    return;

  is_index_built_ = false;
  documents_to_update_.insert(documents_to_update_.end(), documents.begin(),
                              documents.end());
  InvertedIndexController();
}

uint32_t InvertedIndex::RemoveDocuments(
    const std::vector<std::string>& document_ids) {
  uint32_t num_erase = 0;
  for (const auto& id : document_ids) {
    if (doc_length_.find(id) == doc_length_.end())
      continue;
    num_erase++;
    documents_to_update_.push_back({id, std::vector<Token>()});
  }

  if (num_erase == 0)
    return num_erase;

  is_index_built_ = false;
  InvertedIndexController();
  return num_erase;
}

std::vector<TfidfResult> InvertedIndex::GetTfidf(
    const base::string16& term) const {
  if (tfidf_cache_.find(term) != tfidf_cache_.end()) {
    return tfidf_cache_.at(term);
  }

  return {};
}

void InvertedIndex::BuildInvertedIndex() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_to_build_index_ = true;
  InvertedIndexController();
}

void InvertedIndex::ClearInvertedIndex() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_to_clear_index_ = true;
  InvertedIndexController();
}

void InvertedIndex::InvertedIndexController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(thanhdng): A clear-index call should ideally cancel all other update
  // operations. Need to update the code to reflect this.
  if (update_in_progress_)
    return;

  if (request_to_clear_index_) {
    update_in_progress_ = true;
    request_to_clear_index_ = false;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ClearData, std::move(documents_to_update_), doc_length_,
                       std::move(dictionary_), std::move(terms_to_be_updated_),
                       std::move(tfidf_cache_)),
        base::BindOnce(&InvertedIndex::OnDataCleared,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (documents_to_update_.empty()) {
    if (request_to_build_index_) {
      update_in_progress_ = true;
      index_building_in_progress_ = true;
      request_to_build_index_ = false;
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&BuildTfidf, num_docs_from_last_update_, doc_length_,
                         dictionary_, std::move(terms_to_be_updated_),
                         tfidf_cache_),
          base::BindOnce(&InvertedIndex::OnBuildTfidfComplete,
                         weak_ptr_factory_.GetWeakPtr()));
    } else if (terms_to_be_updated_.empty()) {
      // If there's no more work to do and all changed terms have been used to
      // update the index, then mark index is built and make the callback.
      is_index_built_ = true;
      if (!on_index_built_.is_null())
        on_index_built_.Run();
    }
  } else {
    update_in_progress_ = true;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        // Can't move doc_length_ since it is used to check if a document exists
        // or not.
        base::BindOnce(&UpdateDocuments, std::move(documents_to_update_),
                       doc_length_, std::move(dictionary_),
                       std::move(terms_to_be_updated_)),
        base::BindOnce(&InvertedIndex::OnUpdateDocumentsComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void InvertedIndex::OnBuildTfidfComplete(TfidfCache&& new_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  num_docs_from_last_update_ = doc_length_.size();
  tfidf_cache_ = std::move(new_cache);

  update_in_progress_ = false;
  index_building_in_progress_ = false;
  InvertedIndexController();
}

void InvertedIndex::OnUpdateDocumentsComplete(
    DocumentStateVariables&& document_state_variables) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  doc_length_ = std::move(std::get<0>(document_state_variables));
  dictionary_ = std::move(std::get<1>(document_state_variables));
  terms_to_be_updated_ = std::move(std::get<2>(document_state_variables));

  update_in_progress_ = false;
  InvertedIndexController();
}

void InvertedIndex::OnDataCleared(
    std::pair<DocumentStateVariables, TfidfCache>&& inverted_index_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  doc_length_ = std::move(std::get<0>(inverted_index_data.first));
  dictionary_ = std::move(std::get<1>(inverted_index_data.first));
  terms_to_be_updated_ = std::move(std::get<2>(inverted_index_data.first));
  tfidf_cache_ = std::move(inverted_index_data.second);

  num_docs_from_last_update_ = 0;
  update_in_progress_ = false;
  InvertedIndexController();
}

}  // namespace local_search_service
}  // namespace chromeos
