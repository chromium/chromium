// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/keyword_table.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string_view>
#include <tuple>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/database_utils/url_converter.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

using base::Time;

// static
constexpr char KeywordTable::kDefaultSearchProviderKey[] =
    "Default Search Provider ID";

namespace {

// Keys used in the meta table.
constexpr char kBuiltinKeywordDataVersion[] = "Builtin Keyword Version";
constexpr char kBuiltinKeywordMilestone[] = "Builtin Keyword Milestone";
constexpr char kBuiltinKeywordCountry[] = "Builtin Keyword Country";
constexpr char kStarterPackKeywordVersion[] = "Starter Pack Keyword Version";

const std::string ColumnsForVersion(int version, bool concatenated) {
  std::vector<std::string_view> columns;

  columns.push_back("id");
  columns.push_back("short_name");
  columns.push_back("keyword");
  columns.push_back("favicon_url");
  columns.push_back("url");
  columns.push_back("safe_for_autoreplace");
  columns.push_back("originating_url");
  columns.push_back("date_created");
  columns.push_back("usage_count");
  columns.push_back("input_encodings");
  if (version <= 67) {
    // Column removed after version 67.
    columns.push_back("show_in_default_list");
  }
  columns.push_back("suggest_url");
  columns.push_back("prepopulate_id");
  if (version <= 44) {
    // Columns removed after version 44.
    columns.push_back("autogenerate_keyword");
    columns.push_back("logo_id");
  }
  columns.push_back("created_by_policy");
  if (version <= 75) {
    // Column removed after version 75.
    columns.push_back("instant_url");
  }
  columns.push_back("last_modified");
  columns.push_back("sync_guid");
  if (version >= 47) {
    // Column added in version 47.
    columns.push_back("alternate_urls");
  }
  if (version >= 49 && version <= 75) {
    // Column added in version 49 and removed after version 75.
    columns.push_back("search_terms_replacement_key");
  }
  if (version >= 52) {
    // Column added in version 52.
    columns.push_back("image_url");
    columns.push_back("search_url_post_params");
    columns.push_back("suggest_url_post_params");
  }
  if (version >= 52 && version <= 75) {
    // Column added in version 52 and removed after version 75.
    columns.push_back("instant_url_post_params");
  }
  if (version >= 52) {
    // Column added in version 52.
    columns.push_back("image_url_post_params");
  }
  if (version >= 53) {
    // Column added in version 53.
    columns.push_back("new_tab_url");
  }
  if (version >= 69) {
    // Column added in version 69.
    columns.push_back("last_visited");
  }
  if (version >= 82) {
    // Column added in version 82.
    columns.push_back("created_from_play_api");
  }
  if (version >= 97) {
    // Column added in version 97.
    columns.push_back("is_active");
  }
  if (version >= 103) {
    // Column added in version 103.
    columns.push_back("starter_pack_id");
  }
  if (version >= 112) {
    // Column added in version 112.
    columns.push_back("enforced_by_policy");
  }
  if (version >= 122) {
    // Column added in version 122.
    columns.push_back("featured_by_policy");
  }

  return base::JoinString(columns, std::string(concatenated ? " || " : ", "));
}

// Inserts the data from |data| into |s|.  |s| is assumed to have slots for all
// the columns in the keyword table.  |id_column| is the slot number to bind
// |data|'s |id| to; |starting_column| is the slot number of the first of a
// contiguous set of slots to bind all the other fields to.
void BindURLToStatement(const TemplateURLData& data,
                        sql::Statement* s,
                        int id_column,
                        int starting_column) {
  // Serialize |alternate_urls| to JSON.
  // TODO(beaudoin): Check what it would take to use a new table to store
  // alternate_urls while keeping backups and table signature in a good state.
  // See: crbug.com/153520
  base::Value::List alternate_urls_value;
  for (size_t i = 0; i < data.alternate_urls.size(); ++i)
    alternate_urls_value.Append(data.alternate_urls[i]);
  std::string alternate_urls;
  base::JSONWriter::Write(alternate_urls_value, &alternate_urls);

  s->BindInt64(id_column, data.id);
  s->BindString16(starting_column, data.short_name());
  s->BindString16(starting_column + 1, data.keyword());
  s->BindString(starting_column + 2,
                data.favicon_url.is_valid()
                    ? database_utils::GurlToDatabaseUrl(data.favicon_url)
                    : std::string());
  s->BindString(starting_column + 3, data.url());
  s->BindBool(starting_column + 4, data.safe_for_autoreplace);
  s->BindString(starting_column + 5,
                data.originating_url.is_valid()
                    ? database_utils::GurlToDatabaseUrl(data.originating_url)
                    : std::string());
  s->BindTime(starting_column + 6, data.date_created);
  s->BindInt(starting_column + 7, data.usage_count);
  s->BindString(starting_column + 8,
                base::JoinString(data.input_encodings, ";"));
  s->BindString(starting_column + 9, data.suggestions_url);
  s->BindInt(starting_column + 10, data.prepopulate_id);
  s->BindInt(starting_column + 11, static_cast<int>(data.created_by_policy));
  s->BindTime(starting_column + 12, data.last_modified);
  s->BindString(starting_column + 13, data.sync_guid);
  s->BindString(starting_column + 14, alternate_urls);
  s->BindString(starting_column + 15, data.image_url);
  s->BindString(starting_column + 16, data.search_url_post_params);
  s->BindString(starting_column + 17, data.suggestions_url_post_params);
  s->BindString(starting_column + 18, data.image_url_post_params);
  s->BindString(starting_column + 19, data.new_tab_url);
  s->BindTime(starting_column + 20, data.last_visited);
  s->BindBool(starting_column + 21, data.created_from_play_api);
  s->BindInt(starting_column + 22, static_cast<int>(data.is_active));
  s->BindInt(starting_column + 23, data.starter_pack_id);
  s->BindBool(starting_column + 24, data.enforced_by_policy);
  s->BindBool(starting_column + 25, data.featured_by_policy);
}

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

KeywordTable::KeywordTable() {
}

KeywordTable::~KeywordTable() = default;

KeywordTable* KeywordTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<KeywordTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey KeywordTable::GetTypeKey() const {
  return GetKey();
}

bool KeywordTable::CreateTablesIfNecessary() {
  return db()->DoesTableExist("keywords") ||
         db()->Execute(
             "CREATE TABLE keywords ("
             "id INTEGER PRIMARY KEY,"
             "short_name VARCHAR NOT NULL,"
             "keyword VARCHAR NOT NULL,"
             "favicon_url VARCHAR NOT NULL,"
             "url VARCHAR NOT NULL,"
             "safe_for_autoreplace INTEGER,"
             "originating_url VARCHAR,"
             "date_created INTEGER DEFAULT 0,"
             "usage_count INTEGER DEFAULT 0,"
             "input_encodings VARCHAR,"
             "suggest_url VARCHAR,"
             "prepopulate_id INTEGER DEFAULT 0,"
             "created_by_policy INTEGER DEFAULT 0,"
             "last_modified INTEGER DEFAULT 0,"
             "sync_guid VARCHAR,"
             "alternate_urls VARCHAR,"
             "image_url VARCHAR,"
             "search_url_post_params VARCHAR,"
             "suggest_url_post_params VARCHAR,"
             "image_url_post_params VARCHAR,"
             "new_tab_url VARCHAR,"
             "last_visited INTEGER DEFAULT 0, "
             "created_from_play_api INTEGER DEFAULT 0, "
             "is_active INTEGER DEFAULT 0, "
             "starter_pack_id INTEGER DEFAULT 0, "
             "enforced_by_policy INTEGER DEFAULT 0, "
             "featured_by_policy INTEGER DEFAULT 0)");
}

bool KeywordTable::MigrateToVersion(int version,
                                    bool* update_compatible_version) {
  // Migrate if necessary.
  switch (version) {
    case 53:
      *update_compatible_version = true;
      return MigrateToVersion53AddNewTabURLColumn();
    case 59:
      *update_compatible_version = true;
      return MigrateToVersion59RemoveExtensionKeywords();
    case 68:
      *update_compatible_version = true;
      return MigrateToVersion68RemoveShowInDefaultListColumn();
    case 69:
      return MigrateToVersion69AddLastVisitedColumn();
    case 76:
      *update_compatible_version = true;
      return MigrateToVersion76RemoveInstantColumns();
    case 77:
      *update_compatible_version = true;
      return MigrateToVersion77IncreaseTimePrecision();
    case 82:
      return MigrateToVersion82AddCreatedFromPlayApiColumn();
    case 97:
      return MigrateToVersion97AddIsActiveColumn();
    case 103:
      return MigrateToVersion103AddStarterPackIdColumn();
    case 112:
      return MigrateToVersion112AddEnforcedByPolicyColumn();
    case 122:
      return MigrateToVersion122AddSiteSearchPolicyColumns();
  }

  return true;
}

bool KeywordTable::PerformOperations(const Operations& operations) {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;

  for (auto i(operations.begin()); i != operations.end(); ++i) {
    switch (i->first) {
      case ADD:
        if (!AddKeyword(i->second))
          return false;
        break;

      case REMOVE:
        if (!RemoveKeyword(i->second.id))
          return false;
        break;

      case UPDATE:
        if (!UpdateKeyword(i->second))
          return false;
        break;
    }
  }

  return transaction.Commit();
}

bool KeywordTable::GetKeywords(Keywords* keywords) {
  const std::string query = base::StrCat(
      {"SELECT ", GetKeywordColumns(), " FROM keywords ORDER BY id ASC"});
  sql::Statement s(db()->GetUniqueStatement(query));

  std::set<TemplateURLID> bad_entries;
  while (s.Step()) {
    keywords->push_back(TemplateURLData());
    if (!GetKeywordDataFromStatement(s, &keywords->back())) {
      bad_entries.insert(s.ColumnInt64(0));
      keywords->pop_back();
    }
  }
  bool succeeded = s.Succeeded();
  for (auto i(bad_entries.begin()); i != bad_entries.end(); ++i)
    succeeded &= RemoveKeyword(*i);
  return succeeded;
}

bool KeywordTable::SetDefaultSearchProviderID(int64_t id) {
  return meta_table()->SetValue(kDefaultSearchProviderKey, id);
}

int64_t KeywordTable::GetDefaultSearchProviderID() {
  int64_t value = kInvalidTemplateURLID;
  meta_table()->GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool KeywordTable::SetBuiltinKeywordDataVersion(int version) {
  return meta_table()->SetValue(kBuiltinKeywordDataVersion, version);
}

int KeywordTable::GetBuiltinKeywordDataVersion() {
  int version = 0;
  return meta_table()->GetValue(kBuiltinKeywordDataVersion, &version) ? version
                                                                      : 0;
}

bool KeywordTable::ClearBuiltinKeywordMilestone() {
  return meta_table()->DeleteKey(kBuiltinKeywordMilestone);
}

bool KeywordTable::SetBuiltinKeywordCountry(int country_id) {
  return meta_table()->SetValue(kBuiltinKeywordCountry, country_id);
}

int KeywordTable::GetBuiltinKeywordCountry() {
  int country_id = 0;
  return meta_table()->GetValue(kBuiltinKeywordCountry, &country_id)
             ? country_id
             : 0;
}

bool KeywordTable::SetStarterPackKeywordVersion(int version) {
  return meta_table()->SetValue(kStarterPackKeywordVersion, version);
}

int KeywordTable::GetStarterPackKeywordVersion() {
  int version = 0;
  return meta_table()->GetValue(kStarterPackKeywordVersion, &version) ? version
                                                                      : 0;
}

// static
std::string KeywordTable::GetKeywordColumns() {
  return ColumnsForVersion(WebDatabase::kCurrentVersionNumber, false);
}

bool KeywordTable::MigrateToVersion53AddNewTabURLColumn() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN new_tab_url "
      "VARCHAR DEFAULT ''");
}

