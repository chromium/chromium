// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/sql_table_builder.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_switches.h"
#include "components/password_manager/core/browser/password_store/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_store/password_notes_table.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_IOS)
#import <Security/Security.h>
#endif  // BUILDFLAG(IS_IOS)

using signin::GaiaIdHash;

namespace password_manager {

// The current version number of the login database schema.
constexpr int kCurrentVersionNumber = 41;
// The oldest version of the schema such that a legacy Chrome client using that
// version can still read/write the current database.
constexpr int kCompatibleVersionNumber = 40;

base::Pickle SerializeAlternativeElementVector(
    const AlternativeElementVector& vector) {
  base::Pickle p;
  for (const auto& element : vector) {
    p.WriteString16(element.value);
    p.WriteString16(element.name);
  }
  return p;
}

AlternativeElementVector DeserializeAlternativeElementVector(
    const base::Pickle& p) {
  AlternativeElementVector ret;
  std::u16string value;
  std::u16string field_name;

  base::PickleIterator iterator(p);
  while (iterator.ReadString16(&value)) {
    bool name_success = iterator.ReadString16(&field_name);
    DCHECK(name_success);
    // TODO(crbug.com/40201826): migrate field identifier from name to
    // field_signature + field_rank. Field names are not unique and have
    // collisions on some forms. We don't store field_renderer_id in the
    // storage as it has no guarantees to be stable across page reloads.
    ret.emplace_back(AlternativeElement::Value(value),
                     autofill::FieldRendererId(),
                     AlternativeElement::Name(field_name));
  }
  return ret;
}

base::Pickle SerializeGaiaIdHashVector(const std::vector<GaiaIdHash>& hashes) {
  base::Pickle p;
  for (const auto& hash : hashes) {
    p.WriteString(hash.ToBinary());
  }
  return p;
}

std::vector<GaiaIdHash> DeserializeGaiaIdHashVector(const base::Pickle& p) {
  std::vector<GaiaIdHash> hashes;
  std::string hash;

  base::PickleIterator iterator(p);
  while (iterator.ReadString(&hash)) {
    hashes.push_back(GaiaIdHash::FromBinary(std::move(hash)));
    hash = {};
  }
  return hashes;
}

namespace {

using affiliations::SQLTableBuilder;

// Common prefix for all histograms.
constexpr char kPasswordManager[] = "PasswordManager";

// A simple class for scoping a login database transaction. This does not
// support rollback since the login database doesn't either.
class ScopedTransaction {
 public:
  explicit ScopedTransaction(LoginDatabase* db) : db_(db) {
    db_->BeginTransaction();
  }

  ScopedTransaction(const ScopedTransaction&) = delete;
  ScopedTransaction& operator=(const ScopedTransaction&) = delete;

  ~ScopedTransaction() { db_->CommitTransaction(); }

 private:
  raw_ptr<LoginDatabase> db_;
};

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginDatabaseTableColumns {
  COLUMN_ORIGIN_URL = 0,
  COLUMN_ACTION_URL,
  COLUMN_USERNAME_ELEMENT,
  COLUMN_USERNAME_VALUE,
  COLUMN_PASSWORD_ELEMENT,
  COLUMN_PASSWORD_VALUE,
  COLUMN_SUBMIT_ELEMENT,
  COLUMN_SIGNON_REALM,
  COLUMN_DATE_CREATED,
  COLUMN_BLOCKLISTED_BY_USER,
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA,
  COLUMN_DISPLAY_NAME,
  COLUMN_ICON_URL,
  COLUMN_FEDERATION_URL,
  COLUMN_SKIP_ZERO_CLICK,
  COLUMN_GENERATION_UPLOAD_STATUS,
  COLUMN_POSSIBLE_USERNAME_PAIRS,
  COLUMN_ID,
  COLUMN_DATE_LAST_USED,
  COLUMN_MOVING_BLOCKED_FOR,
  COLUMN_DATE_PASSWORD_MODIFIED,
  COLUMN_SENDER_EMAIL,
  COLUMN_SENDER_NAME,
  COLUMN_DATE_RECEIVED,
  COLUMN_SHARING_NOTIFICATION_DISPLAYED,
  COLUMN_KEYCHAIN_IDENTIFIER,
  COLUMN_SENDER_PROFILE_IMAGE_URL,
  COLUMN_NUM  // Keep this last.
};

enum class HistogramSize { SMALL, LARGE };

// An enum for UMA reporting. Add values to the end only.
enum DatabaseInitError {
  INIT_OK = 0,
  OPEN_FILE_ERROR = 1,
  START_TRANSACTION_ERROR = 2,
  META_TABLE_INIT_ERROR = 3,
  INCOMPATIBLE_VERSION = 4,
  INIT_LOGINS_ERROR = 5,
  INIT_STATS_ERROR = 6,
  MIGRATION_ERROR = 7,
  COMMIT_TRANSACTION_ERROR = 8,
  INIT_COMPROMISED_CREDENTIALS_ERROR = 9,
  INIT_FIELD_INFO_ERROR = 10,  // Deprecated.
  FOREIGN_KEY_ERROR = 11,
  INIT_PASSWORD_NOTES_ERROR = 12,

  DATABASE_INIT_ERROR_COUNT,
};

// Represents the encryption issues of the login database. Entries should
// not be renumbered and numeric values should never be reused. Always keep this
// enum in sync with the corresponding LoginDatabaseEncryptionStatus in
// enums.xml.
enum class LoginDatabaseEncryptionStatus {
  kNoIssues = 0,
  kInvalidEntriesInDatabase = 1,
  kEncryptionUnavailable = 2,
  kMaxValue = kEncryptionUnavailable,
};

// Represents whether undecryptable passwords should be deleted from the login
// database or the reason if they shouldn't be deleted.
// Entries should not be renumbered and numeric values should never be
// reused. Always keep this enum in sync with the corresponding
// LoginDatabaseShouldDeleteUndecryptablePasswords in enums.xml.
enum class ShouldDeleteUndecryptablePasswordsResult {
  kShouldDelete = 0,
  kUserDataDirEnvVarIsPresent = 1,
  kUserDataDirSwitchIsPresent = 2,
  kUserPasswordStoreSwitchIsPresent = 3,
  kUserEncryptionSelectionSwitchrIsPresent = 4,
  kEncryptionNotAvailiable = 5,
  kUserDataDirPolicySet = 6,
  kDisabledByPolicy = 7,
  kMaxValue = kDisabledByPolicy,
};

// Struct to hold table builder for different tables in the LoginDatabase.
struct SQLTableBuilders {
  raw_ptr<SQLTableBuilder> logins;
  raw_ptr<SQLTableBuilder> insecure_credentials;
  raw_ptr<SQLTableBuilder> password_notes;
  raw_ptr<SQLTableBuilder> passwords_sync_entities_metadata;
  raw_ptr<SQLTableBuilder> passwords_sync_model_metadata;
};

void BindAddStatement(const PasswordForm& form,
                      sql::Statement* s,
                      const std::string& encrypted_password) {
  s->BindString(COLUMN_ORIGIN_URL, form.url.spec());
  s->BindString(COLUMN_ACTION_URL, form.action.spec());
  s->BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s->BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s->BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  s->BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password);
  s->BindBlob(COLUMN_KEYCHAIN_IDENTIFIER, form.keychain_identifier);
  s->BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s->BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s->BindTime(COLUMN_DATE_CREATED, form.date_created);
  s->BindInt(COLUMN_BLOCKLISTED_BY_USER, form.blocked_by_user);
  s->BindInt(COLUMN_SCHEME, static_cast<int>(form.scheme));
  s->BindInt(COLUMN_PASSWORD_TYPE, static_cast<int>(form.type));
  s->BindInt(COLUMN_TIMES_USED, form.times_used_in_html_form);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s->BindBlob(COLUMN_FORM_DATA, form_data_pickle);
  s->BindString16(COLUMN_DISPLAY_NAME, form.display_name);
  s->BindString(COLUMN_ICON_URL,
                form.icon_url.is_valid() ? form.icon_url.spec() : "");
  // An empty Origin serializes as "null" which would be strange to store here.
  s->BindString(COLUMN_FEDERATION_URL, form.federation_origin.IsValid()
                                           ? form.federation_origin.Serialize()
                                           : std::string());
  s->BindInt(COLUMN_SKIP_ZERO_CLICK, form.skip_zero_click);
  s->BindInt(COLUMN_GENERATION_UPLOAD_STATUS,
             static_cast<int>(form.generation_upload_status));
  base::Pickle usernames_pickle =
      SerializeAlternativeElementVector(form.all_alternative_usernames);
  s->BindBlob(COLUMN_POSSIBLE_USERNAME_PAIRS, usernames_pickle);
  s->BindTime(COLUMN_DATE_LAST_USED, form.date_last_used);
  base::Pickle moving_blocked_for_pickle =
      SerializeGaiaIdHashVector(form.moving_blocked_for_list);
  s->BindBlob(COLUMN_MOVING_BLOCKED_FOR, moving_blocked_for_pickle);
  s->BindTime(COLUMN_DATE_PASSWORD_MODIFIED, form.date_password_modified);
  s->BindString16(COLUMN_SENDER_EMAIL, form.sender_email);
  s->BindString16(COLUMN_SENDER_NAME, form.sender_name);
  s->BindString(COLUMN_SENDER_PROFILE_IMAGE_URL,
                form.sender_profile_image_url.is_valid()
                    ? form.sender_profile_image_url.spec()
                    : "");
  s->BindTime(COLUMN_DATE_RECEIVED, form.date_received);
  s->BindBool(COLUMN_SHARING_NOTIFICATION_DISPLAYED,
              form.sharing_notification_displayed);
}

// Output parameter is the first one because of binding order.
void AddCallback(int* output_err, int err, sql::Statement* /*stmt*/) {
  DCHECK(output_err);
  *output_err = err;
  if (err == 19 /*SQLITE_CONSTRAINT*/) {
    DLOG(WARNING) << "LoginDatabase::AddLogin updated an existing form";
  }
}

class ScopedDbErrorHandler {
 public:
  explicit ScopedDbErrorHandler(sql::Database* db) : db_(db) {
    db_->set_error_callback(
        base::BindRepeating(AddCallback, &sqlite_error_code_));
  }
  ScopedDbErrorHandler(const ScopedDbErrorHandler&) = delete;
  ScopedDbErrorHandler& operator=(const ScopedDbErrorHandler&) = delete;

  ~ScopedDbErrorHandler() { db_->reset_error_callback(); }

  // Error codes are defined in the sql::SqliteResultCode enum.
  void reset_error_code() { sqlite_error_code_ = 0; }
  int get_error_code() const { return sqlite_error_code_; }

 private:
  raw_ptr<sql::Database> db_;
  int sqlite_error_code_{0};
};

bool DoesMatchConstraints(const PasswordForm& form) {
  if (!affiliations::IsValidAndroidFacetURI(form.signon_realm) &&
      form.url.is_empty()) {
    DLOG(ERROR) << "Constraint violation: form.origin is empty";
    return false;
  }
  if (form.signon_realm.empty()) {
    DLOG(ERROR) << "Constraint violation: form.signon_realm is empty";
    return false;
  }
  if (!form.url.is_empty() && !form.url.is_valid()) {
    DLOG(ERROR) << "Constraint violation: form.url is non-empty and invalid";
    return false;
  }
  return true;
}

void LogDatabaseInitError(DatabaseInitError error) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.LoginDatabaseInit2", error,
                            DATABASE_INIT_ERROR_COUNT);
}

constexpr char kPasswordsSyncModelMetadataTableName[] = "sync_model_metadata";
constexpr char kPasswordsSyncEntitiesMetadataTableName[] =
    "sync_entities_metadata";

