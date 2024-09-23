// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_RAM_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_RAM_STORAGE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/file_manager/indexing/file_info.h"
#include "chromeos/ash/components/file_manager/indexing/index_storage.h"
#include "chromeos/ash/components/file_manager/indexing/query.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "url/gurl.h"

namespace ash::file_manager {

// An in-memory implementation of the file index. Nothing is persisted. All data
// is kept in various maps.
class COMPONENT_EXPORT(FILE_MANAGER) RamStorage : public IndexStorage {
 public:
  RamStorage();
  ~RamStorage() override;

  RamStorage(const RamStorage&) = delete;
  RamStorage& operator=(const RamStorage&) = delete;

  // RAM implementation of IndexStorage methods.

  // Inverted index and plain index functions.
  const std::set<int64_t> GetUrlIdsForTermId(int64_t term_id) const override;
  const std::set<int64_t> GetTermIdsForUrl(int64_t url_id) const override;

  // Posting list support.
  size_t AddToPostingList(int64_t term_id, int64_t url_id) override;
  size_t DeleteFromPostingList(int64_t term_id, int64_t url_id) override;

  // Term ID management.
  int64_t GetTermId(const Term& term) const override;
  int64_t GetOrCreateTermId(const Term& term) override;

  // Token ID management.
  int64_t GetTokenId(const std::string& token_bytes) const override;
  int64_t GetOrCreateTokenId(const std::string& token0_bytes) override;

  // URL ID management.
  int64_t GetUrlId(const GURL& url) const override;
  int64_t GetOrCreateUrlId(const GURL& url) override;
  int64_t MoveUrl(const GURL& from, const GURL& to) override;
  int64_t DeleteUrl(const GURL& url) override;

  // FileInfo management.
  int64_t PutFileInfo(const FileInfo& file_info) override;
  std::optional<FileInfo> GetFileInfo(int64_t url_id) const override;
  int64_t DeleteFileInfo(int64_t url_id) override;

  // Miscellaneous.
  size_t AddTermIdsForUrl(const std::set<int64_t>& term_ids,
                          int64_t url_id) override;
  size_t DeleteTermIdsForUrl(const std::set<int64_t>& term_ids,
                             int64_t url_id) override;

 private:
  // Adds to the inverted posting lists the specified `term_id`. This may be
  // a no-op if the given term has previously been associated with the file
  // info ID.
  void AddToPlainIndex(int64_t url_id, int64_t term_id);

  // Removes the given `term_id` from the inverted posting lists of the
  // specified `url_id`. This may be a no-op if the term_id is not present
  // on the term list for the given `url_id`.
  void DeleteFromPlainIndex(int64_t url_id, int64_t term_id);

  // Maps from stringified tokens to a unique ID.
  std::map<std::string, int64_t> token_map_;
  int64_t token_id_ = 0;

  // Maps field and token ID to a single term ID. It uses token_id rather than
  // token value to minimize memory usage.
  std::map<std::tuple<std::string, int64_t>, int64_t> term_map_;
  int64_t term_id_ = 0;

  // Maps a file URL to a unique ID. The GURL is the data uniquely identifying
  // a file. Hence we use the GURL rather than the whole FileInfo. For example,
  // if the size of the file changes, it does not have consequences on this
  // index.
  std::map<GURL, int64_t> url_to_id_;
  int64_t url_id_ = 0;

  // Maps url_id to the corresponding FileInfo.
  std::map<int64_t, FileInfo> url_id_to_file_info_;

  // A posting list, which is a map from an term ID to a set of all
  // URL IDs that represent files that has this term ID associated with them.
  std::map<int64_t, std::set<int64_t>> posting_lists_;

  // A map from URL ID to term IDs that are stored for a given file.
  // This works like a plain index (mapping from URL ID to all terms known
  // for that URL ID).
  std::map<int64_t, std::set<int64_t>> plain_index_;

  // A pre-allocated empty ID set, returned when we have no ID set available.
  const std::set<int64_t> empty_id_set_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_RAM_STORAGE_H_