bool KeywordTable::MigrateToVersion59RemoveExtensionKeywords() {
  return db()->Execute(
      "DELETE FROM keywords "
      "WHERE url LIKE 'chrome-extension://%'");
}

// SQLite does not support DROP COLUMN operation. So A new table is created
// without the show_in_default_list column. Data from all but the dropped column
// of the old table is copied into it. After that, the old table is dropped and
// the new table is renamed to it.
bool KeywordTable::MigrateToVersion68RemoveShowInDefaultListColumn() {
  sql::Transaction transaction(db());
  const std::string query_str =
      base::StrCat({"INSERT INTO temp_keywords SELECT ",
                    ColumnsForVersion(68, false), " FROM keywords"});
  return transaction.Begin() &&
         db()->Execute(
             "CREATE TABLE temp_keywords ("
             "id INTEGER PRIMARY KEY,"
             "short_name VARCHAR NOT NULL,"
             "keyword VARCHAR NOT NULL,"
             "favicon_url VARCHAR NOT NULL,"
             "url VARCHAR NOT NULL,"
             "safe_for_autoreplace INTEGER,"
             "originating_url VARCHAR,"
             "date_created INTEGER DEFAULT 0,"
             "usage_count INTEGER DEFAULT 0,"
             "input_encodings VARCHAR,"
             "suggest_url VARCHAR,"
             "prepopulate_id INTEGER DEFAULT 0,"
             "created_by_policy INTEGER DEFAULT 0,"
             "instant_url VARCHAR,"
             "last_modified INTEGER DEFAULT 0,"
             "sync_guid VARCHAR,"
             "alternate_urls VARCHAR,"
             "search_terms_replacement_key VARCHAR,"
             "image_url VARCHAR,"
             "search_url_post_params VARCHAR,"
             "suggest_url_post_params VARCHAR,"
             "instant_url_post_params VARCHAR,"
             "image_url_post_params VARCHAR,"
             "new_tab_url VARCHAR)") &&
         db()->Execute(query_str) && db()->Execute("DROP TABLE keywords") &&
         db()->Execute("ALTER TABLE temp_keywords RENAME TO keywords") &&
         transaction.Commit();
}

