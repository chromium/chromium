// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/shared_structs.h"

namespace chromeos {
namespace local_search_service {
// A posting is a list of WeightedPosition.
using Posting = std::vector<WeightedPosition>;

// A map from document id to posting.
using PostingList = std::unordered_map<std::string, Posting>;

// A tuple that stores a document ID, token's positions and token's TF-IDF
// score.
using TfidfResult = std::tuple<std::string, Posting, float>;

// A map from document IDs to their length.
using DocLength = std::unordered_map<std::string, uint32_t>;

// A map from terms to their PostingList.
using Dictionary = std::unordered_map<base::string16, PostingList>;

// A set of terms.
using TermSet = std::unordered_set<base::string16>;

// Data structure to store TF-IDF cache keyed by terms.
using TfidfCache = std::unordered_map<base::string16, std::vector<TfidfResult>>;

// Tuple to store document state variables.
using DocumentStateVariables = std::tuple<DocLength, Dictionary, TermSet>;

// A vector that stores documents to update. If the token vector is empty, the
// corresponding document will be deleted.
using DocumentToUpdate =
    std::vector<std::pair<std::string, std::vector<Token>>>;

// InvertedIndex stores the inverted index for local search. It provides the
// abilities to add/remove documents, find term, etc. Before this class can be
// used to return tf-idf scores of a term, the client should build the index
// first (using BuildInvertedIndex).
class InvertedIndex {
 public:
  InvertedIndex();
  ~InvertedIndex();
  InvertedIndex(const InvertedIndex&) = delete;
  InvertedIndex& operator=(const InvertedIndex&) = delete;

  // |on_index_built| will be called after the index is built.
  void RegisterIndexBuiltCallback(
      base::RepeatingCallback<void()> on_index_built);

  // Returns document ID and positions of a term.
  PostingList FindTerm(const base::string16& term) const;

  // Returns documents that approximately match one or more terms in |terms|.
  // Returned documents will be ranked.
  std::vector<Result> FindMatchingDocumentsApproximately(
      const std::unordered_set<base::string16>& terms,
      double prefix_threshold,
      double block_threshold) const;

  // Adds new documents to the inverted index. If the document ID is already in
  // the index, remove the existing and add the new one. All tokens must be
  // unique (have unique content). This function doesn't modify any cache. It
  // only adds documents and tokens to the index.
  void AddDocuments(const DocumentToUpdate& documents);

  // Removes documents from the inverted index. Do nothing if the document id is
  // not in the index. Returns number of documents deleted.
  // This function doesn't modify any cache. It only removes
  // documents and tokens from the index.
  uint32_t RemoveDocuments(const std::vector<std::string>& document_ids);

  // Gets TF-IDF scores for a term. This function returns the TF-IDF score from
  // the cache.
  // Note: client of this function should call BuildInvertedIndex before using
  // this function to have up-to-date score.
  std::vector<TfidfResult> GetTfidf(const base::string16& term) const;

  // Builds the inverted index.
  void BuildInvertedIndex();

  // Clears all the data from the inverted index.
  void ClearInvertedIndex();

  // Checks if the inverted index has been built: returns |true| if the inverted
  // index is up to date, returns |false| if there are some modified document
  // since the last time the index has been built.
  bool IsInvertedIndexBuilt() const { return is_index_built_; }

  // Returns number of documents in the index.
  uint64_t NumberDocuments() const { return doc_length_.size(); }

 private:
  friend class InvertedIndexTest;

  // This is the single function that actually changes state variables. In
  // summary, it schedules all heavy-duty work to workers, and it does so one at
  // the time. Moreover, document-updating request takes precedence over
  // index-building request
  void InvertedIndexController();

  // Called on the main thread after BuildTfidf is completed.
  void OnBuildTfidfComplete(TfidfCache&& new_cache);

  // Called on the main thread after UpdateDocuments is completed.
  void OnUpdateDocumentsComplete(
      DocumentStateVariables&& document_state_variables);

  void OnDataCleared(
      std::pair<DocumentStateVariables, TfidfCache>&& inverted_index_data);

  base::RepeatingCallback<void()> on_index_built_;

  // |is_index_built_| is only true if index's TF-IDF is consistent with the
  // documents in the index. This means as soon as documents are modified
  // (added, updated or deleted), |is_index_built_| will be set to false. While
  // the index is being rebuilt, its value will remain false. After the index is
  // fully built/rebuilt, this value will be set to true.
  bool is_index_built_ = true;

  // Set of the terms that are needed to be update in |tfidf_cache_|.
  TermSet terms_to_be_updated_;
  // Contains the length of the document (the number of terms in the document).
  // The size of this map will always equal to the number of documents in the
  // index.
  DocLength doc_length_;
  // A map from term to PostingList.
  Dictionary dictionary_;
  // Contains the TF-IDF scores for all the term in the index.
  TfidfCache tfidf_cache_;
  // Stores the documents that need to be updated.
  DocumentToUpdate documents_to_update_;
  // Number of documents when the index was built.
  uint32_t num_docs_from_last_update_ = 0;
  bool request_to_build_index_ = false;
  bool update_in_progress_ = false;
  bool index_building_in_progress_ = false;
  bool request_to_clear_index_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InvertedIndex> weak_ptr_factory_{this};
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_
