// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_POSTING_LIST_TABLE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_POSTING_LIST_TABLE_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "sql/database.h"

namespace ash::file_manager {

// Represents a posting list for terms. A typical posting list allows us to
// retrieve URL IDs for all files that contain some term. In other words,
// there is a map from an term ID to a set of URL IDs. For SQL we use a table
// with two columns, term_id and url_id. The pair (term_id, url_id) forms
// a unique key. In addition to this table we create two indexes. One arranged
// by term_id. This one allows for quick retrieval of all URL IDs that contain
// the given term. The other index is created on URL IDs. This one allows us
// to quickly retrieve all term IDs associated with the given URL (and thus
// file). This index allows us to quickly locate all entries that need to be
// removed when the file is deleted.
class PostingListTable {
 public:
  explicit PostingListTable(sql::Database* db);
  ~PostingListTable();

  PostingListTable(const PostingListTable&) = delete;
  PostingListTable& operator=(const PostingListTable&) = delete;

  // Initializes this table. Returns true if the initialization was successful.
  // False otherwise.
  bool Init();

  // Adds the given `url_id` to the posting list of the given
  // `term_id`. Returns true if successful, false otherwise.
  size_t AddToPostingList(int64_t term_id, int64_t url_id);

  // Deletes the given `url_id` to the posting list of the given
  // `term_id`. Returns true if successful, false otherwise.
  size_t DeleteFromPostingList(int64_t term_id, int64_t url_id);

  // For the given term ID it returns all known URL IDs that are associated
  // with that term.
  std::set<int64_t> GetUrlIdsForTerm(int64_t term_id) const;

  // For the given `url_id` returns all known term_ids associated
  // with it.
  const std::set<int64_t> GetTermIdsForUrl(int64_t url_id) const;

 private:
  raw_ptr<sql::Database> db_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_POSTING_LIST_TABLE_H_