bool KeywordTable::MigrateToVersion69AddLastVisitedColumn() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN last_visited "
      "INTEGER DEFAULT 0");
}

// SQLite does not support DROP COLUMN operation. So a new table is created
// without the removed columns. Data from all but the dropped columns of the old
// table is copied into it. After that, the old table is dropped and the new
// table is renamed to it.
bool KeywordTable::MigrateToVersion76RemoveInstantColumns() {
  sql::Transaction transaction(db());
  const std::string query_str =
      base::StrCat({"INSERT INTO temp_keywords SELECT ",
                    ColumnsForVersion(76, false), " FROM keywords"});
  return transaction.Begin() &&
         db()->Execute(
             "CREATE TABLE temp_keywords ("
             "id INTEGER PRIMARY KEY,"
             "short_name VARCHAR NOT NULL,"
             "keyword VARCHAR NOT NULL,"
             "favicon_url VARCHAR NOT NULL,"
             "url VARCHAR NOT NULL,"
             "safe_for_autoreplace INTEGER,"
             "originating_url VARCHAR,"
             "date_created INTEGER DEFAULT 0,"
             "usage_count INTEGER DEFAULT 0,"
             "input_encodings VARCHAR,"
             "suggest_url VARCHAR,"
             "prepopulate_id INTEGER DEFAULT 0,"
             "created_by_policy INTEGER DEFAULT 0,"
             "last_modified INTEGER DEFAULT 0,"
             "sync_guid VARCHAR,"
             "alternate_urls VARCHAR,"
             "image_url VARCHAR,"
             "search_url_post_params VARCHAR,"
             "suggest_url_post_params VARCHAR,"
             "image_url_post_params VARCHAR,"
             "new_tab_url VARCHAR,"
             "last_visited INTEGER DEFAULT 0)") &&
         db()->Execute(query_str) && db()->Execute("DROP TABLE keywords") &&
         db()->Execute("ALTER TABLE temp_keywords RENAME TO keywords") &&
         transaction.Commit();
}