bool ClearAllSyncMetadata(sql::Database* db, syncer::DataType data_type) {
  CHECK_EQ(data_type, syncer::PASSWORDS);
  sql::Statement s1(db->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf("DELETE FROM %s",
                                        kPasswordsSyncModelMetadataTableName)));

  sql::Statement s2(db->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s",
                         kPasswordsSyncEntitiesMetadataTableName)));

  return s1.Run() && s2.Run();
}

// Seals the version of the given builders. This is method should be always used
// to seal versions of all builder to make sure all builders are at the same
// version.
void SealVersion(SQLTableBuilders builders, unsigned expected_version) {
  unsigned logins_version = builders.logins->SealVersion();
  DCHECK_EQ(expected_version, logins_version);

  unsigned insecure_credentials_version =
      builders.insecure_credentials->SealVersion();
  DCHECK_EQ(expected_version, insecure_credentials_version);

  unsigned notes_version = builders.password_notes->SealVersion();
  DCHECK_EQ(expected_version, notes_version);

  unsigned passwords_sync_entities_metadata_version =
      builders.passwords_sync_entities_metadata->SealVersion();
  DCHECK_EQ(expected_version, passwords_sync_entities_metadata_version);

  unsigned passwords_sync_model_metadata_version =
      builders.passwords_sync_model_metadata->SealVersion();
  DCHECK_EQ(expected_version, passwords_sync_model_metadata_version);
}

