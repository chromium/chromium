// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"

namespace ash::local_search_service {

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
using Dictionary = std::unordered_map<std::u16string, PostingList>;

// A set of terms.
using TermSet = std::unordered_set<std::u16string>;

// Data structure to store TF-IDF cache keyed by terms.
using TfidfCache = std::unordered_map<std::u16string, std::vector<TfidfResult>>;

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

  // Returns document ID and positions of a term.
  PostingList FindTerm(const std::u16string& term) const;

  // Returns documents that approximately match one or more terms in |terms|.
  // Returned documents will be ranked.
  std::vector<Result> FindMatchingDocumentsApproximately(
      const std::unordered_set<std::u16string>& terms,
      double prefix_threshold,
      double block_threshold) const;

  // Adds new documents to the inverted index. If the document ID is already in
  // the index, remove the existing and add the new one. All tokens must be
  // unique (have unique content). It'll build TF-IDF cache after adding
  // documents.
  void AddDocuments(const DocumentToUpdate& documents,
                    base::OnceCallback<void()> callback);

  // Removes documents from the inverted index. Do nothing if the document id is
  // not in the index. It will build TF-IDF cache after removing documents.
  void RemoveDocuments(const std::vector<std::string>& document_ids,
                       base::OnceCallback<void(uint32_t)> callback);

  // Updates documents from the inverted index. It combines two functions:
  // AddDocuments and RemoveDocument. This function will returns number of
  // documents to be removed (number of documents that have empty content).
  //   - If a document ID is not in the index, add the document to the index.
  //   - If a document ID is in the index and it's new content isn't empty,
  //   update it's content in the index.
  //   - If a document ID is in the index and it's content is empty, remove it
  //   from the index.
  // It will build TF-IDF cache after updating the documents.
  void UpdateDocuments(const DocumentToUpdate& documents,
                       base::OnceCallback<void(uint32_t)> callback);

  // Gets TF-IDF scores for a term. This function returns the TF-IDF score from
  // the cache.
  // Note: client of this function should call BuildInvertedIndex before using
  // this function to have up-to-date score.
  std::vector<TfidfResult> GetTfidf(const std::u16string& term) const;

  // Builds the inverted index.
  void BuildInvertedIndex(base::OnceCallback<void()> callback);

  // Clears all the data from the inverted index.
  void ClearInvertedIndex(base::OnceCallback<void()> callback);

  // Checks if the inverted index has been built: returns |true| if the inverted
  // index is up to date, returns |false| if there are some modified document
  // since the last time the index has been built.
  bool IsInvertedIndexBuilt() const { return is_index_built_; }

  // Returns number of documents in the index.
  uint64_t NumberDocuments() const { return doc_length_.size(); }

 private:
  friend class InvertedIndexTest;

  // Called on the main thread after BuildTfidf is completed.
  void OnBuildTfidfComplete(base::OnceCallback<void()> callback,
                            TfidfCache&& new_cache);
  // Called on the main thread after UpdateDocumentsStateVariables is completed.
  void OnUpdateDocumentsComplete(base::OnceCallback<void(uint32_t)> callback,
                                 std::pair<DocumentStateVariables, uint32_t>&&
                                     document_state_variables_and_num_deleted);
  void OnAddDocumentsComplete(base::OnceCallback<void()> callback,
                              std::pair<DocumentStateVariables, uint32_t>&&
                                  document_state_variables_and_num_deleted);

  void OnDataCleared(
      base::OnceCallback<void()> callback,
      std::pair<DocumentStateVariables, TfidfCache>&& inverted_index_data);

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

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InvertedIndex> weak_ptr_factory_{this};
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_H_
