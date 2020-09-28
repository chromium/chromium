// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/index.h"
#include "chromeos/components/local_search_service/shared_structs.h"

namespace chromeos {
namespace local_search_service {

class InvertedIndex;

// An implementation of Index.
// A search via the inverted index backend with TF-IDF based document ranking.
class InvertedIndexSearch : public Index {
 public:
  InvertedIndexSearch(IndexId index_id, PrefService* local_state);
  ~InvertedIndexSearch() override;

  InvertedIndexSearch(const InvertedIndexSearch&) = delete;
  InvertedIndexSearch& operator=(const InvertedIndexSearch&) = delete;

  // Index overrides:
  uint64_t GetSize() override;
  // TODO(jiameng): we always build the index after documents are updated. May
  // revise this strategy if there is a different use case.
  void AddOrUpdate(const std::vector<Data>& data) override;
  // TODO(jiameng): we always build the index after documents are deleted. May
  // revise this strategy if there is a different use case.
  uint32_t Delete(const std::vector<std::string>& ids) override;
  void ClearIndex() override;
  // Returns matching results for a given query by approximately matching the
  // query with terms in the documents. Documents are ranked by TF-IDF scores.
  // Scores in results are positive but not guaranteed to be in any particular
  // range.
  ResponseStatus Find(const base::string16& query,
                      uint32_t max_results,
                      std::vector<Result>* results) override;

  // Returns document id and number of occurrences of |term|.
  // Document ids are sorted in alphabetical order.
  std::vector<std::pair<std::string, uint32_t>> FindTermForTesting(
      const base::string16& term) const;

 private:
  void OnExtractDocumentsContentDone(
      const std::vector<std::pair<std::string, std::vector<Token>>>& documents);

  std::unique_ptr<InvertedIndex> inverted_index_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InvertedIndexSearch> weak_ptr_factory_{this};
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