// Teaches |builders| about the different DB schemes in different versions.
void InitializeBuilders(SQLTableBuilders builders) {
  // Versions 0 and 1, which are the same.
  builders.logins->AddColumnToUniqueKey("origin_url", "VARCHAR NOT NULL");
  builders.logins->AddColumn("action_url", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("username_element", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("username_value", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("password_element", "VARCHAR");
  builders.logins->AddColumn("password_value", "BLOB");
  builders.logins->AddColumn("submit_element", "VARCHAR");
  builders.logins->AddColumnToUniqueKey("signon_realm", "VARCHAR NOT NULL");
  builders.logins->AddColumn("ssl_valid", "INTEGER NOT NULL");
  builders.logins->AddColumn("preferred", "INTEGER NOT NULL");
  builders.logins->AddColumn("date_created", "INTEGER NOT NULL");
  builders.logins->AddColumn("blacklisted_by_user", "INTEGER NOT NULL");
  builders.logins->AddColumn("scheme", "INTEGER NOT NULL");
  builders.logins->AddIndex("logins_signon", {"signon_realm"});
  SealVersion(builders, /*expected_version=*/0u);
  SealVersion(builders, /*expected_version=*/1u);

  // Version 2.
  builders.logins->AddColumn("password_type", "INTEGER");
  builders.logins->AddColumn("possible_usernames", "BLOB");
  SealVersion(builders, /*expected_version=*/2u);

  // Version 3.
  builders.logins->AddColumn("times_used", "INTEGER");
  SealVersion(builders, /*expected_version=*/3u);

  // Version 4.
  builders.logins->AddColumn("form_data", "BLOB");
  SealVersion(builders, /*expected_version=*/4u);

  // Version 5.
  builders.logins->AddColumn("use_additional_auth", "INTEGER");
  SealVersion(builders, /*expected_version=*/5u);

  // Version 6.
  builders.logins->AddColumn("date_synced", "INTEGER");
  SealVersion(builders, /*expected_version=*/6u);

  // Version 7.
  builders.logins->AddColumn("display_name", "VARCHAR");
  builders.logins->AddColumn("avatar_url", "VARCHAR");
  builders.logins->AddColumn("federation_url", "VARCHAR");
  builders.logins->AddColumn("is_zero_click", "INTEGER");
  SealVersion(builders, /*expected_version=*/7u);

  // Version 8.
  SealVersion(builders, /*expected_version=*/8u);
  // Version 9.
  SealVersion(builders, /*expected_version=*/9u);
  // Version 10.
  builders.logins->DropColumn("use_additional_auth");
  SealVersion(builders, /*expected_version=*/10u);

  // Version 11.
  builders.logins->RenameColumn("is_zero_click", "skip_zero_click");
  SealVersion(builders, /*expected_version=*/11u);

  // Version 12.
  builders.logins->AddColumn("generation_upload_status", "INTEGER");
  SealVersion(builders, /*expected_version=*/12u);

  // Version 13.
  SealVersion(builders, /*expected_version=*/13u);
  // Version 14.
  builders.logins->RenameColumn("avatar_url", "icon_url");
  SealVersion(builders, /*expected_version=*/14u);

  // Version 15.
  SealVersion(builders, /*expected_version=*/15u);
  // Version 16.
  SealVersion(builders, /*expected_version=*/16u);
  // Version 17.
  SealVersion(builders, /*expected_version=*/17u);

  // Version 18.
  builders.logins->DropColumn("ssl_valid");
  SealVersion(builders, /*expected_version=*/18u);

  // Version 19.
  builders.logins->DropColumn("possible_usernames");
  builders.logins->AddColumn("possible_username_pairs", "BLOB");
  SealVersion(builders, /*expected_version=*/19u);

  // Version 20.
  builders.logins->AddPrimaryKeyColumn("id");
  SealVersion(builders, /*expected_version=*/20u);

  // Version 21.
  builders.passwords_sync_entities_metadata->AddPrimaryKeyColumn("storage_key");
  builders.passwords_sync_entities_metadata->AddColumn("metadata",
                                                       "VARCHAR NOT NULL");
  builders.passwords_sync_model_metadata->AddPrimaryKeyColumn("id");
  builders.passwords_sync_model_metadata->AddColumn("model_metadata",
                                                    "VARCHAR NOT NULL");
  SealVersion(builders, /*expected_version=*/21u);

  // Version 22. Changes in Sync metadata encryption.
  SealVersion(builders, /*expected_version=*/22u);

  // Version 23. Version 22 could have some corruption in Sync metadata and
  // hence we are migrating users on it by clearing their metadata to make Sync
  // start clean from scratch.
  SealVersion(builders, /*expected_version=*/23u);

  // Version 24. Version 23 could have some corruption in Sync metadata and
  // hence we are migrating users on it by clearing their metadata to make Sync
  // start clean from scratch.
  SealVersion(builders, /*expected_version=*/24u);

  // Version 25. Introduce date_last_used column to replace the preferred
  // column. MigrateDatabase() will take care of migrating the data.
  builders.logins->AddColumn("date_last_used", "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/25u);

  // Version 26 is the first version where the id is AUTOINCREMENT.
  SealVersion(builders, /*expected_version=*/26u);

  // Version 27. Add the moving_blocked_for column to contain serialized list of
  // gaia id hashes for users that prefer not to move this credential to their
  // account store.
  builders.logins->AddColumn("moving_blocked_for", "BLOB");
  SealVersion(builders, /*expected_version=*/27u);

  // Version 28.
  builders.logins->DropColumn("preferred");
  SealVersion(builders, /*expected_version=*/28u);

  // Version 29.
  // Migrate the compromised credentials from "compromised_credentials" to the
  // new table "insecure credentials" with a foreign key to the logins table.
  builders.insecure_credentials->AddColumnToUniqueKey(
      "parent_id", "INTEGER", "logins", "foreign_key_index");
  builders.insecure_credentials->AddColumnToUniqueKey("insecurity_type",
                                                      "INTEGER NOT NULL");
  builders.insecure_credentials->AddColumn("create_time", "INTEGER NOT NULL");
  builders.insecure_credentials->AddColumn("is_muted",
                                           "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/29u);

  // Version 30. Introduce 'date_password_modified' column.
  builders.logins->AddColumn("date_password_modified",
                             "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/30u);

  // Version 31. Dropped 'date_synced' column.
  builders.logins->DropColumn("date_synced");
  SealVersion(builders, /*expected_version=*/31u);

  // Version 32. Set timestamps of uninitialized timestamps in
  // 'insecure_credentials' table.
  SealVersion(builders, /*expected_version=*/32u);

  // Version 33. Introduce password notes table.
  builders.password_notes->AddPrimaryKeyColumn("id");
  builders.password_notes->AddColumnToUniqueKey(
      "parent_id", "INTEGER NOT NULL", "logins", "foreign_key_index_notes");
  builders.password_notes->AddColumnToUniqueKey("key", "VARCHAR NOT NULL");
  builders.password_notes->AddColumn("value", "BLOB");
  builders.password_notes->AddColumn("date_created", "INTEGER NOT NULL");
  builders.password_notes->AddColumn("confidential", "INTEGER");
  SealVersion(builders, /*expected_version=*/33u);

  // Version 34. Add `trigger_notification_from_backend` column to the
  // `insecure_credentials_table`.
  builders.insecure_credentials->AddColumn("trigger_notification_from_backend",
                                           "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/34u);

  // Version 35.
  // In version 35, two tables have been introduced to the logins database
  // `incoming_sharing_invitation_sync_model_metadata` and
  // `incoming_sharing_invitation_sync_entities_metadata`. Those tables aren't
  // required to be part of the login database and shouldn't be created.
  SealVersion(builders, /*expected_version=*/35u);

  // Version 36.
  // In version 36, the tables 'incoming_sharing_invitation_sync_model_metadata`
  // and `incoming_sharing_invitation_sync_entities_metadata` are dropped.
  SealVersion(builders, /*expected_version=*/36u);

  // Version 37.
  // In version 37, more fields are added to the logins table to carry the
  // metadata of shared password such as sender name.
  builders.logins->AddColumn("sender_email", "VARCHAR");
  builders.logins->AddColumn("sender_name", "VARCHAR");
  builders.logins->AddColumn("date_received", "INTEGER");
  builders.logins->AddColumn("sharing_notification_displayed",
                             "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/37u);

  // Version 38.
  SealVersion(builders, /*expected_version=*/38u);

  // Version 39.
  // Adding keychain identifier where the password is stored. It's the same as
  // password_value column before this version. This column is needed to support
  // Credential Provider on iOS.
  builders.logins->AddColumn("keychain_identifier", "BLOB");
  SealVersion(builders, /*expected_version=*/39u);

  // Version 40.
  // Migrate password notes encryption to OSCrypt.
  SealVersion(builders, /*expected_version=*/40u);

  // Version 41.
  // Add sender profile image url as part of the shared passwords metadata
  // similar to changes in version 37.
  builders.logins->AddColumn("sender_profile_image_url", "VARCHAR");
  SealVersion(builders, /*expected_version=*/41u);

  static_assert(kCurrentVersionNumber == 41, "Seal the recent version");
  CHECK_EQ(static_cast<size_t>(COLUMN_NUM), builders.logins->NumberOfColumns())
      << "Adjust LoginDatabaseTableColumns if you change column definitions "
         "here.";
}

// Callback called upon each migration step of the logins table. It's used to
// inject custom schema migration logic not covered by the generic
// SQLTableBuilder migration. |new_version| indicates how far
// SQLTableBuilder is in the migration process.
bool LoginsTablePostMigrationStepCallback(sql::Database* db,
                                          unsigned new_version) {
  // In version 26, the primary key of the logins table became an
  // AUTOINCREMENT field. Since SQLite doesn't allow changing the column type,
  // the only way is to actually create a temp table with the primary key
  // properly set as an AUTOINCREMENT field, and move the data there. The code
  // has been adjusted such that newly created tables have the primary key
  // properly set as AUTOINCREMENT.
  if (new_version == 26) {
    // This statement creates the logins database similar to version 26 with
    // the primary key column set to AUTOINCREMENT.
    const char temp_table_create_statement_version_26[] =
        "CREATE TABLE logins_temp (origin_url VARCHAR NOT NULL,action_url "
        "VARCHAR,username_element VARCHAR,username_value "
        "VARCHAR,password_element VARCHAR,password_value BLOB,submit_element "
        "VARCHAR,signon_realm VARCHAR NOT NULL,preferred INTEGER NOT "
        "NULL,date_created INTEGER NOT NULL,blacklisted_by_user INTEGER NOT "
        "NULL,scheme INTEGER NOT NULL,password_type INTEGER,times_used "
        "INTEGER,form_data BLOB,date_synced INTEGER,display_name "
        "VARCHAR,icon_url VARCHAR,federation_url VARCHAR,skip_zero_click "
        "INTEGER,generation_upload_status INTEGER,possible_username_pairs "
        "BLOB,id INTEGER PRIMARY KEY AUTOINCREMENT,date_last_used "
        "INTEGER,UNIQUE (origin_url, username_element, username_value, "
        "password_element, signon_realm))";
    const char move_data_statement[] =
        "INSERT INTO logins_temp SELECT * from logins";
    const char drop_table_statement[] = "DROP TABLE logins";
    const char rename_table_statement[] =
        "ALTER TABLE logins_temp RENAME TO logins";

    sql::Transaction transaction(db);
    if (!(transaction.Begin() &&
          db->Execute(temp_table_create_statement_version_26) &&
          db->Execute(move_data_statement) &&
          db->Execute(drop_table_statement) &&
          db->Execute(rename_table_statement) && transaction.Commit())) {
      return false;
    }
  }
  return true;
}

bool InsecureCredentialsPostMigrationStepCallback(
    SQLTableBuilder* insecure_credentials_builder,
    sql::Database* db,
    unsigned new_version) {
  if (new_version == 29) {
    std::string create_table_statement =
        "CREATE TABLE insecure_credentials ("
        "parent_id INTEGER REFERENCES logins ON UPDATE CASCADE ON DELETE "
        "CASCADE DEFERRABLE INITIALLY DEFERRED, "
        "insecurity_type INTEGER NOT NULL, "
        "create_time INTEGER NOT NULL, "
        "is_muted INTEGER NOT NULL DEFAULT 0, "
        "UNIQUE (parent_id, insecurity_type))";
    std::string create_index_statement =
        "CREATE INDEX foreign_key_index ON insecure_credentials "
        "(parent_id)";
    sql::Transaction creation_transaction(db);
    bool table_creation_success =
        creation_transaction.Begin() && db->Execute(create_table_statement) &&
        db->Execute(create_index_statement) && creation_transaction.Commit();
    if (!table_creation_success) {
      LOG(ERROR) << "Failed to create the 'insecure_credentials' table";
      LogDatabaseInitError(INIT_COMPROMISED_CREDENTIALS_ERROR);
      return false;
    }
    if (!db->DoesTableExist("compromised_credentials")) {
      return true;
    }
    // The 'compromised_credentials' table must be migrated to
    // 'insecure_credentials'.
    constexpr char select_compromised[] =
        "SELECT "
        "id, create_time, compromise_type FROM compromised_credentials "
        "INNER JOIN logins ON "
        "compromised_credentials.url = logins.signon_realm AND "
        "compromised_credentials.username = logins.username_value";
    const std::string insert_statement = base::StringPrintf(
        "INSERT OR REPLACE INTO %s "
        "(parent_id, create_time, insecurity_type) %s",
        InsecureCredentialsTable::kTableName, select_compromised);
    constexpr char drop_table_statement[] =
        "DROP TABLE compromised_credentials";
    sql::Transaction transaction(db);
    if (!(transaction.Begin() && db->Execute(insert_statement) &&
          db->Execute(drop_table_statement) && transaction.Commit())) {
      return false;
    }
  }
  return true;
}

bool PasswordNotesPostMigrationStepCallback(
    SQLTableBuilder* password_notes_builder,
    sql::Database* db,
    unsigned new_version) {
  if (new_version == 33) {
    std::string create_table_statement =
        "CREATE TABLE password_notes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "parent_id INTEGER NOT NULL REFERENCES logins ON UPDATE CASCADE ON "
        "DELETE CASCADE DEFERRABLE INITIALLY DEFERRED, "
        "key VARCHAR NOT NULL, "
        "value BLOB, "
        "date_created INTEGER NOT NULL, "
        "confidential INTEGER, "
        "UNIQUE (parent_id, key))";
    std::string create_index_statement =
        "CREATE INDEX foreign_key_index_notes ON password_notes (parent_id)";
    sql::Transaction transaction(db);
    bool table_creation_success =
        transaction.Begin() && db->Execute(create_table_statement) &&
        db->Execute(create_index_statement) && transaction.Commit();
    if (!table_creation_success) {
      LOG(ERROR) << "Failed to create the 'password_notes' table";
      LogDatabaseInitError(INIT_PASSWORD_NOTES_ERROR);
      return false;
    }
  }
  return true;
}

#if BUILDFLAG(IS_IOS)
bool DeletePassword(sql::Database* db, int id) {
  sql::Statement password_delete(
      db->GetUniqueStatement("DELETE FROM logins WHERE id = ?"));
  password_delete.BindInt(0, id);
  return password_delete.Run();
}

bool UpdatePassword(sql::Database* db,
                    int id,
                    const std::string& encrypted_password) {
  sql::Statement password_value_update(db->GetUniqueStatement(
      "UPDATE logins SET password_value = ? WHERE id = ?"));
  password_value_update.BindBlob(0, encrypted_password);
  password_value_update.BindInt(1, id);
  return password_value_update.Run();
}

bool MigrateToOSCrypt(IsAccountStore is_account_store,
                      sql::Database* db,
                      EncryptDecryptInterface* encryptor) {
  sql::Statement get_passwords_statement(
      db->GetUniqueStatement("SELECT id, password_value FROM logins"));
  // Update each password_value with the new BLOB.
  while (get_passwords_statement.Step()) {
    int id = get_passwords_statement.ColumnInt(0);
    // First get decrypted password value using old method.
    std::u16string plaintext_password;
    OSStatus retrieval_status = GetTextFromKeychainIdentifier(
        get_passwords_statement.ColumnString(1), &plaintext_password);
    // Password no longer exists in the keychain, meaning it's lost forever.
    // In this case delete the entry from the database and continue with
    // migration.
    if (retrieval_status == errSecItemNotFound) {
      if (!DeletePassword(db, id)) {
        return false;
      }
    } else if (retrieval_status != errSecSuccess) {
      // Stop migration with any other error.
      return false;
    } else {
      // Encrypt password using OSCrypt.
      std::string encrypted_password;
      if (encryptor->EncryptedString(plaintext_password, &encrypted_password) !=
          EncryptionResult::kSuccess) {
        return false;
      }
      // Updated password_value in the database.
      if (!UpdatePassword(db, id, encrypted_password)) {
        return false;
      }
    }
  }
  return true;
}

#endif

// Call this after having called InitializeBuilders(), to migrate the database
// from the current version to kCurrentVersionNumber.
bool MigrateDatabase(unsigned current_version,
                     SQLTableBuilders builders,
                     IsAccountStore is_account_store,
                     sql::Database* db,
                     EncryptDecryptInterface* encryptor) {
  if (!builders.logins->MigrateFrom(
          current_version, db,
          base::BindRepeating(&LoginsTablePostMigrationStepCallback))) {
    return false;
  }
  if (!builders.insecure_credentials->MigrateFrom(
          current_version, db,
          base::BindRepeating(&InsecureCredentialsPostMigrationStepCallback,
                              builders.insecure_credentials))) {
    return false;
  }
  if (!builders.password_notes->MigrateFrom(
          current_version, db,
          base::BindRepeating(&PasswordNotesPostMigrationStepCallback,
                              builders.password_notes))) {
    return false;
  }

  if (!builders.passwords_sync_entities_metadata->MigrateFrom(current_version,
                                                              db)) {
    return false;
  }

  if (!builders.passwords_sync_model_metadata->MigrateFrom(current_version,
                                                           db)) {
    return false;
  }

  // Data changes, not covered by the schema migration above.
  if (current_version <= 8) {
    sql::Statement fix_time_format;
    fix_time_format.Assign(db->GetUniqueStatement(
        "UPDATE logins SET date_created = (date_created * ?) + ?"));
    fix_time_format.BindInt64(0, base::Time::kMicrosecondsPerSecond);
    fix_time_format.BindInt64(1, base::Time::kTimeTToMicrosecondsOffset);
    if (!fix_time_format.Run()) {
      return false;
    }
  }

  if (current_version <= 16) {
    sql::Statement reset_zero_click;
    reset_zero_click.Assign(
        db->GetUniqueStatement("UPDATE logins SET skip_zero_click = 1"));
    if (!reset_zero_click.Run()) {
      return false;
    }
  }

  // Passwords Sync Metadata tables have been introduced in version 21. It is
  // enough to drop all data because Sync would populate the tables properly at
  // startup.
  if (current_version >= 21 && current_version < 26) {
    if (!ClearAllSyncMetadata(db, syncer::PASSWORDS)) {
      return false;
    }
  }

  // Set the default value for 'date_password_modified'.
  if (current_version < 30) {
    sql::Statement set_date_password_modified;
    set_date_password_modified.Assign(db->GetUniqueStatement(
        "UPDATE logins SET date_password_modified = date_created"));
    if (!set_date_password_modified.Run()) {
      return false;
    }
  }

  // Set the create_time value when uninitialized for 'insecure_credentials'.
  if (current_version >= 29 && current_version < 32) {
    sql::Statement set_timestamp;
    set_timestamp.Assign(
        db->GetUniqueStatement("UPDATE insecure_credentials SET create_time = "
                               "? WHERE create_time = 0"));
    set_timestamp.BindTime(0, base::Time::Now());
    if (!set_timestamp.Run()) {
      return false;
    }
  }

  if (current_version < 36) {
    // Tables 'incoming_sharing_invitation_sync_model_metadata' and
    // 'incoming_sharing_invitation_sync_entities_metadata' are not required to
    // be part of the login database anymore.
    if (db->DoesTableExist("incoming_sharing_invitation_sync_model_metadata")) {
      if (!db->Execute(
              "DROP TABLE incoming_sharing_invitation_sync_model_metadata")) {
        return false;
      }
    }
    if (db->DoesTableExist(
            "incoming_sharing_invitation_sync_entities_metadata")) {
      if (!db->Execute("DROP TABLE "
                       "incoming_sharing_invitation_sync_entities_metadata")) {
        return false;
      }
    }
  }

#if BUILDFLAG(IS_IOS)
  if (current_version < 39) {
    // Before version 39, password_value was used to store keychain identifier
    // where the actual password is. After this version password_value is
    // encrypted password using OSCrypt. To ensure Credential Provider works as
    // intended we need to add new column and preserve saving password to
    // keychain.
    sql::Statement copy_keychain_identifier(db->GetUniqueStatement(
        "UPDATE logins SET keychain_identifier = password_value"));
    if (!copy_keychain_identifier.Run()) {
      return false;
    }

    return MigrateToOSCrypt(is_account_store, db, encryptor);
  }
#endif

  return true;
}

// Because of https://crbug.com/295851, some early version numbers might be
// wrong. This function detects that and fixes the version.
bool FixVersionIfNeeded(sql::Database* db, int* current_version) {
  if (*current_version == 1) {
    int extra_columns = 0;
    if (db->DoesColumnExist("logins", "password_type")) {
      ++extra_columns;
    }
    if (db->DoesColumnExist("logins", "possible_usernames")) {
      ++extra_columns;
    }
    if (extra_columns == 2) {
      *current_version = 2;
    } else if (extra_columns == 1) {
      // If this is https://crbug.com/295851 then either both columns exist
      // or none.
      return false;
    }
  }
  if (*current_version == 2) {
    if (db->DoesColumnExist("logins", "times_used")) {
      *current_version = 3;
    }
  }
  if (*current_version == 3) {
    if (db->DoesColumnExist("logins", "form_data")) {
      *current_version = 4;
    }
  }
  // "date_last_used" columns has been introduced in version 25. if it exists,
  // the version should be at least 25. This has been added to address this bug
  // (crbug.com/1020320).
  if (*current_version < 25) {
    if (db->DoesColumnExist("logins", "date_last_used")) {
      *current_version = 25;
    }
  }
  return true;
}

// Generates the string "(?,?,...,?)" with |count| repetitions of "?".
std::string GeneratePlaceholders(size_t count) {
  std::string result(2 * count + 1, ',');
  result.front() = '(';
  result.back() = ')';
  for (size_t i = 1; i < 2 * count + 1; i += 2) {
    result[i] = '?';
  }
  return result;
}

// Fills |form| with necessary data required to be removed from the database
// and returns it.
PasswordForm GetFormForRemoval(sql::Statement& statement) {
  PasswordForm form;
  form.url = GURL(statement.ColumnString(COLUMN_ORIGIN_URL));
  form.username_element = statement.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form.username_value = statement.ColumnString16(COLUMN_USERNAME_VALUE);
  form.password_element = statement.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form.signon_realm = statement.ColumnString(COLUMN_SIGNON_REALM);
  return form;
}

// Whether we should try to return the decryptable passwords while the
// encryption service fails for some passwords.
bool ShouldReturnPartialPasswords() {
  return base::FeatureList::IsEnabled(features::kSkipUndecryptablePasswords);
}

std::unique_ptr<sync_pb::EntityMetadata> DecryptAndParseSyncEntityMetadata(
    const std::string& encrypted_serialized_metadata) {
  std::string decrypted_serialized_metadata;
  if (!OSCrypt::DecryptString(encrypted_serialized_metadata,
                              &decrypted_serialized_metadata)) {
    DLOG(WARNING) << "Failed to decrypt PASSWORD data type "
                     "sync_pb::EntityMetadata.";
    return nullptr;
  }

  auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
  if (!entity_metadata->ParseFromString(decrypted_serialized_metadata)) {
    DLOG(WARNING) << "Failed to deserialize PASSWORD data type "
                     "sync_pb::EntityMetadata.";
    return nullptr;
  }

  return entity_metadata;
}

EncryptionResult DecryptPasswordFromStatement(
    sql::Statement& s,
    std::u16string* plaintext_password,
    EncryptDecryptInterface* decryptor) {
  CHECK(plaintext_password);
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  EncryptionResult encryption_result =
      decryptor->DecryptedString(encrypted_password, plaintext_password);
  if (encryption_result != EncryptionResult::kSuccess) {
    DLOG(WARNING) << "Password decryption failed, encryption_result is "
                  << static_cast<int>(encryption_result);
  }
  return encryption_result;
}

void RecordShouldDeleteUndecryptablePasswordsMetric(
    ShouldDeleteUndecryptablePasswordsResult should_delete_status) {
  base::UmaHistogramEnumeration(
      "PasswordManager.LoginDatabase.ShouldDeleteUndecryptablePasswords",
      should_delete_status);
}

bool ShouldDeleteUndecryptablePasswords(
    LoginDatabase::OnUndecryptablePasswordsRemoved
        clearing_undecryptable_passwords,
    bool is_user_data_dir_policy_set,
    bool is_enabled_by_policy,
    IsAccountStore is_account_store) {
#if BUILDFLAG(IS_LINUX)
  std::string user_data_dir_string;
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  // On Linux user data directory ca be specified using an env variable. If it
  // exists, passwords shouldn't be deleted.
  if (environment->GetVar("CHROME_USER_DATA_DIR", &user_data_dir_string)) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::kUserDataDirEnvVarIsPresent);
    return false;
  }
#endif  // BUILDFLAG(IS_LINUX)

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(password_manager::kUserDataDir)) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::kUserDataDirSwitchIsPresent);
    return false;
  }

#if BUILDFLAG(IS_LINUX)
  if (command_line->HasSwitch(password_manager::kPasswordStore)) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::
            kUserPasswordStoreSwitchIsPresent);
    return false;
  }
  if (command_line->HasSwitch(password_manager::kEnableEncryptionSelection)) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::
            kUserEncryptionSelectionSwitchrIsPresent);
    return false;
  }
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (is_user_data_dir_policy_set) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::kUserDataDirPolicySet);
    return false;
  }