bool KeywordTable::MigrateToVersion77IncreaseTimePrecision() {
  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return false;

  static constexpr char kQuery[] =
      "SELECT id, date_created, last_modified, last_visited FROM keywords";
  sql::Statement s(db()->GetUniqueStatement(kQuery));
  std::vector<std::tuple<TemplateURLID, Time, Time, Time>> updates;
  while (s.Step()) {
    updates.emplace_back(std::make_tuple(s.ColumnInt64(0), s.ColumnTime(1),
                                         s.ColumnTime(2), s.ColumnTime(3)));
  }
  if (!s.Succeeded())
    return false;

  for (auto tuple : updates) {
    sql::Statement update_statement(db()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE keywords SET date_created = ?, last_modified = ?, last_visited "
        "= ? WHERE id = ? "));
    update_statement.BindTime(0, std::get<1>(tuple));
    update_statement.BindTime(1, std::get<2>(tuple));
    update_statement.BindTime(2, std::get<3>(tuple));
    update_statement.BindInt64(3, std::get<0>(tuple));
    if (!update_statement.Run()) {
      return false;
    }
  }
  return transaction.Commit();
}

bool KeywordTable::MigrateToVersion82AddCreatedFromPlayApiColumn() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN created_from_play_api INTEGER DEFAULT "
      "0");
}

bool KeywordTable::MigrateToVersion97AddIsActiveColumn() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN is_active INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion103AddStarterPackIdColumn() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN starter_pack_id INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion112AddEnforcedByPolicyColumn() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN enforced_by_policy INTEGER DEFAULT 0");
}

bool KeywordTable::MigrateToVersion122AddSiteSearchPolicyColumns() {
  return db()->Execute(
      "ALTER TABLE keywords ADD COLUMN featured_by_policy INTEGER DEFAULT 0");
}

