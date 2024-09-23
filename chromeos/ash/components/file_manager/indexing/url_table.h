// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_URL_TABLE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_URL_TABLE_H_

#include <memory>
#include <optional>

#include "chromeos/ash/components/file_manager/indexing/text_table.h"
#include "sql/database.h"
#include "url/gurl.h"

namespace ash::file_manager {

// A table that maintains a mapping from a unique URL ID to the URL text.
// URLs represent location of a file. For example, for a local file we
// may have URL such as:
//
//   filesystem:chrome://file-manager/external/Downloads-user123/foo.txt
//
// This table is meant to be managed by the SqlStorage class.
class UrlTable : public TextTable {
 public:
  // Creates a new table and passes the pointer to the SQL database to it. The
  // caller must make sure it owns both the sql::Database object and this table.
  // The caller also must make sure that the sql::Database outlives the table.
  explicit UrlTable(sql::Database* db);
  ~UrlTable() override;

  UrlTable(const UrlTable&) = delete;
  UrlTable& operator=(const UrlTable&) = delete;

  // Deletes the given URL from the table. Returns -1, if the URL was not
  // found. Otherwise, returns the ID that the URL was assigned.
  int64_t DeleteUrl(const GURL& url);

  // Returns the ID for the given URL, or -1 if this URL has not been seen.
  int64_t GetUrlId(const GURL& url) const;

  // Gets or creates the URL ID for the given URL.
  int64_t GetOrCreateUrlId(const GURL& url);

  // For the given `url_id` attempts to find the corresponding URL spec.
  // If one cannot be found, returns -1. Otherwise returns `url_id` and fills
  // the `url_spec` with the found value.
  std::optional<std::string> GetUrlSpec(int64_t url_id) const;

  // Changes the URL from the `from` value to the `to` value, if the `from`
  // value exists. Returns the ID of the changed URL if the operation was
  // successful, or -1 otherwise.
  int64_t ChangeUrl(const GURL& from, const GURL& to);

 protected:
  std::unique_ptr<sql::Statement> MakeGetValueIdStatement() const override;
  std::unique_ptr<sql::Statement> MakeGetValueStatement() const override;
  std::unique_ptr<sql::Statement> MakeInsertStatement() const override;
  std::unique_ptr<sql::Statement> MakeDeleteStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateTableStatement() const override;
  std::unique_ptr<sql::Statement> MakeCreateIndexStatement() const override;
  std::unique_ptr<sql::Statement> MakeChangeValueStatement() const override;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_URL_TABLE_H_