#endif

  if (!OSCrypt::IsEncryptionAvailable()) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::kEncryptionNotAvailiable);
    return false;
  }

  if (!is_enabled_by_policy) {
    RecordShouldDeleteUndecryptablePasswordsMetric(
        ShouldDeleteUndecryptablePasswordsResult::kDisabledByPolicy);
    return false;
  }

  RecordShouldDeleteUndecryptablePasswordsMetric(
      ShouldDeleteUndecryptablePasswordsResult::kShouldDelete);

  // Needed in order to maintain kClearUndecryptablePasswords experiment groups
  // population.
  if (clearing_undecryptable_passwords) {
    clearing_undecryptable_passwords.Run(is_account_store);
  }
  return base::FeatureList::IsEnabled(features::kClearUndecryptablePasswords);
}

}  // namespace

struct LoginDatabase::PrimaryKeyAndPassword {
  int primary_key;
  std::u16string decrypted_password;
  std::string keychain_identifier;
};

LoginDatabase::LoginDatabase(const base::FilePath& db_path,
                             IsAccountStore is_account_store,
                             DeletingUndecryptablePasswordsEnabled can_delete)
    : db_path_(db_path),
      is_account_store_(is_account_store),
      // Set options for a small, private database (based on WebDatabase).
      db_({.page_size = 2048, .cache_size = 32}),
      is_deleting_undecryptable_logins_enabled_by_policy_(can_delete) {}

LoginDatabase::~LoginDatabase() = default;

