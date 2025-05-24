// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_KEYWORD_TABLE_H_
#define COMPONENTS_SEARCH_ENGINES_KEYWORD_TABLE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/template_url_id.h"
#include "components/webdata/common/web_database_table.h"

struct TemplateURLData;
class WebDatabase;

namespace sql {
class Statement;
}  // namespace sql

namespace features {
// An emergency 'off switch' to disable hash verification.
// TODO(crbug.com/376303929): Remove in M134.
BASE_DECLARE_FEATURE(kKeywordTableHashVerification);
}  // namespace features

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
//   safe_for_autoreplace   This is set to false for any entry that was manually
//                          added or edited by the user.
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURLData::prepopulate_id.
//   created_by_policy      See TemplateURLData::policy_origin.  This was
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
//   is_active              See TemplateURLData::is_active. This was added
//                          in version 97.
//   starter_pack_id        See TemplateURLData::starter_pack_id. This was added
//                          in version 103.
//   enforced_by_policy     See TemplateURLData::enforced_by_policy. This was
//                          added in version 112.
//   featured_by_policy     See TemplateURLData::featured_by_policy. This was
//                          added in version 122.
//   url_hash               An encrypted hash of the url and id fields used to
//                          detect database inconsistency. Added in version 137.
//                          This can be NULL if no encryption services are
//                          available.
//
// This class also manages some fields in the |meta| table:
//
// Builtin Keyword Version           The version of builtin keywords data.
// Starter Pack Keyword Version      The version of starter pack data.
// Builtin Keyword Country           The country associated with the builtin
//                                   keywords data, stored as a country ID.
// Builtin Keyword Milestone         The version number of Chrome milestone when
//                                   the keyword data has been last merged into
//                                   the database. Written between Chrome M122
//                                   and M129.
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

  KeywordTable(const KeywordTable&) = delete;
  KeywordTable& operator=(const KeywordTable&) = delete;

  ~KeywordTable() override;

  // Retrieves the KeywordTable* owned by |database|.
  static KeywordTable* FromWebDatabase(WebDatabase* db);

  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
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

  // Version of the built-in keyword data. It gets set from the
  // `TemplateURLPrepopulateData::kCurrentDataVersion` when the data was
  // last updated.
  bool SetBuiltinKeywordDataVersion(int version);
  int GetBuiltinKeywordDataVersion();

  // Chrome milestone when the built-in keywords were last updated.
  bool ClearBuiltinKeywordMilestone();

  // Country associated with the built-in keywords, stored as a country ID,
  // see `country_codes::CountryId()`.
  bool SetBuiltinKeywordCountry(country_codes::CountryId country_id);
  country_codes::CountryId GetBuiltinKeywordCountry();

  // Version of built-in starter pack keywords (@bookmarks, @settings, etc.).
  bool SetStarterPackKeywordVersion(int version);
  int GetStarterPackKeywordVersion();

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
  bool MigrateToVersion97AddIsActiveColumn();
  bool MigrateToVersion103AddStarterPackIdColumn();
  bool MigrateToVersion112AddEnforcedByPolicyColumn();
  bool MigrateToVersion122AddSiteSearchPolicyColumns();
  bool MigrateToVersion137AddHashColumn();

 private:
  friend class KeywordTableTest;

  // Returns a TemplateURLData with the data in `s`. Returns std::nullopt if we
  // couldn't fill for some reason, e.g. `s` tried to set one of the fields to
  // an illegal value.
  std::optional<TemplateURLData> GetKeywordDataFromStatement(sql::Statement& s);

  // Inserts the data from `data` into `s`. `s` is assumed to have slots for all
  // the columns in the keyword table. `id_column` is the slot number to bind
  // `data`'s `id` to; `starting_column` is the slot number of the first of a
  // contiguous set of slots to bind all the other fields to.
  void BindURLToStatement(const TemplateURLData& data,
                          sql::Statement* s,
                          int id_column,
                          int starting_column);

  // Adds a new keyword to the keyword table. Returns true if successful.
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
};

#endif  // COMPONENTS_SEARCH_ENGINES_KEYWORD_TABLE_H_
