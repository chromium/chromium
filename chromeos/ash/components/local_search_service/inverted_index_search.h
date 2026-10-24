// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/local_search_service/index.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"

namespace ash::local_search_service {

class InvertedIndex;

// An implementation of Index.
// A search via the inverted index backend with TF-IDF based document ranking.
class InvertedIndexSearch : public Index {
 public:
  explicit InvertedIndexSearch(IndexId index_id);
  ~InvertedIndexSearch() override;

  InvertedIndexSearch(const InvertedIndexSearch&) = delete;
  InvertedIndexSearch& operator=(const InvertedIndexSearch&) = delete;

  // Index overrides:
  // GetSize is only accurate if the index has done updating.
  void GetSize(GetSizeCallback callback) override;
  void AddOrUpdate(const std::vector<Data>& data,
                   AddOrUpdateCallback callback) override;
  void Delete(const std::vector<std::string>& ids,
              DeleteCallback callback) override;
  void UpdateDocuments(const std::vector<Data>& data,
                       UpdateDocumentsCallback callback) override;
  void Find(const std::u16string& query,
            uint32_t max_results,
            FindCallback callback) override;
  void ClearIndex(ClearIndexCallback callback) override;
  uint32_t GetIndexSize() const override;

  // Returns document id and number of occurrences of |term|.
  // Document ids are sorted in alphabetical order.
  std::vector<std::pair<std::string, uint32_t>> FindTermForTesting(
      const std::u16string& term) const;

 private:
  void FinalizeAddOrUpdate(
      AddOrUpdateCallback callback,
      const std::vector<std::pair<std::string, std::vector<Token>>>& documents);

  // FinalizeDelete is called if Delete cannot be immediately done because
  // there's another index updating operation before it, i.e.
  // |num_queued_index_updates_| is not zero.
  void FinalizeDelete(DeleteCallback callback,
                      const std::vector<std::string>& ids);

  void FinalizeUpdateDocuments(
      UpdateDocumentsCallback callback,
      const std::vector<std::pair<std::string, std::vector<Token>>>& documents);

  std::unique_ptr<InvertedIndex> inverted_index_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InvertedIndexSearch> weak_ptr_factory_{this};
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INVERTED_INDEX_SEARCH_H_