// static
bool KeywordTable::GetKeywordDataFromStatement(sql::Statement& s,
                                               TemplateURLData* data) {
  DCHECK(data);

  data->SetShortName(s.ColumnString16(1));
  data->SetKeyword(s.ColumnString16(2));
  // Due to past bugs, we might have persisted entries with empty URLs.  Avoid
  // reading these out.  (GetKeywords() will delete these entries on return.)
  // NOTE: This code should only be needed as long as we might be reading such
  // potentially-old data and can be removed afterward.
  if (s.ColumnString(4).empty())
    return false;
  data->SetURL(s.ColumnString(4));
  data->suggestions_url = s.ColumnString(10);
  data->image_url = s.ColumnString(16);
  data->new_tab_url = s.ColumnString(20);
  data->search_url_post_params = s.ColumnString(17);
  data->suggestions_url_post_params = s.ColumnString(18);
  data->image_url_post_params = s.ColumnString(19);
  data->favicon_url = GURL(s.ColumnString(3));
  data->originating_url = GURL(s.ColumnString(6));
  data->safe_for_autoreplace = s.ColumnBool(5);
  data->input_encodings = base::SplitString(
      s.ColumnString(9), ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  data->id = s.ColumnInt64(0);
  data->date_created = s.ColumnTime(7);
  data->last_modified = s.ColumnTime(13);
  data->created_by_policy =
      static_cast<TemplateURLData::CreatedByPolicy>(s.ColumnInt(12));
  data->created_from_play_api = s.ColumnBool(22);
  data->usage_count = s.ColumnInt(8);
  data->prepopulate_id = s.ColumnInt(11);
  data->sync_guid = s.ColumnString(14);
  data->is_active = static_cast<TemplateURLData::ActiveStatus>(s.ColumnInt(23));
  data->starter_pack_id = s.ColumnInt(24);
  data->enforced_by_policy = s.ColumnBool(25);
  data->featured_by_policy = s.ColumnBool(26);

  data->alternate_urls.clear();
  std::optional<base::Value> value(base::JSONReader::Read(s.ColumnString(15)));
  if (value && value->is_list()) {
    for (const base::Value& alternate_url : value->GetList()) {
      if (alternate_url.is_string()) {
        data->alternate_urls.push_back(alternate_url.GetString());
      }
    }
  }

  data->last_visited = s.ColumnTime(21);

  return true;
}

bool KeywordTable::AddKeyword(const TemplateURLData& data) {
  DCHECK(data.id);
  const std::string query = base::StrCat(
      {"INSERT INTO keywords (", GetKeywordColumns(),
       ") "
       "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"});
  sql::Statement s(db()->GetCachedStatement(SQL_FROM_HERE, query));
  BindURLToStatement(data, &s, 0, 1);

  return s.Run();
}

bool KeywordTable::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(db()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM keywords WHERE id = ?"));
  s.BindInt64(0, id);

  return s.Run();
}

bool KeywordTable::UpdateKeyword(const TemplateURLData& data) {
  DCHECK(data.id);
  sql::Statement s(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE keywords SET short_name=?, keyword=?, favicon_url=?, url=?, "
      "safe_for_autoreplace=?, originating_url=?, date_created=?, "
      "usage_count=?, input_encodings=?, suggest_url=?, prepopulate_id=?, "
      "created_by_policy=?, last_modified=?, sync_guid=?, alternate_urls=?, "
      "image_url=?, search_url_post_params=?, suggest_url_post_params=?, "
      "image_url_post_params=?, new_tab_url=?, last_visited=?, "
      "created_from_play_api=?, is_active=?, starter_pack_id=?, "
      "enforced_by_policy=?, featured_by_policy=? WHERE id=?"));
  BindURLToStatement(data, &s, 26, 0);  // "26" binds id() as the last item.

  return s.Run();
}

bool KeywordTable::GetKeywordAsString(TemplateURLID id,
                                      const std::string& table_name,
                                      std::string* result) {
  const std::string query = base::StrCat(
      {"SELECT ", ColumnsForVersion(WebDatabase::kCurrentVersionNumber, true),
       " FROM ", table_name, " WHERE id=?"});
  sql::Statement s(db()->GetUniqueStatement(query));
  s.BindInt64(0, id);

  if (!s.Step()) {
    LOG_IF(WARNING, s.Succeeded()) << "No keyword with id: " << id
                                   << ", ignoring.";
    return true;
  }

  if (!s.Succeeded())
    return false;

  *result = s.ColumnString(0);
  return true;
}
