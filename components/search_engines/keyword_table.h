// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_KEYWORD_TABLE_H_
#define COMPONENTS_SEARCH_ENGINES_KEYWORD_TABLE_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/search_engines/template_url_id.h"
#include "components/webdata/common/web_database_table.h"

struct TemplateURLData;
class WebDatabase;

namespace sql {
class Statement;
}  // namespace sql

// This class manages the |keywords| MetaTable within the SQLite database
// passed to the constructor. It expects the following schema:
//
// Note: The database stores time in seconds, UTC.
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURLData.  See that struct for more details.
//   id
//   short_name
//   keyword
//   favicon_url
//   url
//   safe_for_autoreplace
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURLData::prepopulate_id.
//   created_by_policy      See TemplateURLData::created_by_policy.  This was
//                          added in version 26.
//   last_modified          See TemplateURLData::last_modified.  This was added
//                          in version 38.
//   sync_guid              See TemplateURLData::sync_guid. This was added in
//                          version 39.
//   alternate_urls         See TemplateURLData::alternate_urls. This was added
//                          in version 47.
//   image_url              See TemplateURLData::image_url. This was added in
//                          version 52.
//   search_url_post_params See TemplateURLData::search_url_post_params. This
//                          was added in version 52.
//   suggest_url_post_params See TemplateURLData::suggestions_url_post_params.
//                          This was added in version 52.
//   image_url_post_params  See TemplateURLData::image_url_post_params. This
//                          was added in version 52.
//   new_tab_url            See TemplateURLData::new_tab_url. This was added in
//                          version 53.
//   last_visited           See TemplateURLData::last_visited. This was added in
//                          version 69.
//   created_from_play_api  See TemplateURLData::created_from_play_api. This was
//                          added in version 82.
//
// This class also manages some fields in the |meta| table:
//
// Default Search Provider ID        The id of the default search provider.
// Builtin Keyword Version           The version of builtin keywords data.
//
class KeywordTable : public WebDatabaseTable {
 public:
  enum OperationType {
    ADD,
    REMOVE,
    UPDATE,
  };

  typedef std::pair<OperationType, TemplateURLData> Operation;
  typedef std::vector<Operation> Operations;
  typedef std::vector<TemplateURLData> Keywords;

  // Constants exposed for the benefit of test code:

  static const char kDefaultSearchProviderKey[];

  KeywordTable();
  ~KeywordTable() override;

  // Retrieves the KeywordTable* owned by |database|.
  static KeywordTable* FromWebDatabase(WebDatabase* db);

  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool IsSyncable() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Performs an arbitrary number of Add/Remove/Update operations as a single
  // transaction.  This is provided for efficiency reasons: if the caller needs
  // to perform a large number of operations, doing them in a single transaction
  // instead of one-per-transaction can be dramatically more efficient.
  bool PerformOperations(const Operations& operations);

  // Loads the keywords into the specified vector. It's up to the caller to
  // delete the returned objects.
  // Returns true on success.
  bool GetKeywords(Keywords* keywords);

  // ID (TemplateURLData->id) of the default search provider.
  bool SetDefaultSearchProviderID(int64_t id);
  int64_t GetDefaultSearchProviderID();

  // Version of the built-in keywords.
  bool SetBuiltinKeywordVersion(int version);
  int GetBuiltinKeywordVersion();

  // Returns a comma-separated list of the keyword columns for the current
  // version of the table.
  static std::string GetKeywordColumns();

  // Table migration functions.
  bool MigrateToVersion53AddNewTabURLColumn();
  bool MigrateToVersion59RemoveExtensionKeywords();
  bool MigrateToVersion68RemoveShowInDefaultListColumn();
  bool MigrateToVersion69AddLastVisitedColumn();
  bool MigrateToVersion76RemoveInstantColumns();
  bool MigrateToVersion77IncreaseTimePrecision();
  bool MigrateToVersion82AddCreatedFromPlayApiColumn();

 private:
  friend class KeywordTableTest;

  // NOTE: Since the table columns have changed in different versions, many
  // functions below take a |table_version| argument which dictates which
  // version number's column set to use.

  // Fills |data| with the data in |s|.  Returns false if we couldn't fill
  // |data| for some reason, e.g. |s| tried to set one of the fields to an
  // illegal value.
  static bool GetKeywordDataFromStatement(const sql::Statement& s,
                                          TemplateURLData* data);

  // Adds a new keyword, updating the id field on success.
  // Returns true if successful.
  bool AddKeyword(const TemplateURLData& data);

  // Removes the specified keyword.
  // Returns true if successful.
  bool RemoveKeyword(TemplateURLID id);

  // Updates the database values for the specified url.
  // Returns true on success.
  bool UpdateKeyword(const TemplateURLData& data);

  // Gets a string representation for keyword with id specified.
  // Used to store its result in |meta| table or to compare with another
  // keyword. Returns true on success, false otherwise.
  bool GetKeywordAsString(TemplateURLID id,
                          const std::string& table_name,
                          std::string* result);

  DISALLOW_COPY_AND_ASSIGN(KeywordTable);
};

#endif  // COMPONENTS_SEARCH_ENGINES_KEYWORD_TABLE_H_