bool LoginDatabase::Init(
    OnUndecryptablePasswordsRemoved on_undecryptable_passwords_removed,
    std::unique_ptr<os_crypt_async::Encryptor> encryptor) {
  TRACE_EVENT0("passwords", "LoginDatabase::Init");
  on_undecryptable_passwords_removed_ =
      std::move(on_undecryptable_passwords_removed);
  encryptor_ = std::move(encryptor);

  db_.set_histogram_tag("Passwords");

  if (!db_.Open(db_path_)) {
    LogDatabaseInitError(OPEN_FILE_ERROR);
    LOG(ERROR) << "Unable to open the password store database.";
    return false;
  }

  absl::Cleanup close_db_runner = [this] { db_.Close(); };

  if (!db_.Execute("PRAGMA foreign_keys = ON")) {
    LogDatabaseInitError(FOREIGN_KEY_ERROR);
    LOG(ERROR) << "Unable to activate foreign keys.";
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LogDatabaseInitError(START_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to start a transaction.";
    return false;
  }

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    LogDatabaseInitError(META_TABLE_INIT_ERROR);
    LOG(ERROR) << "Unable to create the meta table.";
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LogDatabaseInitError(INCOMPATIBLE_VERSION);
    LOG(ERROR) << "Password store database is too new, kCurrentVersionNumber="
               << kCurrentVersionNumber << ", GetCompatibleVersionNumber="
               << meta_table_.GetCompatibleVersionNumber();
    return false;
  }

  SQLTableBuilder logins_builder("logins");
  SQLTableBuilder insecure_credentials_builder(
      InsecureCredentialsTable::kTableName);
  SQLTableBuilder password_notes_builder(PasswordNotesTable::kTableName);
  SQLTableBuilder passwords_sync_entities_metadata_builder(
      kPasswordsSyncEntitiesMetadataTableName);
  SQLTableBuilder passwords_sync_model_metadata_builder(
      kPasswordsSyncModelMetadataTableName);
  SQLTableBuilders builders = {&logins_builder, &insecure_credentials_builder,
                               &password_notes_builder,
                               &passwords_sync_entities_metadata_builder,
                               &passwords_sync_model_metadata_builder};
  InitializeBuilders(builders);
  InitializeStatementStrings(logins_builder);

  if (!logins_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'logins' table";
    return false;
  }

  if (!passwords_sync_entities_metadata_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'sync_entities_metadata' table";
    return false;
  }

  if (!passwords_sync_model_metadata_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'sync_model_metadata' table";
    return false;
  }

  stats_table_.Init(&db_);
  insecure_credentials_table_.Init(&db_);
  password_notes_table_.Init(&db_, this);

  int current_version = meta_table_.GetVersionNumber();
  bool migration_success = FixVersionIfNeeded(&db_, &current_version);

  // If the file on disk is an older database version, bring it up to date.
  if (migration_success && current_version < kCurrentVersionNumber) {
    migration_success =
        MigrateDatabase(base::checked_cast<unsigned>(current_version), builders,
                        is_account_store_, &db_, this);
  }
  // Enforce that 'insecure_credentials' is created only after the 'logins'
  // table was created and migrated to the latest version. This guarantees the
  // existence of the `id` column in the `logins` table which was introduced
  // only in version 20 and is referenced by `insecure_credentials` table. The
  // table will be created here for a new profile. For an old profile it's
  // created in MigrateDatabase above.
  if (migration_success && !insecure_credentials_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'insecure_credentials' table";
    LogDatabaseInitError(INIT_COMPROMISED_CREDENTIALS_ERROR);
    return false;
  }
  // Enforce that 'password_notes' is created only after the 'logins' table was
  // created and migrated to the latest version. This guarantees the existence
  // of the `id` column in the `logins` table which was introduced only in
  // version 20 and is referenced by 'password_notes' table. The table will be
  // created here for a new profile. For an old profile it's created in
  // MigrateDatabase above.
  if (migration_success && !password_notes_builder.CreateTable(&db_)) {
    LOG(ERROR) << "Failed to create the 'password_notes' table";
    LogDatabaseInitError(INIT_PASSWORD_NOTES_ERROR);
    return false;
  }
  if (migration_success) {
    migration_success = password_notes_table_.MigrateTable(
        current_version, is_account_store_.value());
  }
  if (migration_success && current_version <= 15) {
    migration_success = stats_table_.MigrateToVersion(16);
  }
  if (migration_success && current_version < kCurrentVersionNumber) {
    // |migration_success| could be true when no logins have been actually
    // migrated. We should protect against downgrading the database version in
    // such case. Update the database version only if a migration took place.
    migration_success =
        meta_table_.SetCompatibleVersionNumber(kCompatibleVersionNumber) &&
        meta_table_.SetVersionNumber(kCurrentVersionNumber);
  }
  if (!migration_success) {
    LogDatabaseInitError(MIGRATION_ERROR);
    LOG(ERROR) << "Unable to migrate database from "
               << meta_table_.GetVersionNumber() << " to "
               << kCurrentVersionNumber;
    return false;
  }

  if (!stats_table_.CreateTableIfNecessary()) {
    LogDatabaseInitError(INIT_STATS_ERROR);
    LOG(ERROR) << "Unable to create the stats table.";
    return false;
  }

  // The table "leaked_credentials" was previously created without a flag.
  // The table is now renamed to "compromised_credentials" and also includes
  // a new column so the old table needs to be deleted.
  if (db_.DoesTableExist("leaked_credentials")) {
    if (!db_.Execute("DROP TABLE leaked_credentials")) {
      LOG(ERROR) << "Unable to create the stats table.";
      return false;
    }
  }

  // The table "field_info" is deprecated.
  if (db_.DoesTableExist("field info")) {
    if (!db_.Execute("DROP TABLE field_info")) {
      LOG(ERROR) << "Unable to delete the field info table.";
      return false;
    }
  }

  if (!transaction.Commit()) {
    LogDatabaseInitError(COMMIT_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to commit a transaction.";
    return false;
  }

  TriggerIsEmptyCb();
  LogDatabaseInitError(INIT_OK);

  // Keep the database open if everything went well.
  std::move(close_db_runner).Cancel();

  return true;
}

void LoginDatabase::ReportBubbleSuppressionMetrics() {
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  base::UmaHistogramCustomCounts(
      "PasswordManager.BubbleSuppression.AccountsInStatisticsTable2",
      stats_table_.GetNumAccounts(), 0, 1000, 100);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
}

void LoginDatabase::ReportInaccessiblePasswordsMetrics() {
  sql::Statement get_passwords_statement(
      db_.GetUniqueStatement("SELECT password_value "
                             "FROM logins WHERE blacklisted_by_user = 0"));

  size_t failed_encryption = 0;
  while (get_passwords_statement.Step()) {
    std::u16string decrypted_password;
    if (DecryptedString(get_passwords_statement.ColumnString(0),
                        &decrypted_password) != EncryptionResult::kSuccess) {
      ++failed_encryption;
    }
  }

  std::string_view suffix_for_store =
      is_account_store_.value() ? ".AccountStore" : ".ProfileStore";
  base::UmaHistogramCounts100(base::StrCat({kPasswordManager, suffix_for_store,
                                            ".InaccessiblePasswords3"}),
                              failed_encryption);

  LoginDatabaseEncryptionStatus encryption_status =
      LoginDatabaseEncryptionStatus::kNoIssues;
  if (!OSCrypt::IsEncryptionAvailable()) {
    encryption_status = LoginDatabaseEncryptionStatus::kEncryptionUnavailable;
  } else if (failed_encryption > 0) {
    encryption_status =
        LoginDatabaseEncryptionStatus::kInvalidEntriesInDatabase;
  }
  base::UmaHistogramEnumeration(
      base::StrCat({kPasswordManager, suffix_for_store,
                    ".LoginDatabaseEncryptionStatus"}),
      encryption_status);
}

void LoginDatabase::ReportMetrics() {
  TRACE_EVENT0("passwords", "LoginDatabase::ReportMetrics");

  ReportInaccessiblePasswordsMetrics();

  // BubbleSuppression fields aren't used in the account store.
  if (is_account_store_.value()) {
    return;
  }
  ReportBubbleSuppressionMetrics();
}

PasswordStoreChangeList LoginDatabase::AddLogin(const PasswordForm& form,
                                                AddCredentialError* error) {
  TRACE_EVENT0("passwords", "LoginDatabase::AddLogin");
  absl::Cleanup is_empty_runner = [this] { TriggerIsEmptyCb(); };
  if (error) {
    *error = AddCredentialError::kNone;
  }
  if (!DoesMatchConstraints(form)) {
    if (error) {
      *error = AddCredentialError::kConstraintViolation;
    }
    return PasswordStoreChangeList();
  }
  PasswordForm form_to_add = form;
#if BUILDFLAG(IS_IOS)
  // [iOS] Passwords created in Credential Provider Extension (CPE) are already
  // encrypted in the keychain and there is no need to do the process again.
  // However, the password needs to be decrypted instead so the actual password
  // syncs correctly.
  bool has_encrypted_password =
      !form.keychain_identifier.empty() && form.password_value.empty();
  if (has_encrypted_password) {
    std::u16string plaintext_password;
    if (GetTextFromKeychainIdentifier(form.keychain_identifier,
                                      &plaintext_password) != errSecSuccess) {
      if (error) {
        *error = AddCredentialError::kEncryptionServiceFailure;
      }
      return PasswordStoreChangeList();
    }
    form_to_add.password_value = plaintext_password;
  } else {
    if (!CreateKeychainIdentifier(form.password_value,
                                  &form_to_add.keychain_identifier)) {
      if (error) {
        *error = AddCredentialError::kEncryptionServiceFailure;
      }
      return PasswordStoreChangeList();
    }
  }
#else
  CHECK(form.keychain_identifier.empty());
#endif  // BUILDFLAG(IS_IOS)
  std::string encrypted_password;
  if (EncryptedString(form_to_add.password_value, &encrypted_password) !=
      EncryptionResult::kSuccess) {
    if (error) {
      *error = AddCredentialError::kEncryptionServiceFailure;
    }
    return PasswordStoreChangeList();
  }

  PasswordStoreChangeList list;
  DCHECK(!add_statement_.empty());
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, add_statement_));
  BindAddStatement(form_to_add, &s, encrypted_password);
  ScopedDbErrorHandler db_error_handler(&db_);
  const bool success = s.Run();
  if (success) {
    // If success, the row never existed so password was not changed.
    form_to_add.in_store = GetStore();
    FormPrimaryKey primary_key = FormPrimaryKey(db_.GetLastInsertRowId());
    form_to_add.primary_key = primary_key;
    if (!form_to_add.password_issues.empty()) {
      UpdateInsecureCredentials(primary_key, form_to_add.password_issues);
    }
    UpdatePasswordNotes(primary_key, form_to_add.notes);
    list.emplace_back(PasswordStoreChange::ADD, std::move(form_to_add),
                      /*password_changed=*/false);
    return list;
  }
  // Repeat the same statement but with REPLACE semantic.
  db_error_handler.reset_error_code();
  DCHECK(!add_replace_statement_.empty());
  PrimaryKeyAndPassword old_primary_key_password =
      GetPrimaryKeyAndPassword(form);
  bool password_changed =
      form_to_add.password_value != old_primary_key_password.decrypted_password;
  s.Assign(db_.GetCachedStatement(SQL_FROM_HERE, add_replace_statement_));
  BindAddStatement(form_to_add, &s, encrypted_password);
  if (s.Run()) {
    form_to_add.in_store = GetStore();
    PasswordForm removed_form = form_to_add;
    removed_form.primary_key =
        FormPrimaryKey(old_primary_key_password.primary_key);
    list.emplace_back(PasswordStoreChange::REMOVE, removed_form);

    FormPrimaryKey primary_key = FormPrimaryKey(db_.GetLastInsertRowId());
    form_to_add.primary_key = primary_key;
    InsecureCredentialsChanged insecure_changed(false);
    if (!form_to_add.password_issues.empty()) {
      insecure_changed =
          UpdateInsecureCredentials(primary_key, form_to_add.password_issues);
    }
    UpdatePasswordNotes(primary_key, form_to_add.notes);
    list.emplace_back(PasswordStoreChange::ADD, std::move(form_to_add),
                      password_changed, insecure_changed);
  } else if (error) {
    if (db_error_handler.get_error_code() == 19 /*SQLITE_CONSTRAINT*/) {
      *error = AddCredentialError::kConstraintViolation;
    } else {
      *error = AddCredentialError::kDbError;
    }
  }
  return list;
}

PasswordStoreChangeList LoginDatabase::UpdateLogin(
    const PasswordForm& form,
    UpdateCredentialError* error) {
  TRACE_EVENT0("passwords", "LoginDatabase::UpdateLogin");
  if (error) {
    *error = UpdateCredentialError::kNone;
  }
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
      EncryptionResult::kSuccess) {
    if (error) {
      *error = UpdateCredentialError::kEncryptionServiceFailure;
    }
    return PasswordStoreChangeList();
  }

  const PrimaryKeyAndPassword old_primary_key_password =
      GetPrimaryKeyAndPassword(form);

  std::string new_keychain_identifier;
#if BUILDFLAG(IS_IOS)
  DeleteEncryptedPasswordFromKeychain(
      old_primary_key_password.keychain_identifier);
  if (!CreateKeychainIdentifier(form.password_value,
                                &new_keychain_identifier)) {
    if (error) {
      *error = UpdateCredentialError::kEncryptionServiceFailure;
    }
    return PasswordStoreChangeList();
  }
#endif
  DCHECK(!update_statement_.empty());
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, update_statement_));
  int next_param = 0;
  s.BindString(next_param++, form.action.spec());
  s.BindBlob(next_param++, encrypted_password);
  s.BindString16(next_param++, form.submit_element);
  s.BindTime(next_param++, form.date_created);
  s.BindInt(next_param++, form.blocked_by_user);
  s.BindInt(next_param++, static_cast<int>(form.scheme));
  s.BindInt(next_param++, static_cast<int>(form.type));
  s.BindInt(next_param++, form.times_used_in_html_form);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s.BindBlob(next_param++, form_data_pickle);
  s.BindString16(next_param++, form.display_name);
  s.BindString(next_param++,
               form.icon_url.is_valid() ? form.icon_url.spec() : "");
  // An empty Origin serializes as "null" which would be strange to store here.
  s.BindString(next_param++, form.federation_origin.IsValid()
                                 ? form.federation_origin.Serialize()
                                 : std::string());
  s.BindInt(next_param++, form.skip_zero_click);
  s.BindInt(next_param++, static_cast<int>(form.generation_upload_status));
  base::Pickle username_pickle =
      SerializeAlternativeElementVector(form.all_alternative_usernames);
  s.BindBlob(next_param++, username_pickle);
  s.BindTime(next_param++, form.date_last_used);
  base::Pickle moving_blocked_for_pickle =
      SerializeGaiaIdHashVector(form.moving_blocked_for_list);
  s.BindBlob(next_param++, moving_blocked_for_pickle);
  s.BindTime(next_param++, form.date_password_modified);
  s.BindString16(next_param++, form.sender_email);
  s.BindString16(next_param++, form.sender_name);
  s.BindTime(next_param++, form.date_received);
  s.BindBool(next_param++, form.sharing_notification_displayed);
  s.BindBlob(next_param++, new_keychain_identifier);
  s.BindString(next_param++, form.sender_profile_image_url.is_valid()
                                 ? form.sender_profile_image_url.spec()
                                 : "");
  // NOTE: Add new fields here unless the field is a part of the unique key.
  // If so, add new field below.

  // WHERE starts here.
  s.BindString(next_param++, form.url.spec());
  s.BindString16(next_param++, form.username_element);
  s.BindString16(next_param++, form.username_value);
  s.BindString16(next_param++, form.password_element);
  s.BindString(next_param++, form.signon_realm);
  // NOTE: Add new fields here only if the field is a part of the unique key.
  // Otherwise, add the field above "WHERE starts here" comment.

  if (!s.Run()) {
    if (error) {
      *error = UpdateCredentialError::kDbError;
    }
    return PasswordStoreChangeList();
  }

  // If no rows changed due to this command, it means that there was no row to
  // update, so there is no point trying to update insecure credentials data or
  // the notes table.
  if (db_.GetLastChangeCount() == 0) {
    if (error) {
      *error = UpdateCredentialError::kNoUpdatedRecords;
    }
    return PasswordStoreChangeList();
  }

  bool password_changed =
      form.password_value != old_primary_key_password.decrypted_password;

  PasswordForm form_with_encrypted_password = form;
  form_with_encrypted_password.keychain_identifier = new_keychain_identifier;

  // TODO(crbug.com/40774419): It should be the responsibility of the caller to
  // set `password_issues` to empty.
  // Remove this once all `UpdateLogin` calls have been checked.
  if (password_changed) {
    form_with_encrypted_password.password_issues =
        base::flat_map<InsecureType, InsecurityMetadata>();
  }

  InsecureCredentialsChanged insecure_changed = UpdateInsecureCredentials(
      FormPrimaryKey(old_primary_key_password.primary_key),
      form_with_encrypted_password.password_issues);
  UpdatePasswordNotes(FormPrimaryKey(old_primary_key_password.primary_key),
                      form.notes);

  PasswordStoreChangeList list;
  form_with_encrypted_password.in_store = GetStore();
  form_with_encrypted_password.primary_key =
      FormPrimaryKey(old_primary_key_password.primary_key);
  list.emplace_back(PasswordStoreChange::UPDATE,
                    std::move(form_with_encrypted_password), password_changed,
                    insecure_changed);

  return list;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form,
                                PasswordStoreChangeList* changes) {
  TRACE_EVENT0("passwords", "LoginDatabase::RemoveLogin");
  absl::Cleanup is_empty_runner = [this] { TriggerIsEmptyCb(); };
  if (changes) {
    changes->clear();
  }
  const PrimaryKeyAndPassword old_primary_key_password =
      GetPrimaryKeyAndPassword(form);
#if BUILDFLAG(IS_IOS)
  DeleteEncryptedPasswordFromKeychain(
      old_primary_key_password.keychain_identifier);
#endif
  // Remove a login by UNIQUE-constrained fields.
  DCHECK(!delete_statement_.empty());
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, delete_statement_));
  s.BindString(0, form.url.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);
  ScopedDbErrorHandler db_error_handler(&db_);

  if (!s.Run()) {
    std::string_view suffix_for_store =
        is_account_store_.value() ? ".AccountStore" : ".ProfileStore";
    sql::UmaHistogramSqliteResult(
        base::StrCat(
            {kPasswordManager, suffix_for_store, ".RemoveLoginDBError"}),
        db_error_handler.get_error_code());
    return false;
  }
  if (db_.GetLastChangeCount() == 0) {
    return false;
  }
  if (changes) {
    PasswordForm removed_form = form;
    removed_form.in_store = GetStore();
    removed_form.primary_key =
        FormPrimaryKey(old_primary_key_password.primary_key);
    changes->emplace_back(PasswordStoreChange::REMOVE, removed_form,
                          /*password_changed=*/true);
  }
  return true;
}

