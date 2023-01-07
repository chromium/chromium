// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_LINEAR_MAP_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_LINEAR_MAP_SEARCH_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chromeos/ash/components/local_search_service/index.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"

namespace ash::string_matching {
class TokenizedString;
}

namespace ash::local_search_service {

// A map from key to a vector of (tag-id, tokenized tag).
typedef std::map<
    std::string,
    std::vector<std::pair<std::string,
                          std::unique_ptr<string_matching::TokenizedString>>>>
    KeyToTagVector;

// An implementation of Index.
// A search backend that linearly scans all documents in the storage and finds
// documents that match the input query. Search is done by matching query with
// documents' search tags.
class LinearMapSearch : public Index {
 public:
  explicit LinearMapSearch(IndexId index_id);
  ~LinearMapSearch() override;

  LinearMapSearch(const LinearMapSearch&) = delete;
  LinearMapSearch& operator=(const LinearMapSearch&) = delete;

  // Index overrides:
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

 private:
  // Returns all search results for a given query.
  std::vector<Result> GetSearchResults(const std::u16string& query,
                                       uint32_t max_results) const;

  KeyToTagVector data_;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_LINEAR_MAP_SEARCH_H_
