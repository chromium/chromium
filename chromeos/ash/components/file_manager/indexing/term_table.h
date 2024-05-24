// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TERM_TABLE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TERM_TABLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "sql/database.h"

namespace ash::file_manager {

// Stores a mapping from term IDs to an terms. A term term is a combination of
// a token and a field name. The main job of this table is to provide a unique
// ID for, say "label:starred" and "content:starred" terms. The token table
// provides a unique value for "starred". However, we need to be able to
// distinguish between "starred" being used a, say, label, vs it being part of
// a content. This is what this table does.
class TermTable {
 public:
  // Creates a table that maps term IDs to terms. An term consists of the field
  // name and a token ID.
  explicit TermTable(sql::Database* db);
  ~TermTable();

  TermTable(const TermTable&) = delete;
  TermTable& operator=(const TermTable&) = delete;

  // Initializes the table. Returns true on success, and false on failure.
  bool Init();

  // Returns the ID corresponding to the given term. If the term cannot be
  // located, the method returns -1.
  int64_t GetTermId(const std::string& field_name, int64_t term_id) const;

  // Returns the ID corresponding to the term. If the term cannot be located,
  // a new ID is allocated and returned.
  int64_t GetOrCreateTermId(const std::string& field_name, int64_t term_id);

  // Attempts to remove the given term by its ID from the database. If not
  // present, this method returns -1. Otherwise, it returns the `term_id`.
  int64_t DeleteTermById(int64_t term_id);

 private:
  // The pointer to a database owned by the whoever created this table.
  raw_ptr<sql::Database> db_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TERM_TABLE_H_