bool LoginDatabase::RemoveLoginByPrimaryKey(FormPrimaryKey primary_key,
                                            PasswordStoreChangeList* changes) {
  TRACE_EVENT0("passwords", "LoginDatabase::RemoveLoginByPrimaryKey");
  CHECK(changes);

  absl::Cleanup is_empty_runner = [this] { TriggerIsEmptyCb(); };
  changes->clear();
  sql::Statement s1(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT * FROM logins WHERE id = ?"));
  s1.BindInt(0, primary_key.value());
  if (!s1.Step()) {
    return false;
  }
  PasswordForm form = GetFormWithoutPasswordFromStatement(s1);
  CHECK_EQ(form.primary_key.value(), primary_key);

#if BUILDFLAG(IS_IOS)
  DeleteEncryptedPasswordFromKeychain(form.keychain_identifier);
#endif
  DCHECK(!delete_by_id_statement_.empty());
  sql::Statement s2(
      db_.GetCachedStatement(SQL_FROM_HERE, delete_by_id_statement_));
  s2.BindInt(0, primary_key.value());
  if (!s2.Run() || db_.GetLastChangeCount() == 0) {
    return false;
  }
  if (changes) {
    form.in_store = GetStore();
    changes->emplace_back(PasswordStoreChange::REMOVE, std::move(form),
                          /*password_changed=*/true);
  }
  return true;
}

bool LoginDatabase::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeList* changes) {
  TRACE_EVENT0("passwords", "LoginDatabase::RemoveLoginsCreatedBetween");
  absl::Cleanup is_empty_runner = [this] { TriggerIsEmptyCb(); };
  if (changes) {
    changes->clear();
  }
  std::vector<PasswordForm> forms;
  ScopedTransaction transaction(this);
  if (!GetLoginsCreatedBetween(delete_begin, delete_end, &forms)) {
    return false;
  }

#if BUILDFLAG(IS_IOS)
  base::Time start = base::Time::Now();
  for (const auto& form : forms) {
    DeleteEncryptedPasswordFromKeychain(form.keychain_identifier);
  }
  base::UmaHistogramMediumTimes(
      "PasswordManager.PasswordStoreBuiltInBackend.RemoveLoginsCreatedBetween."
      "KeychainLatency",
      base::Time::Now() - start);
#endif

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM logins WHERE "
                             "date_created >= ? AND date_created <= ?"));
  s.BindTime(0, delete_begin);
  s.BindTime(1, delete_end.is_null() ? base::Time::Max() : delete_end);

  if (!s.Run()) {
    return false;
  }
  if (changes) {
    for (auto& form : forms) {
      changes->emplace_back(PasswordStoreChange::REMOVE, std::move(form),
                            /*password_changed=*/true);
    }
  }
  return true;
}

bool LoginDatabase::GetAutoSignInLogins(std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAutoSignInLogins");
  CHECK(forms);
  CHECK(!autosignin_statement_.empty());
  forms->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, autosignin_statement_));
  FormRetrievalResult result = StatementToForms(&s, nullptr, forms);
  return (result == FormRetrievalResult::kSuccess ||
          result ==
              FormRetrievalResult::kEncryptionServiceFailureWithPartialData);
}

bool LoginDatabase::DisableAutoSignInForOrigin(const GURL& origin) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE logins SET skip_zero_click = 1 WHERE origin_url = ?;"));
  s.BindString(0, origin.spec());

  return s.Run();
}

PasswordForm LoginDatabase::GetFormWithoutPasswordFromStatement(
    sql::Statement& s) const {
  PasswordForm form;
  form.primary_key = FormPrimaryKey(s.ColumnInt(COLUMN_ID));
  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form.url = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form.action = GURL(tmp);
  form.username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form.username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form.password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  s.ColumnBlobAsString(COLUMN_KEYCHAIN_IDENTIFIER, &form.keychain_identifier);
  form.submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form.signon_realm = tmp;
  form.date_created = s.ColumnTime(COLUMN_DATE_CREATED);
  form.blocked_by_user = (s.ColumnInt(COLUMN_BLOCKLISTED_BY_USER) > 0);
  // TODO(crbug.com/40732888): Add metrics to capture how often these values
  // fall out of the valid enum range.
  form.scheme = static_cast<PasswordForm::Scheme>(s.ColumnInt(COLUMN_SCHEME));
  form.type =
      static_cast<PasswordForm::Type>(s.ColumnInt(COLUMN_PASSWORD_TYPE));
  base::span<const uint8_t> possible_username_pairs_blob =
      s.ColumnBlob(COLUMN_POSSIBLE_USERNAME_PAIRS);
  if (!possible_username_pairs_blob.empty()) {
    base::Pickle pickle =
        base::Pickle::WithUnownedBuffer(possible_username_pairs_blob);
    form.all_alternative_usernames =
        DeserializeAlternativeElementVector(pickle);
  }
  form.times_used_in_html_form = s.ColumnInt(COLUMN_TIMES_USED);
  base::span<const uint8_t> form_data_blob = s.ColumnBlob(COLUMN_FORM_DATA);
  if (!form_data_blob.empty()) {
    base::Pickle form_data_pickle =
        base::Pickle::WithUnownedBuffer(form_data_blob);
    base::PickleIterator form_data_iter(form_data_pickle);
    autofill::DeserializeFormData(&form_data_iter, &form.form_data);
  }
  form.display_name = s.ColumnString16(COLUMN_DISPLAY_NAME);
  form.icon_url = GURL(s.ColumnString(COLUMN_ICON_URL));
  form.federation_origin =
      url::SchemeHostPort(GURL(s.ColumnString(COLUMN_FEDERATION_URL)));
  form.skip_zero_click = (s.ColumnInt(COLUMN_SKIP_ZERO_CLICK) > 0);
  form.generation_upload_status =
      static_cast<PasswordForm::GenerationUploadStatus>(
          s.ColumnInt(COLUMN_GENERATION_UPLOAD_STATUS));
  form.date_last_used = s.ColumnTime(COLUMN_DATE_LAST_USED);
  base::span<const uint8_t> moving_blocked_for_blob =
      s.ColumnBlob(COLUMN_MOVING_BLOCKED_FOR);
  if (!moving_blocked_for_blob.empty()) {
    base::Pickle pickle =
        base::Pickle::WithUnownedBuffer(moving_blocked_for_blob);
    form.moving_blocked_for_list = DeserializeGaiaIdHashVector(pickle);
  }
  form.date_password_modified = s.ColumnTime(COLUMN_DATE_PASSWORD_MODIFIED);
  form.sender_email = s.ColumnString16(COLUMN_SENDER_EMAIL);
  form.sender_name = s.ColumnString16(COLUMN_SENDER_NAME);
  form.sender_profile_image_url =
      GURL(s.ColumnString(COLUMN_SENDER_PROFILE_IMAGE_URL));
  form.date_received = s.ColumnTime(COLUMN_DATE_RECEIVED);
  form.sharing_notification_displayed =
      s.ColumnBool(COLUMN_SHARING_NOTIFICATION_DISPLAYED);

  CHECK(form.primary_key.has_value());
  form.password_issues = GetPasswordIssues(form.primary_key.value());
  form.notes = GetPasswordNotes(form.primary_key.value());

  return form;
}

bool LoginDatabase::GetLogins(const PasswordFormDigest& form,
                              bool should_PSL_matching_apply,
                              std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetLogins");
  CHECK(forms);
  forms->clear();

  const bool should_federated_apply =
      form.scheme == PasswordForm::Scheme::kHtml;
  CHECK(!get_statement_.empty());
  CHECK(!get_statement_psl_.empty());
  CHECK(!get_statement_federated_.empty());
  CHECK(!get_statement_psl_federated_.empty());
  const std::string* sql_query = &get_statement_;
  if (should_PSL_matching_apply && should_federated_apply) {
    sql_query = &get_statement_psl_federated_;
  } else if (should_PSL_matching_apply) {
    sql_query = &get_statement_psl_;
  } else if (should_federated_apply) {
    sql_query = &get_statement_federated_;
  }

  // TODO(nyquist) Consider usage of GetCachedStatement when
  // http://crbug.com/248608 is fixed.
  sql::Statement s(db_.GetUniqueStatement(*sql_query));
  s.BindString(0, form.signon_realm);
  int placeholder = 1;

  // PSL matching only applies to HTML forms.
  if (should_PSL_matching_apply) {
    s.BindString(placeholder++, GetRegexForPSLMatching(form.signon_realm));

    if (should_federated_apply) {
      // This regex matches any subdomain of |registered_domain|, in particular
      // it matches the empty subdomain. Hence exact domain matches are also
      // retrieved.
      s.BindString(placeholder++,
                   GetRegexForPSLFederatedMatching(form.signon_realm));
    }
  } else if (should_federated_apply) {
    s.BindString(placeholder++,
                 GetExpressionForFederatedMatching(form.url) + "%");
  }
  FormRetrievalResult result = StatementToForms(
      &s, should_PSL_matching_apply || should_federated_apply ? &form : nullptr,
      forms);
  if (result != FormRetrievalResult::kSuccess &&
      result != FormRetrievalResult::kEncryptionServiceFailureWithPartialData) {
    forms->clear();
    return false;
  }
  return true;
}

bool LoginDatabase::GetLoginsCreatedBetween(const base::Time begin,
                                            const base::Time end,
                                            std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetLoginsCreatedBetween");
  CHECK(forms);
  CHECK(!created_statement_.empty());
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, created_statement_));
  s.BindTime(0, begin);
  s.BindTime(1, end.is_null() ? base::Time::Max() : end);

  return StatementToForms(&s, nullptr, forms) == FormRetrievalResult::kSuccess;
}

FormRetrievalResult LoginDatabase::GetAllLogins(
    std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAllLogins");
  DCHECK(forms);
  forms->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, "SELECT * FROM logins"));

  return StatementToForms(&s, nullptr, forms);
}

FormRetrievalResult LoginDatabase::GetLoginsBySignonRealmAndUsername(
    const std::string& signon_realm,
    const std::u16string& username,
    std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetLoginsBySignonRealmAndUsername");
  forms->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, get_statement_username_));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);

  return StatementToForms(&s, nullptr, forms);
}

bool LoginDatabase::GetAutofillableLogins(std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetAutofillableLogins");
  return GetAllLoginsWithBlocklistSetting(false, forms);
}

bool LoginDatabase::GetBlocklistLogins(std::vector<PasswordForm>* forms) {
  TRACE_EVENT0("passwords", "LoginDatabase::GetBlocklistLogins");
  return GetAllLoginsWithBlocklistSetting(true, forms);
}

bool LoginDatabase::GetAllLoginsWithBlocklistSetting(
    bool blocklisted,
    std::vector<PasswordForm>* forms) {
  DCHECK(forms);
  DCHECK(!blocklisted_statement_.empty());
  forms->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, blocklisted_statement_));
  s.BindInt(0, blocklisted ? 1 : 0);

  FormRetrievalResult result = StatementToForms(&s, nullptr, forms);
  if (result != FormRetrievalResult::kSuccess &&
      result != FormRetrievalResult::kEncryptionServiceFailureWithPartialData) {
    forms->clear();
    return false;
  }

  return true;
}

LoginDatabase::LoginDatabaseEmptinessState LoginDatabase::IsEmpty() {
  sql::Statement count_all_logins(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT EXISTS(SELECT 1 FROM logins)"));
  // `blacklisted_by_user = 0` means the entry is not a blocklisted entry.
  // `LENGTH(federation_url) = 0` means the entry is not a federated credential.
  // `scheme <> 4` means the entry is not a username-only credential.
  sql::Statement count_autofillable_credentials(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT EXISTS(SELECT 1 FROM logins WHERE blacklisted_by_user = 0 AND "
      "LENGTH(federation_url) = 0 AND scheme <> 4)"));

  return LoginDatabase::LoginDatabaseEmptinessState{
      .no_login_found =
          (count_all_logins.Step() && count_all_logins.ColumnInt(0) == 0),
      .autofillable_credentials_exist =
          (count_autofillable_credentials.Step() &&
           count_autofillable_credentials.ColumnInt(0) > 0)};
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  TRACE_EVENT0("passwords", "LoginDatabase::DeleteAndRecreateDatabaseFile");
  DCHECK(db_.is_open());

#if BUILDFLAG(IS_IOS)
  {  // Scope the statement so the database closes properly.
    // Clear keychain on iOS before deleting passwords.
    sql::Statement s(
        db_.GetUniqueStatement("SELECT keychain_identifier FROM logins"));
    while (s.Step()) {
      std::string keychain_identifier;
      s.ColumnBlobAsString(0, &keychain_identifier);
      DeleteEncryptedPasswordFromKeychain(keychain_identifier);
    }
  }
#endif

  meta_table_.Reset();
  db_.Close();
  sql::Database::Delete(db_path_);
  return Init(std::move(on_undecryptable_passwords_removed_),
              std::move(encryptor_));
}

DatabaseCleanupResult LoginDatabase::DeleteUndecryptableLogins() {
  TRACE_EVENT0("passwords", "LoginDatabase::DeleteUndecryptableLogins");
  absl::Cleanup is_empty_runner = [this] { TriggerIsEmptyCb(); };
  // If the Keychain in MacOS or the real secret key in Linux is unavailable,
  // don't delete any logins.
  if (!OSCrypt::IsEncryptionAvailable()) {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kEncryptionUnavailable);
    return DatabaseCleanupResult::kEncryptionUnavailable;
  }

  DCHECK(db_.is_open());

  sql::Statement s(db_.GetUniqueStatement("SELECT * FROM logins"));

  std::vector<PasswordForm> forms_to_be_deleted;

  while (s.Step()) {
    std::string encrypted_password;
    s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
    std::u16string decrypted_password;
    if (DecryptedString(encrypted_password, &decrypted_password) ==
        EncryptionResult::kSuccess) {
      continue;
    }

    // If it was not possible to decrypt the password, remove it from the
    // database.
    forms_to_be_deleted.push_back(GetFormForRemoval(s));
  }

  for (const auto& form : forms_to_be_deleted) {
    if (!RemoveLogin(form, nullptr)) {
      metrics_util::LogDeleteUndecryptableLoginsReturnValue(
          metrics_util::DeleteCorruptedPasswordsResult::kItemFailure);
      return DatabaseCleanupResult::kItemFailure;
    }
  }

  if (forms_to_be_deleted.empty()) {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessNoDeletions);
  } else {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted);
  }

  return DatabaseCleanupResult::kSuccess;
}

bool LoginDatabase::BeginTransaction() {
  TRACE_EVENT0("passwords", "LoginDatabase::BeginTransaction");
  return db_.BeginTransactionDeprecated();
}

void LoginDatabase::RollbackTransaction() {
  TRACE_EVENT0("passwords", "LoginDatabase::RollbackTransaction");
  db_.RollbackTransactionDeprecated();
}

bool LoginDatabase::CommitTransaction() {
  TRACE_EVENT0("passwords", "LoginDatabase::CommitTransaction");
  return db_.CommitTransactionDeprecated();
}

void LoginDatabase::SetIsEmptyCb(IsEmptyCallback is_empty_cb) {
  is_empty_cb_ = std::move(is_empty_cb);
}

LoginDatabase::SyncMetadataStore::SyncMetadataStore(sql::Database* db)
    : db_(db) {
  CHECK(db);
}

LoginDatabase::SyncMetadataStore::~SyncMetadataStore() = default;

std::unique_ptr<sync_pb::EntityMetadata>
LoginDatabase::SyncMetadataStore::GetSyncEntityMetadataForStorageKeyForTest(
    syncer::DataType data_type,
    const std::string& storage_key) {
  return GetSyncEntityMetadataForStorageKey(data_type, storage_key);
}

std::unique_ptr<syncer::MetadataBatch>
LoginDatabase::SyncMetadataStore::GetAllSyncEntityMetadata(
    syncer::DataType data_type) {
  CHECK_EQ(data_type, syncer::PASSWORDS);
  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT storage_key, metadata FROM %s",
                         kPasswordsSyncEntitiesMetadataTableName)));

  while (s.Step()) {
    int storage_key_int = s.ColumnInt(0);
    std::string storage_key = base::NumberToString(storage_key_int);
    std::unique_ptr<sync_pb::EntityMetadata> entity_metadata =
        DecryptAndParseSyncEntityMetadata(s.ColumnString(1));
    if (!entity_metadata) {
      return nullptr;
    }
    metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
  }
  if (!s.Succeeded()) {
    return nullptr;
  }
  return metadata_batch;
}

std::unique_ptr<sync_pb::EntityMetadata>
LoginDatabase::SyncMetadataStore::GetSyncEntityMetadataForStorageKey(
    syncer::DataType data_type,
    const std::string& storage_key) {
  CHECK_EQ(data_type, syncer::PASSWORDS);

  int storage_key_int = 0;
  if (!base::StringToInt(storage_key, &storage_key_int)) {
    DLOG(ERROR) << "Invalid storage key. Failed to convert the storage key to "
                   "an integer.";
    return nullptr;
  }

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT metadata FROM %s WHERE storage_key=?",
                         kPasswordsSyncEntitiesMetadataTableName)));
  s.BindInt(0, storage_key_int);

  if (!s.Step()) {
    // No entity metadata found for this storage key.
    return nullptr;
  }

  return DecryptAndParseSyncEntityMetadata(s.ColumnString(0));
}

std::unique_ptr<sync_pb::DataTypeState>
LoginDatabase::SyncMetadataStore::GetDataTypeState(syncer::DataType data_type) {
  CHECK_EQ(data_type, syncer::PASSWORDS);
  auto state = std::make_unique<sync_pb::DataTypeState>();
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT model_metadata FROM %s WHERE id=1",
                         kPasswordsSyncModelMetadataTableName)));

  if (!s.Step()) {
    if (s.Succeeded()) {
      return state;
    } else {
      return nullptr;
    }
  }

  std::string serialized_state = s.ColumnString(0);
  if (state->ParseFromString(serialized_state)) {
    return state;
  }
  return nullptr;
}

std::unique_ptr<syncer::MetadataBatch>
LoginDatabase::SyncMetadataStore::GetAllSyncMetadata(
    syncer::DataType data_type) {
  TRACE_EVENT0("passwords", "SyncMetadataStore::GetAllSyncMetadata");
  CHECK_EQ(data_type, syncer::PASSWORDS);
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      GetAllSyncEntityMetadata(data_type);
  if (metadata_batch == nullptr) {
    return nullptr;
  }

  std::unique_ptr<sync_pb::DataTypeState> data_type_state =
      GetDataTypeState(data_type);
  if (data_type_state == nullptr) {
    return nullptr;
  }

  metadata_batch->SetDataTypeState(*data_type_state);
  return metadata_batch;
}

void LoginDatabase::SyncMetadataStore::DeleteAllSyncMetadata(
    syncer::DataType data_type) {
  TRACE_EVENT0("passwords", "SyncMetadataStore::DeleteAllSyncMetadata");
  CHECK_EQ(data_type, syncer::PASSWORDS);
  CHECK_EQ(data_type, syncer::PASSWORDS);
  bool had_unsynced_password_deletions = HasUnsyncedPasswordDeletions();
  ClearAllSyncMetadata(db_, data_type);
  if (had_unsynced_password_deletions &&
      password_deletions_have_synced_callback_) {
    // Note: At this point we can't be fully sure whether the deletions actually
    // reached the server yet. We might have sent a commit, but haven't received
    // the commit confirmation. Let's be conservative and assume they haven't
    // been successfully deleted.
    password_deletions_have_synced_callback_.Run(/*success=*/false);
  }
}

bool LoginDatabase::SyncMetadataStore::UpdateEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  TRACE_EVENT0("passwords", "SyncMetadataStore::UpdateSyncMetadata");
  CHECK_EQ(data_type, syncer::PASSWORDS);

  int storage_key_int = 0;
  if (!base::StringToInt(storage_key, &storage_key_int)) {
    DLOG(ERROR) << "Invalid storage key. Failed to convert the storage key to "
                   "an integer.";
    return false;
  }

  std::string encrypted_metadata;
  if (!OSCrypt::EncryptString(metadata.SerializeAsString(),
                              &encrypted_metadata)) {
    DLOG(ERROR) << "Cannot encrypt the sync metadata";
    return false;
  }

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT OR REPLACE INTO %s (storage_key, metadata) VALUES(?, ?)",
          kPasswordsSyncEntitiesMetadataTableName)));

  s.BindInt(0, storage_key_int);
  s.BindString(1, encrypted_metadata);
  if (data_type != syncer::PASSWORDS) {
    return s.Run();
  }
  CHECK_EQ(data_type, syncer::PASSWORDS);

  // This ongoing operation may influence the value returned by
  // HasUnsyncedPasswordDeletions() only if the storage key being updated
  // represents a pending deletion AND the new metadata is not (necessary but
  // not sufficient condition). Because HasUnsyncedPasswordDeletions() may be
  // expensive, it is evaluated lazily to avoid performance issues.
  //
  // Note: No need for an explicit "is unsynced" check: Once the deletion is
  // committed, the metadata entry is removed.
  std::unique_ptr<sync_pb::EntityMetadata> previous_metadata =
      GetSyncEntityMetadataForStorageKey(data_type, storage_key);
  bool was_unsynced_deletion =
      previous_metadata && previous_metadata->is_deleted();

  bool result = s.Run();
  if (result && was_unsynced_deletion && !metadata.is_deleted() &&
      !HasUnsyncedPasswordDeletions() &&
      password_deletions_have_synced_callback_) {
    password_deletions_have_synced_callback_.Run(/*success=*/true);
  }
  return result;
}

bool LoginDatabase::SyncMetadataStore::ClearEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key) {
  TRACE_EVENT0("passwords", "SyncMetadataStore::ClearSyncMetadata");
  CHECK_EQ(data_type, syncer::PASSWORDS);

  int storage_key_int = 0;
  if (!base::StringToInt(storage_key, &storage_key_int)) {
    DLOG(ERROR) << "Invalid storage key. Failed to convert the storage key to "
                   "an integer.";
    return false;
  }

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE storage_key=?",
                         kPasswordsSyncEntitiesMetadataTableName)));
  s.BindInt(0, storage_key_int);
  if (data_type != syncer::PASSWORDS) {
    return s.Run();
  }
  CHECK_EQ(data_type, syncer::PASSWORDS);

  // This ongoing operation may influence the value returned by
  // HasUnsyncedPasswordDeletions() only if the storage key being cleared
  // represents a pending deletion (necessary but not sufficient condition).
  // Because HasUnsyncedPasswordDeletions() may be expensive, it is evaluated
  // lazily to avoid performance issues.
  //
  // Note: No need for an explicit "is unsynced" check: Once the deletion is
  // committed, the metadata entry is removed.
  std::unique_ptr<sync_pb::EntityMetadata> previous_metadata =
      GetSyncEntityMetadataForStorageKey(data_type, storage_key);
  bool was_unsynced_deletion =
      previous_metadata && previous_metadata->is_deleted();

  bool result = s.Run();
  if (result && was_unsynced_deletion && !HasUnsyncedPasswordDeletions() &&
      password_deletions_have_synced_callback_) {
    password_deletions_have_synced_callback_.Run(/*success=*/true);
  }
  return result;
}

bool LoginDatabase::SyncMetadataStore::UpdateDataTypeState(
    syncer::DataType data_type,
    const sync_pb::DataTypeState& data_type_state) {
  TRACE_EVENT0("passwords", "SyncMetadataStore::UpdateDataTypeState");
  CHECK_EQ(data_type, syncer::PASSWORDS);

  // Make sure only one row is left by storing it in the entry with id=1
  // every time.
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("INSERT OR REPLACE INTO %s (id, model_metadata) "
                         "VALUES(1, ?)",
                         kPasswordsSyncModelMetadataTableName)));
  s.BindString(0, data_type_state.SerializeAsString());

  return s.Run();
}

bool LoginDatabase::SyncMetadataStore::ClearDataTypeState(
    syncer::DataType data_type) {
  TRACE_EVENT0("passwords", "SyncMetadataStore::ClearDataTypeState");
  CHECK_EQ(data_type, syncer::PASSWORDS);

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf("DELETE FROM %s WHERE id=1",
                                        kPasswordsSyncModelMetadataTableName)));

  return s.Run();
}

void LoginDatabase::SyncMetadataStore::SetPasswordDeletionsHaveSyncedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  password_deletions_have_synced_callback_ = std::move(callback);
}

bool LoginDatabase::SyncMetadataStore::HasUnsyncedPasswordDeletions() {
  TRACE_EVENT0("passwords", "SyncMetadataStore::HasUnsyncedDeletions");

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("SELECT metadata FROM %s",
                         kPasswordsSyncEntitiesMetadataTableName)));

  while (s.Step()) {
    std::unique_ptr<sync_pb::EntityMetadata> entity_metadata =
        DecryptAndParseSyncEntityMetadata(s.ColumnString(0));
    if (!entity_metadata) {
      return false;
    }
    // Note: No need for an explicit "is unsynced" check: Once the deletion is
    // committed, the metadata entry is removed.
    if (entity_metadata->is_deleted()) {
      return true;
    }
  }

  return false;
}

LoginDatabase::PrimaryKeyAndPassword LoginDatabase::GetPrimaryKeyAndPassword(
    const PasswordForm& form) const {
  DCHECK(!id_and_password_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, id_and_password_statement_));

  s.BindString(0, form.url.is_valid() ? form.url.spec() : std::string_view());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);

  if (s.Step()) {
    PrimaryKeyAndPassword result = {s.ColumnInt(0)};
    std::string encrypted_password;
    s.ColumnBlobAsString(1, &encrypted_password);
    s.ColumnBlobAsString(2, &result.keychain_identifier);
    if (DecryptedString(encrypted_password, &result.decrypted_password) !=
        EncryptionResult::kSuccess) {
      result.decrypted_password.clear();
    }
    return result;
  }
  return {-1, std::u16string(), std::string()};
}

FormRetrievalResult LoginDatabase::StatementToForms(
    sql::Statement* statement,
    const PasswordFormDigest* matched_form,
    std::vector<PasswordForm>* forms) {
  DCHECK(forms);
  forms->clear();
  bool failed = false;

  // Since this member is only used to trigger sync, it should maintain the most
  // relevant value for the PasswordSyncBridge. Which means if there were no
  // reads assume that the read was successful and there were no deletions.
  // Deletion will update this member's value to true, only starting sync can
  // set it back to false. This way PasswordSyncBridge will always know whether
  // the sync should happen or no, no matter how many calls happen before that.
  if (!were_undecryptable_logins_deleted_.has_value()) {
    were_undecryptable_logins_deleted_ = false;
  }

  while (statement->Step()) {
    std::u16string plaintext_password;
    EncryptionResult result =
        DecryptPasswordFromStatement(*statement, &plaintext_password, this);
    if (result == EncryptionResult::kItemFailure ||
        result == EncryptionResult::kServiceFailure) {
      failed = true;
      continue;
    }

    DCHECK_EQ(EncryptionResult::kSuccess, result);

    PasswordForm form = GetFormWithoutPasswordFromStatement(*statement);
    form.password_value = std::move(plaintext_password);
    form.in_store = GetStore();

    if (matched_form &&
        GetMatchResult(form, *matched_form) == MatchResult::NO_MATCH) {
      continue;
    }

    forms->push_back(std::move(form));
  }

  if (!statement->Succeeded()) {
    return FormRetrievalResult::kDbError;
  }
  if (failed) {
    if (ShouldDeleteUndecryptablePasswords(
            on_undecryptable_passwords_removed_, is_user_data_dir_policy_set_,
            is_deleting_undecryptable_logins_enabled_by_policy_.value(),
            is_account_store_)) {
      DatabaseCleanupResult result = DeleteUndecryptableLogins();
      if (result == DatabaseCleanupResult::kSuccess) {
        were_undecryptable_logins_deleted_ = true;
        return FormRetrievalResult::kSuccess;
      }
    }
    if (ShouldReturnPartialPasswords()) {
      return FormRetrievalResult::kEncryptionServiceFailureWithPartialData;
    }

    return FormRetrievalResult::kEncryptionServiceFailure;
  }
  return FormRetrievalResult::kSuccess;
}

void LoginDatabase::InitializeStatementStrings(const SQLTableBuilder& builder) {
  // This method may be called multiple times, if Chrome switches backends and
  // LoginDatabase::DeleteAndRecreateDatabaseFile ends up being called. In those
  // case do not recompute the SQL statements, because they would end up the
  // same.
  if (!add_statement_.empty()) {
    return;
  }

  // Initialize the cached strings.
  std::string all_column_names = builder.ListAllColumnNames();
  std::string right_amount_of_placeholders =
      GeneratePlaceholders(builder.NumberOfColumns());
  std::string all_unique_key_column_names = builder.ListAllUniqueKeyNames();
  std::string all_nonunique_key_column_names =
      builder.ListAllNonuniqueKeyNames();

  add_statement_ = "INSERT INTO logins (" + all_column_names + ") VALUES " +
                   right_amount_of_placeholders;
  DCHECK(add_replace_statement_.empty());
  add_replace_statement_ = "INSERT OR REPLACE INTO logins (" +
                           all_column_names + ") VALUES " +
                           right_amount_of_placeholders;
  DCHECK(update_statement_.empty());
  update_statement_ = "UPDATE logins SET " + all_nonunique_key_column_names +
                      " WHERE " + all_unique_key_column_names;
  DCHECK(delete_statement_.empty());
  delete_statement_ = "DELETE FROM logins WHERE " + all_unique_key_column_names;
  DCHECK(delete_by_id_statement_.empty());
  delete_by_id_statement_ = "DELETE FROM logins WHERE id=?";
  DCHECK(autosignin_statement_.empty());
  autosignin_statement_ = "SELECT " + all_column_names +
                          " FROM logins "
                          "WHERE skip_zero_click = 0 ORDER BY origin_url";
  DCHECK(get_statement_.empty());
  get_statement_ = "SELECT " + all_column_names +
                   " FROM logins "
                   "WHERE signon_realm == ?";
  std::string psl_statement = "OR signon_realm REGEXP ? ";
  std::string federated_statement =
      "OR (signon_realm LIKE ? AND password_type == 2) ";
  std::string psl_federated_statement =
      "OR (signon_realm REGEXP ? AND password_type == 2) ";
  DCHECK(get_statement_psl_.empty());
  get_statement_psl_ = get_statement_ + psl_statement;
  DCHECK(get_statement_federated_.empty());
  get_statement_federated_ = get_statement_ + federated_statement;
  DCHECK(get_statement_psl_federated_.empty());
  get_statement_psl_federated_ =
      get_statement_ + psl_statement + psl_federated_statement;
  DCHECK(get_statement_username_.empty());
  get_statement_username_ = get_statement_ + " AND username_value == ?";
  DCHECK(created_statement_.empty());
  created_statement_ =
      "SELECT " + all_column_names +
      " FROM logins WHERE date_created >= ? AND date_created <= "
      "? ORDER BY origin_url";
  DCHECK(blocklisted_statement_.empty());
  blocklisted_statement_ =
      "SELECT " + all_column_names +
      " FROM logins WHERE blacklisted_by_user == ? ORDER BY origin_url";
  DCHECK(id_and_password_statement_.empty());
  id_and_password_statement_ =
      "SELECT id, password_value, keychain_identifier FROM logins WHERE " +
      all_unique_key_column_names;
}

PasswordForm::Store LoginDatabase::GetStore() const {
  return is_account_store() ? PasswordForm::Store::kAccountStore
                            : PasswordForm::Store::kProfileStore;
}

base::flat_map<InsecureType, InsecurityMetadata>
LoginDatabase::GetPasswordIssues(FormPrimaryKey primary_key) const {
  std::vector<InsecureCredential> insecure_credentials =
      insecure_credentials_table_.GetRows(primary_key);
  base::flat_map<InsecureType, InsecurityMetadata> issues;
  for (const auto& insecure_credential : insecure_credentials) {
    issues[insecure_credential.insecure_type] = InsecurityMetadata(
        insecure_credential.create_time, insecure_credential.is_muted,
        insecure_credential.trigger_notification_from_backend);
  }
  return issues;
}

InsecureCredentialsChanged LoginDatabase::UpdateInsecureCredentials(
    FormPrimaryKey primary_key,
    const base::flat_map<InsecureType, InsecurityMetadata>& password_issues) {
  bool changed = false;
  for (const auto& password_issue : password_issues) {
    changed = insecure_credentials_table_.InsertOrReplace(
                  primary_key, password_issue.first, password_issue.second) ||
              changed;
  }

  // If an insecure type has been removed from the form it has to be removed
  // from the database. This can currently happen for phished entries.
  for (auto insecure_type : {InsecureType::kLeaked, InsecureType::kPhished,
                             InsecureType::kWeak, InsecureType::kReused}) {
    if (password_issues.find(insecure_type) == password_issues.end()) {
      changed =
          insecure_credentials_table_.RemoveRow(primary_key, insecure_type) ||
          changed;
    }
  }
  return InsecureCredentialsChanged(changed);
}

std::vector<PasswordNote> LoginDatabase::GetPasswordNotes(
    FormPrimaryKey primary_key) const {
  return password_notes_table_.GetPasswordNotes(primary_key);
}

void LoginDatabase::UpdatePasswordNotes(
    FormPrimaryKey primary_key,
    const std::vector<PasswordNote>& notes) {
  password_notes_table_.RemovePasswordNotes(primary_key);
  for (const PasswordNote& note : notes) {
    password_notes_table_.InsertOrReplace(primary_key, note);
  }
}

void LoginDatabase::TriggerIsEmptyCb() {
  if (is_empty_cb_) {
    is_empty_cb_.Run(IsEmpty());
  }
}

}  // namespace password_manager
