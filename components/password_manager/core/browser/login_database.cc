// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/common/password_form.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/sql_table_builder.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"
#include "url/url_constants.h"

using autofill::PasswordForm;

namespace password_manager {

// The current version number of the login database schema.
const int kCurrentVersionNumber = 26;
// The oldest version of the schema such that a legacy Chrome client using that
// version can still read/write the current database.
const int kCompatibleVersionNumber = 19;

base::Pickle SerializeValueElementPairs(
    const autofill::ValueElementVector& vec) {
  base::Pickle p;
  for (size_t i = 0; i < vec.size(); ++i) {
    p.WriteString16(vec[i].first);
    p.WriteString16(vec[i].second);
  }
  return p;
}

autofill::ValueElementVector DeserializeValueElementPairs(
    const base::Pickle& p) {
  autofill::ValueElementVector ret;
  base::string16 value;
  base::string16 field_name;

  base::PickleIterator iterator(p);
  while (iterator.ReadString16(&value)) {
    bool name_success = iterator.ReadString16(&field_name);
    DCHECK(name_success);
    ret.push_back(autofill::ValueElementPair(value, field_name));
  }
  return ret;
}

namespace {

// A simple class for scoping a login database transaction. This does not
// support rollback since the login database doesn't either.
class ScopedTransaction {
 public:
  explicit ScopedTransaction(LoginDatabase* db) : db_(db) {
    db_->BeginTransaction();
  }
  ~ScopedTransaction() { db_->CommitTransaction(); }

 private:
  LoginDatabase* db_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTransaction);
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
  COLUMN_PREFERRED,
  COLUMN_DATE_CREATED,
  COLUMN_BLACKLISTED_BY_USER,
  COLUMN_SCHEME,
  COLUMN_PASSWORD_TYPE,
  COLUMN_TIMES_USED,
  COLUMN_FORM_DATA,
  COLUMN_DATE_SYNCED,
  COLUMN_DISPLAY_NAME,
  COLUMN_ICON_URL,
  COLUMN_FEDERATION_URL,
  COLUMN_SKIP_ZERO_CLICK,
  COLUMN_GENERATION_UPLOAD_STATUS,
  COLUMN_POSSIBLE_USERNAME_PAIRS,
  COLUMN_ID,
  COLUMN_DATE_LAST_USED,
  COLUMN_NUM  // Keep this last.
};

enum class HistogramSize { SMALL, LARGE };

// An enum for UMA reporting. Add values to the end only.
enum DatabaseInitError {
  INIT_OK,
  OPEN_FILE_ERROR,
  START_TRANSACTION_ERROR,
  META_TABLE_INIT_ERROR,
  INCOMPATIBLE_VERSION,
  INIT_LOGINS_ERROR,
  INIT_STATS_ERROR,
  MIGRATION_ERROR,
  COMMIT_TRANSACTION_ERROR,
  INIT_COMPROMISED_CREDENTIALS_ERROR,
  INIT_FIELD_INFO_ERROR,
  DATABASE_INIT_ERROR_COUNT,
};

// Struct to hold table builder for "logins", "sync_entities_metadata", and
// "sync_model_metadata" tables.
struct SQLTableBuilders {
  SQLTableBuilder* logins;
  SQLTableBuilder* sync_entities_metadata;
  SQLTableBuilder* sync_model_metadata;
};

void BindAddStatement(const PasswordForm& form,
                      const std::string& encrypted_password,
                      sql::Statement* s) {
  s->BindString(COLUMN_ORIGIN_URL, form.origin.spec());
  s->BindString(COLUMN_ACTION_URL, form.action.spec());
  s->BindString16(COLUMN_USERNAME_ELEMENT, form.username_element);
  s->BindString16(COLUMN_USERNAME_VALUE, form.username_value);
  s->BindString16(COLUMN_PASSWORD_ELEMENT, form.password_element);
  s->BindBlob(COLUMN_PASSWORD_VALUE, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s->BindString16(COLUMN_SUBMIT_ELEMENT, form.submit_element);
  s->BindString(COLUMN_SIGNON_REALM, form.signon_realm);
  s->BindInt(COLUMN_PREFERRED, form.preferred);
  s->BindInt64(COLUMN_DATE_CREATED, form.date_created.ToInternalValue());
  s->BindInt(COLUMN_BLACKLISTED_BY_USER, form.blacklisted_by_user);
  s->BindInt(COLUMN_SCHEME, static_cast<int>(form.scheme));
  s->BindInt(COLUMN_PASSWORD_TYPE, static_cast<int>(form.type));
  s->BindInt(COLUMN_TIMES_USED, form.times_used);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s->BindBlob(COLUMN_FORM_DATA,
              form_data_pickle.data(),
              form_data_pickle.size());
  s->BindInt64(COLUMN_DATE_SYNCED, form.date_synced.ToInternalValue());
  s->BindString16(COLUMN_DISPLAY_NAME, form.display_name);
  s->BindString(COLUMN_ICON_URL, form.icon_url.spec());
  // An empty Origin serializes as "null" which would be strange to store here.
  s->BindString(COLUMN_FEDERATION_URL,
                form.federation_origin.opaque()
                    ? std::string()
                    : form.federation_origin.Serialize());
  s->BindInt(COLUMN_SKIP_ZERO_CLICK, form.skip_zero_click);
  s->BindInt(COLUMN_GENERATION_UPLOAD_STATUS,
             static_cast<int>(form.generation_upload_status));
  base::Pickle usernames_pickle =
      SerializeValueElementPairs(form.all_possible_usernames);
  s->BindBlob(COLUMN_POSSIBLE_USERNAME_PAIRS, usernames_pickle.data(),
              usernames_pickle.size());
  s->BindInt64(COLUMN_DATE_LAST_USED,
               form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Output parameter is the first one because of binding order.
void AddCallback(int* output_err, int err, sql::Statement* /*stmt*/) {
  DCHECK(output_err);
  *output_err = err;
  if (err == 19 /*SQLITE_CONSTRAINT*/)
    DLOG(WARNING) << "LoginDatabase::AddLogin updated an existing form";
}

bool DoesMatchConstraints(const PasswordForm& form) {
  if (!IsValidAndroidFacetURI(form.signon_realm) && form.origin.is_empty()) {
    DLOG(ERROR) << "Constraint violation: form.origin is empty";
    return false;
  }
  if (form.signon_realm.empty()) {
    DLOG(ERROR) << "Constraint violation: form.signon_realm is empty";
    return false;
  }
  return true;
}

void LogDatabaseInitError(DatabaseInitError error) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.LoginDatabaseInit", error,
                            DATABASE_INIT_ERROR_COUNT);
}

void LogAccountStat(const std::string& name, int sample) {
  base::UmaHistogramCustomCounts(name, sample, 0, 32, 6);
}

void LogAccountStatHiRes(const std::string& name, int sample) {
  base::UmaHistogramCustomCounts(name, sample, 0, 1000, 100);
}

void LogTimesUsedStat(const std::string& name, int sample) {
  base::UmaHistogramCustomCounts(name, sample, 0, 100, 10);
}

void LogNumberOfAccountsForScheme(const std::string& scheme, int sample) {
  base::UmaHistogramCustomCounts(
      "PasswordManager.TotalAccountsHiRes.WithScheme." + scheme, sample, 1,
      1000, 100);
}

void LogNumberOfAccountsReusingPassword(const std::string& suffix,
                                        int sample,
                                        HistogramSize histogram_size) {
  int max = histogram_size == HistogramSize::LARGE ? 500 : 100;
  int bucket_count = histogram_size == HistogramSize::LARGE ? 50 : 20;
  base::UmaHistogramCustomCounts(
      "PasswordManager.AccountsReusingPassword." + suffix, sample, 1, max,
      bucket_count);
}

// Records password reuse metrics given the |signon_realms| corresponding to a
// set of accounts that reuse the same password. See histograms.xml for details.
void LogPasswordReuseMetrics(const std::vector<std::string>& signon_realms) {
  struct StatisticsPerScheme {
    StatisticsPerScheme() : num_total_accounts(0) {}

    // The number of accounts for each registry controlled domain.
    std::map<std::string, int> num_accounts_per_registry_controlled_domain;

    // The number of accounts for each domain.
    std::map<std::string, int> num_accounts_per_domain;

    // Total number of accounts with this scheme. This equals the sum of counts
    // in either of the above maps.
    int num_total_accounts;
  };

  // The scheme (i.e. protocol) of the origin, not PasswordForm::scheme.
  enum Scheme { SCHEME_HTTP, SCHEME_HTTPS };
  const Scheme kAllSchemes[] = {SCHEME_HTTP, SCHEME_HTTPS};

  StatisticsPerScheme statistics[base::size(kAllSchemes)];
  std::map<std::string, std::string> domain_to_registry_controlled_domain;

  for (const std::string& signon_realm : signon_realms) {
    const GURL signon_realm_url(signon_realm);
    const std::string domain = signon_realm_url.host();
    if (domain.empty())
      continue;

    if (!domain_to_registry_controlled_domain.count(domain)) {
      domain_to_registry_controlled_domain[domain] =
          GetRegistryControlledDomain(signon_realm_url);
      if (domain_to_registry_controlled_domain[domain].empty())
        domain_to_registry_controlled_domain[domain] = domain;
    }
    const std::string& registry_controlled_domain =
        domain_to_registry_controlled_domain[domain];

    Scheme scheme = SCHEME_HTTP;
    static_assert(base::size(kAllSchemes) == 2, "Update this logic");
    if (signon_realm_url.SchemeIs(url::kHttpsScheme))
      scheme = SCHEME_HTTPS;
    else if (!signon_realm_url.SchemeIs(url::kHttpScheme))
      continue;

    statistics[scheme].num_accounts_per_domain[domain]++;
    statistics[scheme].num_accounts_per_registry_controlled_domain
        [registry_controlled_domain]++;
    statistics[scheme].num_total_accounts++;
  }

  // For each "source" account of either scheme, count the number of "target"
  // accounts reusing the same password (of either scheme).
  for (const Scheme scheme : kAllSchemes) {
    for (const auto& kv : statistics[scheme].num_accounts_per_domain) {
      const std::string& domain(kv.first);
      const int num_accounts_per_domain(kv.second);
      const std::string& registry_controlled_domain =
          domain_to_registry_controlled_domain[domain];

      Scheme other_scheme = scheme == SCHEME_HTTP ? SCHEME_HTTPS : SCHEME_HTTP;
      static_assert(base::size(kAllSchemes) == 2, "Update |other_scheme|");

      // Discount the account at hand from the number of accounts with the same
      // domain and scheme.
      int num_accounts_for_same_domain[base::size(kAllSchemes)] = {};
      num_accounts_for_same_domain[scheme] =
          statistics[scheme].num_accounts_per_domain[domain] - 1;
      num_accounts_for_same_domain[other_scheme] =
          statistics[other_scheme].num_accounts_per_domain[domain];

      // By definition, a PSL match requires the scheme to be the same.
      int num_psl_matching_accounts =
          statistics[scheme].num_accounts_per_registry_controlled_domain
              [registry_controlled_domain] -
          statistics[scheme].num_accounts_per_domain[domain];

      // Discount PSL matches from the number of accounts with different domains
      // but the same scheme.
      int num_accounts_for_different_domain[base::size(kAllSchemes)] = {};
      num_accounts_for_different_domain[scheme] =
          statistics[scheme].num_total_accounts -
          statistics[scheme].num_accounts_per_registry_controlled_domain
              [registry_controlled_domain];
      num_accounts_for_different_domain[other_scheme] =
          statistics[other_scheme].num_total_accounts -
          statistics[other_scheme].num_accounts_per_domain[domain];

      std::string source_realm_kind =
          scheme == SCHEME_HTTP ? "FromHttpRealm" : "FromHttpsRealm";
      static_assert(base::size(kAllSchemes) == 2, "Update |source_realm_kind|");

      // So far, the calculation has been carried out once per "source" domain,
      // but the metrics need to be recorded on a per-account basis. The set of
      // metrics are the same for all accounts for the same domain, so simply
      // report them as many times as accounts.
      for (int i = 0; i < num_accounts_per_domain; ++i) {
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpRealmWithSameHost",
            num_accounts_for_same_domain[SCHEME_HTTP], HistogramSize::SMALL);
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpsRealmWithSameHost",
            num_accounts_for_same_domain[SCHEME_HTTPS], HistogramSize::SMALL);
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnPSLMatchingRealm",
            num_psl_matching_accounts, HistogramSize::SMALL);

        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpRealmWithDifferentHost",
            num_accounts_for_different_domain[SCHEME_HTTP],
            HistogramSize::LARGE);
        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnHttpsRealmWithDifferentHost",
            num_accounts_for_different_domain[SCHEME_HTTPS],
            HistogramSize::LARGE);

        LogNumberOfAccountsReusingPassword(
            source_realm_kind + ".OnAnyRealmWithDifferentHost",
            num_accounts_for_different_domain[SCHEME_HTTP] +
                num_accounts_for_different_domain[SCHEME_HTTPS],
            HistogramSize::LARGE);
      }
    }
  }
}

bool ClearAllSyncMetadata(sql::Database* db) {
  sql::Statement s1(
      db->GetCachedStatement(SQL_FROM_HERE, "DELETE FROM sync_model_metadata"));

  sql::Statement s2(db->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM sync_entities_metadata"));

  return s1.Run() && s2.Run();
}

// Seals the version of the given builders. This is method should be always used
// to seal versions of all builder to make sure all builders are at the same
// version.
void SealVersion(SQLTableBuilders builders, unsigned expected_version) {
  unsigned logins_version = builders.logins->SealVersion();
  DCHECK_EQ(expected_version, logins_version);

  unsigned sync_entities_metadata_version =
      builders.sync_entities_metadata->SealVersion();
  DCHECK_EQ(expected_version, sync_entities_metadata_version);

  unsigned sync_model_metadata_version =
      builders.sync_model_metadata->SealVersion();
  DCHECK_EQ(expected_version, sync_model_metadata_version);
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
  builders.sync_entities_metadata->AddPrimaryKeyColumn("storage_key");
  builders.sync_entities_metadata->AddColumn("metadata", "VARCHAR NOT NULL");
  builders.sync_model_metadata->AddPrimaryKeyColumn("id");
  builders.sync_model_metadata->AddColumn("model_metadata", "VARCHAR NOT NULL");
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
  // column. MigrateLogins() will take care of migrating the data.
  builders.logins->AddColumn("date_last_used", "INTEGER NOT NULL DEFAULT 0");
  SealVersion(builders, /*expected_version=*/25u);

  // Version 26 is the first version where the id is AUTOINCREMENT.
  SealVersion(builders, /*expected_version=*/26u);

  DCHECK_EQ(static_cast<size_t>(COLUMN_NUM), builders.logins->NumberOfColumns())
      << "Adjust LoginDatabaseTableColumns if you change column definitions "
         "here.";
}

// Call this after having called InitializeBuilders(), to migrate the database
// from the current version to kCurrentVersionNumber.
bool MigrateLogins(unsigned current_version,
                   SQLTableBuilders builders,
                   sql::Database* db) {
  if (!builders.logins->MigrateFrom(current_version, db))
    return false;

  if (!builders.sync_entities_metadata->MigrateFrom(current_version, db))
    return false;

  if (!builders.sync_model_metadata->MigrateFrom(current_version, db))
    return false;

  // Data changes, not covered by the schema migration above.
  if (current_version <= 8) {
    sql::Statement fix_time_format;
    fix_time_format.Assign(db->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE logins SET date_created = (date_created * ?) + ?"));
    fix_time_format.BindInt64(0, base::Time::kMicrosecondsPerSecond);
    fix_time_format.BindInt64(1, base::Time::kTimeTToMicrosecondsOffset);
    if (!fix_time_format.Run())
      return false;
  }

  if (current_version <= 16) {
    sql::Statement reset_zero_click;
    reset_zero_click.Assign(db->GetCachedStatement(
        SQL_FROM_HERE, "UPDATE logins SET skip_zero_click = 1"));
    if (!reset_zero_click.Run())
      return false;
  }

  // Sync Metadata tables have been introduced in version 21. It is enough to
  // drop all data because Sync would populate the tables properly at startup.
  if (current_version >= 21 && current_version < 26) {
    if (!ClearAllSyncMetadata(db))
      return false;
  }

  // "date_last_used" column has been introduced and we should migrate the data
  // in "preferred" column. We set the value of "date_last_used"
  // deterministically such the final output will be consistent across syncing
  // clients.
  // TODO(crbug.com/997670): Drop the "preferred" column together with this
  // migration code in M82.
  if (current_version < 25) {
    sql::Statement preferred_stmt;
    preferred_stmt.Assign(db->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE logins SET date_last_used = ? WHERE preferred > 0"));
    // Set the preferred password to be last used one day after the Windows
    // Epoch to make sure the "preferred" password is used more recently.
    // Non-preferred passwords carry the default value of 0 (which maps to the
    // Windows Epoch).
    preferred_stmt.BindInt64(0, base::TimeDelta::FromDays(1).InMicroseconds());
    if (!preferred_stmt.Run())
      return false;
  }

  // In version 26, the primary key of the logins table became an AUTOINCREMENT
  // field. Since SQLite doesn't allow changing the column type, the only way is
  // to actually create a temp table with the primary key propely set as an
  // AUTOINCREMENT field, and move the data there. The code has been adjusted
  // such that newly created tables have the primary key properly set as
  // AUTOINCREMENT.
  if (current_version < 26) {
    // This statement creates the logins database similar to version 26 with the
    // primary key column set to AUTOINCREMENT.
    std::string temp_table_create_statement_version_26 =
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
    std::string move_data_statement =
        "INSERT INTO logins_temp SELECT * from logins";
    std::string drop_table_statement = "DROP TABLE logins";
    std::string rename_table_statement =
        "ALTER TABLE logins_temp RENAME TO logins";

    sql::Transaction transaction(db);
    if (!(transaction.Begin() &&
          db->Execute(temp_table_create_statement_version_26.c_str()) &&
          db->Execute(move_data_statement.c_str()) &&
          db->Execute(drop_table_statement.c_str()) &&
          db->Execute(rename_table_statement.c_str()) &&
          transaction.Commit())) {
      return false;
    }
  }

  return true;
}

// Because of https://crbug.com/295851, some early version numbers might be
// wrong. This function detects that and fixes the version.
bool FixVersionIfNeeded(sql::Database* db, int* current_version) {
  if (*current_version == 1) {
    int extra_columns = 0;
    if (db->DoesColumnExist("logins", "password_type"))
      ++extra_columns;
    if (db->DoesColumnExist("logins", "possible_usernames"))
      ++extra_columns;
    if (extra_columns == 2) {
      *current_version = 2;
    } else if (extra_columns == 1) {
      // If this is https://crbug.com/295851 then either both columns exist
      // or none.
      return false;
    }
  }
  if (*current_version == 2) {
    if (db->DoesColumnExist("logins", "times_used"))
      *current_version = 3;
  }
  if (*current_version == 3) {
    if (db->DoesColumnExist("logins", "form_data"))
      *current_version = 4;
  }
  // "date_last_used" columns has been introduced in version 25. if it exists,
  // the version should be at least 25. This has been added to address this bug
  // (crbug.com/1020320).
  if (*current_version < 25) {
    if (db->DoesColumnExist("logins", "date_last_used"))
      *current_version = 25;
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
PasswordForm GetFormForRemoval(const sql::Statement& statement) {
  PasswordForm form;
  form.origin = GURL(statement.ColumnString(COLUMN_ORIGIN_URL));
  form.username_element = statement.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form.username_value = statement.ColumnString16(COLUMN_USERNAME_VALUE);
  form.password_element = statement.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form.signon_realm = statement.ColumnString(COLUMN_SIGNON_REALM);
  return form;
}

}  // namespace

LoginDatabase::LoginDatabase(const base::FilePath& db_path,
                             IsAccountStore is_account_store)
    : db_path_(db_path), is_account_store_(is_account_store) {}

LoginDatabase::~LoginDatabase() {
}

bool LoginDatabase::Init() {
  // Set pragmas for a small, private database (based on WebDatabase).
  db_.set_page_size(2048);
  db_.set_cache_size(32);
  db_.set_exclusive_locking();
  db_.set_histogram_tag("Passwords");

  if (!db_.Open(db_path_)) {
    LogDatabaseInitError(OPEN_FILE_ERROR);
    LOG(ERROR) << "Unable to open the password store database.";
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LogDatabaseInitError(START_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to start a transaction.";
    db_.Close();
    return false;
  }

  // Check the database version.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    LogDatabaseInitError(META_TABLE_INIT_ERROR);
    LOG(ERROR) << "Unable to create the meta table.";
    transaction.Rollback();
    db_.Close();
    return false;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LogDatabaseInitError(INCOMPATIBLE_VERSION);
    LOG(ERROR) << "Password store database is too new, kCurrentVersionNumber="
               << kCurrentVersionNumber << ", GetCompatibleVersionNumber="
               << meta_table_.GetCompatibleVersionNumber();
    transaction.Rollback();
    db_.Close();
    return false;
  }

  SQLTableBuilder logins_builder("logins");
  SQLTableBuilder sync_entities_metadata_builder("sync_entities_metadata");
  SQLTableBuilder sync_model_metadata_builder("sync_model_metadata");
  SQLTableBuilders builders = {&logins_builder, &sync_entities_metadata_builder,
                               &sync_model_metadata_builder};
  InitializeBuilders(builders);
  InitializeStatementStrings(logins_builder);

  if (!db_.DoesTableExist("logins")) {
    if (!logins_builder.CreateTable(&db_)) {
      VLOG(0) << "Failed to create the 'logins' table";
      transaction.Rollback();
      db_.Close();
      return false;
    }
  }

  if (!db_.DoesTableExist("sync_entities_metadata")) {
    if (!sync_entities_metadata_builder.CreateTable(&db_)) {
      VLOG(0) << "Failed to create the 'sync_entities_metadata' table";
      transaction.Rollback();
      db_.Close();
      return false;
    }
  }

  if (!db_.DoesTableExist("sync_model_metadata")) {
    if (!sync_model_metadata_builder.CreateTable(&db_)) {
      VLOG(0) << "Failed to create the 'sync_model_metadata' table";
      transaction.Rollback();
      db_.Close();
      return false;
    }
  }

  stats_table_.Init(&db_);
  compromised_credentials_table_.Init(&db_);
  field_info_table_.Init(&db_);

  int current_version = meta_table_.GetVersionNumber();
  bool migration_success = FixVersionIfNeeded(&db_, &current_version);

  // If the file on disk is an older database version, bring it up to date.
  if (migration_success && current_version < kCurrentVersionNumber) {
    migration_success = MigrateLogins(
        base::checked_cast<unsigned>(current_version), builders, &db_);
  }
  if (migration_success && current_version <= 15) {
    migration_success = stats_table_.MigrateToVersion(16);
  }
  if (migration_success) {
    // |migration_success| could be true when no logins have been actually
    // migrated. We should protect against downgrading the database version in
    // such case. Update the database version only if a migration took place.
    if (current_version < kCurrentVersionNumber) {
      meta_table_.SetCompatibleVersionNumber(kCompatibleVersionNumber);
      meta_table_.SetVersionNumber(kCurrentVersionNumber);
    }
  } else {
    LogDatabaseInitError(MIGRATION_ERROR);
    LOG(ERROR) << "Unable to migrate database from "
               << meta_table_.GetVersionNumber() << " to "
               << kCurrentVersionNumber;
    transaction.Rollback();
    db_.Close();
    return false;
  }

  if (!stats_table_.CreateTableIfNecessary()) {
    LogDatabaseInitError(INIT_STATS_ERROR);
    LOG(ERROR) << "Unable to create the stats table.";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  // The table "leaked_credentials" was previously created without a flag.
  // The table is now renamed to "compromised_credentials" and also includes
  // a new column so the old table needs to be deleted.
  if (db_.DoesTableExist("leaked_credentials")) {
    if (!db_.Execute("DROP TABLE leaked_credentials")) {
      LOG(ERROR) << "Unable to create the stats table.";
      transaction.Rollback();
      db_.Close();
      return false;
    }
  }

  // TODO(bdea): Create a more generic experiment for creation of compromise
  // credentials because the current experiment is leak detection specific.
  if (base::FeatureList::IsEnabled(password_manager::features::kLeakHistory)) {
    if (!compromised_credentials_table_.CreateTableIfNecessary()) {
      LogDatabaseInitError(INIT_COMPROMISED_CREDENTIALS_ERROR);
      LOG(ERROR) << "Unable to create the compromised credentials table.";
      transaction.Rollback();
      db_.Close();
      return false;
    }
  }

  if (!field_info_table_.CreateTableIfNecessary()) {
    LogDatabaseInitError(INIT_FIELD_INFO_ERROR);
    LOG(ERROR) << "Unable to create the field info table.";
    transaction.Rollback();
    db_.Close();
    return false;
  }

  if (!transaction.Commit()) {
    LogDatabaseInitError(COMMIT_TRANSACTION_ERROR);
    LOG(ERROR) << "Unable to commit a transaction.";
    db_.Close();
    return false;
  }

  LogDatabaseInitError(INIT_OK);
  return true;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void LoginDatabase::InitPasswordRecoveryUtil(
    std::unique_ptr<PasswordRecoveryUtilMac> password_recovery_util) {
  password_recovery_util_ = std::move(password_recovery_util);
}
#endif

void LoginDatabase::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT signon_realm, password_type, blacklisted_by_user,"
      "COUNT(username_value) FROM logins GROUP BY "
      "signon_realm, password_type, blacklisted_by_user"));

  if (!s.is_valid())
    return;

  // Need to stay in sync with the PasswordGenerated suffix in histograms.xml.
  static constexpr base::StringPiece kAutoGeneratedSuffix = ".AutoGenerated";
  static constexpr base::StringPiece kUserCreatedSuffix = ".UserCreated";
  static constexpr base::StringPiece kOverallSuffix = ".Overall";

  // Need to stay in sync with the PasswordCustomPassphrase suffix in
  // histograms.xml.
  base::StringPiece custom_passphrase_suffix = custom_passphrase_sync_enabled
                                                   ? ".WithCustomPassphrase"
                                                   : ".WithoutCustomPassphrase";

  int total_user_created_accounts = 0;
  int total_generated_accounts = 0;
  int blacklisted_sites = 0;
  while (s.Step()) {
    auto password_type = static_cast<PasswordForm::Type>(s.ColumnInt(1));
    int blacklisted = s.ColumnInt(2);
    int accounts_per_site = s.ColumnInt(3);
    if (blacklisted) {
      ++blacklisted_sites;
      continue;
    }

    static constexpr base::StringPiece kAccountsPerSite =
        "PasswordManager.AccountsPerSiteHiRes";

    if (password_type == PasswordForm::Type::kGenerated) {
      total_generated_accounts += accounts_per_site;
      LogAccountStatHiRes(base::StrCat({kAccountsPerSite, kAutoGeneratedSuffix,
                                        custom_passphrase_suffix}),
                          accounts_per_site);
    } else {
      total_user_created_accounts += accounts_per_site;
      LogAccountStatHiRes(base::StrCat({kAccountsPerSite, kUserCreatedSuffix,
                                        custom_passphrase_suffix}),
                          accounts_per_site);
    }

    LogAccountStatHiRes(base::StrCat({kAccountsPerSite, kOverallSuffix,
                                      custom_passphrase_suffix}),
                        accounts_per_site);
  }

  static constexpr base::StringPiece kTotalAccountsByType =
      "PasswordManager.TotalAccountsHiRes.ByType";

  LogAccountStatHiRes(base::StrCat({kTotalAccountsByType, kUserCreatedSuffix,
                                    custom_passphrase_suffix}),
                      total_user_created_accounts);

  LogAccountStatHiRes(base::StrCat({kTotalAccountsByType, kAutoGeneratedSuffix,
                                    custom_passphrase_suffix}),
                      total_generated_accounts);

  LogAccountStatHiRes(base::StrCat({kTotalAccountsByType, kOverallSuffix,
                                    custom_passphrase_suffix}),
                      total_user_created_accounts + total_generated_accounts);

  LogAccountStatHiRes(base::StrCat({"PasswordManager.BlacklistedSitesHiRes",
                                    custom_passphrase_suffix}),
                      blacklisted_sites);

  sql::Statement usage_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT password_type, times_used FROM logins"));

  if (!usage_statement.is_valid())
    return;

  while (usage_statement.Step()) {
    auto type = static_cast<PasswordForm::Type>(usage_statement.ColumnInt(0));
    const int times_used = usage_statement.ColumnInt(1);

    static constexpr base::StringPiece kTimesPasswordUsed =
        "PasswordManager.TimesPasswordUsed";

    if (type == PasswordForm::Type::kGenerated) {
      LogTimesUsedStat(base::StrCat({kTimesPasswordUsed, kAutoGeneratedSuffix,
                                     custom_passphrase_suffix}),
                       times_used);
    } else {
      LogTimesUsedStat(base::StrCat({kTimesPasswordUsed, kUserCreatedSuffix,
                                     custom_passphrase_suffix}),
                       times_used);
    }
    LogTimesUsedStat(base::StrCat({kTimesPasswordUsed, kOverallSuffix,
                                   custom_passphrase_suffix}),
                     times_used);
  }

  bool syncing_account_saved = false;
  if (!sync_username.empty()) {
    sql::Statement sync_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT username_value FROM logins "
        "WHERE signon_realm == ?"));
    sync_statement.BindString(
        0, GaiaUrls::GetInstance()->gaia_url().GetOrigin().spec());

    if (!sync_statement.is_valid())
      return;

    while (sync_statement.Step()) {
      std::string username = sync_statement.ColumnString(0);
      if (gaia::AreEmailsSame(sync_username, username)) {
        syncing_account_saved = true;
        break;
      }
    }
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.SyncingAccountState",
                            2 * sync_username.empty() + syncing_account_saved,
                            4);

  sql::Statement empty_usernames_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT COUNT(*) FROM logins "
                     "WHERE blacklisted_by_user=0 AND username_value=''"));
  if (empty_usernames_statement.Step()) {
    int empty_forms = empty_usernames_statement.ColumnInt(0);
    UMA_HISTOGRAM_COUNTS_100("PasswordManager.EmptyUsernames.CountInDatabase",
                             empty_forms);
  }

  sql::Statement logins_with_schemes_statement(db_.GetUniqueStatement(
      "SELECT signon_realm, origin_url, blacklisted_by_user FROM logins;"));

  if (!logins_with_schemes_statement.is_valid())
    return;

  int android_logins = 0;
  int ftp_logins = 0;
  int http_logins = 0;
  int https_logins = 0;
  int other_logins = 0;

  while (logins_with_schemes_statement.Step()) {
    std::string signon_realm = logins_with_schemes_statement.ColumnString(0);
    GURL origin_url = GURL(logins_with_schemes_statement.ColumnString(1));
    bool blacklisted_by_user = !!logins_with_schemes_statement.ColumnInt(2);
    if (blacklisted_by_user)
      continue;

    if (IsValidAndroidFacetURI(signon_realm)) {
      ++android_logins;
    } else if (origin_url.SchemeIs(url::kHttpsScheme)) {
      ++https_logins;
    } else if (origin_url.SchemeIs(url::kHttpScheme)) {
      ++http_logins;
    } else if (origin_url.SchemeIs(url::kFtpScheme)) {
      ++ftp_logins;
    } else {
      ++other_logins;
    }
  }

  LogNumberOfAccountsForScheme("Android", android_logins);
  LogNumberOfAccountsForScheme("Ftp", ftp_logins);
  LogNumberOfAccountsForScheme("Http", http_logins);
  LogNumberOfAccountsForScheme("Https", https_logins);
  LogNumberOfAccountsForScheme("Other", other_logins);

#if !defined(OS_IOS) && !defined(OS_ANDROID)
  // Number of times the user needs to dismiss a save/update bubble for it to
  // not be shown again. This happens only on desktop platforms.
  int threshold =
      password_bubble_experiment::GetSmartBubbleDismissalThreshold();
  LogAccountStatHiRes(
      "PasswordManager.BubbleSuppression.DomainsWithSuppressedBubble",
      stats_table_.GetNumDomainsWithAtLeastNDismissals(threshold));
  LogAccountStatHiRes(
      "PasswordManager.BubbleSuppression.AccountsWithSuppressedBubble",
      stats_table_.GetNumAccountsWithAtLeastNDismissals(threshold));
  LogAccountStatHiRes(
      "PasswordManager.BubbleSuppression.AccountsInStatisticsTable",
      stats_table_.GetNumAccounts());
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)

  sql::Statement saved_passwords_statement(
      db_.GetUniqueStatement("SELECT signon_realm, password_value, scheme "
                             "FROM logins WHERE blacklisted_by_user = 0"));

  std::map<base::string16, std::vector<std::string>> passwords_to_realms;
  size_t failed_encryption = 0;
  while (saved_passwords_statement.Step()) {
    base::string16 decrypted_password;
    // Note that CryptProtectData() is non-deterministic, so passwords must be
    // decrypted before checking equality.
    if (DecryptedString(saved_passwords_statement.ColumnString(1),
                        &decrypted_password) == ENCRYPTION_RESULT_SUCCESS) {
      std::string signon_realm = saved_passwords_statement.ColumnString(0);
      if (saved_passwords_statement.ColumnInt(2) == 0 &&
          !decrypted_password.empty() &&
          !IsValidAndroidFacetURI(signon_realm)) {
        passwords_to_realms[decrypted_password].push_back(
            std::move(signon_realm));
      }
    } else {
      ++failed_encryption;
    }
  }
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.InaccessiblePasswords",
                           failed_encryption);

  for (const auto& password_to_realms : passwords_to_realms)
    LogPasswordReuseMetrics(password_to_realms.second);

  {
    sql::Statement duplicates_statement(db_.GetUniqueStatement(
        "SELECT signon_realm, username_value, password_value FROM logins "
        "WHERE blacklisted_by_user = 0"));
    // First group the passwords by [signon_realm, username] (which should be a
    // unique identifier).
    std::map<std::pair<std::string, std::string>, std::vector<std::string>>
        passwords_by_realm_and_user;
    while (duplicates_statement.Step()) {
      std::string signon_realm = duplicates_statement.ColumnString(0);
      std::string username = duplicates_statement.ColumnString(1);
      std::string encrypted_password = duplicates_statement.ColumnString(2);
#if defined(OS_WIN)
      // Note: CryptProtectData() (used on Windows for encrypting passwords) is
      // non-deterministic, so passwords must be decrypted before checking
      // equality.
      base::string16 password16;
      if (DecryptedString(encrypted_password, &password16) !=
          ENCRYPTION_RESULT_SUCCESS) {
        continue;
      }
      std::string password = base::UTF16ToUTF8(password16);
#else
      // On non-Windows platforms, passwords are encrypted in a deterministic
      // way. Since we're only interested in equality, don't bother decrypting
      // them, just use the encrypted data directly.
      std::string password = encrypted_password;
#endif
      passwords_by_realm_and_user[std::make_pair(signon_realm, username)]
          .push_back(password);
    }
    // Now go over the passwords by [realm, username] - typically there should
    // be only one password each.
    size_t credentials_with_duplicates = 0;
    size_t credentials_with_mismatched_duplicates = 0;
    for (auto& entry : passwords_by_realm_and_user) {
      std::vector<std::string>& passwords = entry.second;
      // Only one password -> no duplicates, move on.
      if (passwords.size() == 1)
        continue;
      std::sort(passwords.begin(), passwords.end());
      auto last = std::unique(passwords.begin(), passwords.end());
      // If |last| moved from |.end()|, that means there were duplicate
      // passwords.
      if (last != passwords.end())
        credentials_with_duplicates++;
      // If there is more than 1 password left after de-duping, then there were
      // mismatched duplicates.
      if (std::distance(passwords.begin(), last) > 1)
        credentials_with_mismatched_duplicates++;
    }
    LogAccountStat("PasswordManager.CredentialsWithDuplicates",
                   credentials_with_duplicates);
    LogAccountStat("PasswordManager.CredentialsWithMismatchedDuplicates",
                   credentials_with_mismatched_duplicates);
  }
}

PasswordStoreChangeList LoginDatabase::AddLogin(const PasswordForm& form,
                                                AddLoginError* error) {
  if (error) {
    *error = AddLoginError::kNone;
  }
  PasswordStoreChangeList list;
  if (!DoesMatchConstraints(form)) {
    if (error) {
      *error = AddLoginError::kConstraintViolation;
    }
    return list;
  }
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
      ENCRYPTION_RESULT_SUCCESS) {
    if (error) {
      *error = AddLoginError::kEncrytionServiceFailure;
    }
    return list;
  }
  DCHECK(!add_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, add_statement_.c_str()));
  BindAddStatement(form, encrypted_password, &s);
  int sqlite_error_code;
  db_.set_error_callback(base::BindRepeating(&AddCallback, &sqlite_error_code));
  const bool success = s.Run();
  if (success) {
    list.emplace_back(PasswordStoreChange::ADD, form, db_.GetLastInsertRowId());
    return list;
  }
  // Repeat the same statement but with REPLACE semantic.
  sqlite_error_code = 0;
  DCHECK(!add_replace_statement_.empty());
  int old_primary_key = GetPrimaryKey(form);
  s.Assign(
      db_.GetCachedStatement(SQL_FROM_HERE, add_replace_statement_.c_str()));
  BindAddStatement(form, encrypted_password, &s);
  if (s.Run()) {
    list.emplace_back(PasswordStoreChange::REMOVE, form, old_primary_key);
    list.emplace_back(PasswordStoreChange::ADD, form, db_.GetLastInsertRowId());
  } else if (error) {
    if (sqlite_error_code == 19 /*SQLITE_CONSTRAINT*/) {
      *error = AddLoginError::kConstraintViolation;
    } else {
      *error = AddLoginError::kDbError;
    }
  }
  db_.reset_error_callback();
  return list;
}

PasswordStoreChangeList LoginDatabase::AddBlacklistedLoginForTesting(
    const PasswordForm& form) {
  DCHECK(form.blacklisted_by_user);
  PasswordStoreChangeList list;

  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
      ENCRYPTION_RESULT_SUCCESS)
    return list;

  DCHECK(!add_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, add_statement_.c_str()));
  BindAddStatement(form, encrypted_password, &s);
  if (s.Run())
    list.emplace_back(PasswordStoreChange::ADD, form, db_.GetLastInsertRowId());
  return list;
}

PasswordStoreChangeList LoginDatabase::UpdateLogin(const PasswordForm& form,
                                                   UpdateLoginError* error) {
  if (error) {
    *error = UpdateLoginError::kNone;
  }
  std::string encrypted_password;
  if (EncryptedString(form.password_value, &encrypted_password) !=
      ENCRYPTION_RESULT_SUCCESS) {
    if (error) {
      *error = UpdateLoginError::kEncrytionServiceFailure;
    }
    return PasswordStoreChangeList();
  }

#if defined(OS_IOS)
  DeleteEncryptedPassword(form);
#endif
  // Replacement is necessary to deal with updating imported credentials. See
  // crbug.com/349138 for details.
  DCHECK(!update_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, update_statement_.c_str()));
  int next_param = 0;
  s.BindString(next_param++, form.action.spec());
  s.BindBlob(next_param++, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindString16(next_param++, form.submit_element);
  s.BindInt(next_param++, form.preferred);
  s.BindInt64(next_param++, form.date_created.ToInternalValue());
  s.BindInt(next_param++, form.blacklisted_by_user);
  s.BindInt(next_param++, static_cast<int>(form.scheme));
  s.BindInt(next_param++, static_cast<int>(form.type));
  s.BindInt(next_param++, form.times_used);
  base::Pickle form_data_pickle;
  autofill::SerializeFormData(form.form_data, &form_data_pickle);
  s.BindBlob(next_param++, form_data_pickle.data(), form_data_pickle.size());
  s.BindInt64(next_param++, form.date_synced.ToInternalValue());
  s.BindString16(next_param++, form.display_name);
  s.BindString(next_param++, form.icon_url.spec());
  // An empty Origin serializes as "null" which would be strange to store here.
  s.BindString(next_param++, form.federation_origin.opaque()
                                 ? std::string()
                                 : form.federation_origin.Serialize());
  s.BindInt(next_param++, form.skip_zero_click);
  s.BindInt(next_param++, static_cast<int>(form.generation_upload_status));
  base::Pickle username_pickle =
      SerializeValueElementPairs(form.all_possible_usernames);
  s.BindBlob(next_param++, username_pickle.data(), username_pickle.size());
  s.BindInt64(next_param++,
              form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  // NOTE: Add new fields here unless the field is a part of the unique key.
  // If so, add new field below.

  // WHERE starts here.
  s.BindString(next_param++, form.origin.spec());
  s.BindString16(next_param++, form.username_element);
  s.BindString16(next_param++, form.username_value);
  s.BindString16(next_param++, form.password_element);
  s.BindString(next_param++, form.signon_realm);
  // NOTE: Add new fields here only if the field is a part of the unique key.
  // Otherwise, add the field above "WHERE starts here" comment.

  if (!s.Run()) {
    if (error) {
      *error = UpdateLoginError::kDbError;
    }
    return PasswordStoreChangeList();
  }

  PasswordStoreChangeList list;
  if (db_.GetLastChangeCount()) {
    list.emplace_back(PasswordStoreChange::UPDATE, form, GetPrimaryKey(form));
  } else if (error) {
    *error = UpdateLoginError::kNoUpdatedRecords;
  }

  return list;
}

bool LoginDatabase::RemoveLogin(const PasswordForm& form,
                                PasswordStoreChangeList* changes) {
  if (changes) {
    changes->clear();
  }
  if (form.is_public_suffix_match) {
    // TODO(dvadym): Discuss whether we should allow to remove PSL matched
    // credentials.
    return false;
  }
#if defined(OS_IOS)
  DeleteEncryptedPassword(form);
#endif
  // Remove a login by UNIQUE-constrained fields.
  DCHECK(!delete_statement_.empty());
  int primary_key = GetPrimaryKey(form);
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, delete_statement_.c_str()));
  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);

  if (!s.Run() || db_.GetLastChangeCount() == 0) {
    return false;
  }
  if (changes) {
    changes->emplace_back(PasswordStoreChange::REMOVE, form, primary_key);
  }
  return true;
}

bool LoginDatabase::RemoveLoginByPrimaryKey(int primary_key,
                                            PasswordStoreChangeList* changes) {
  PasswordForm form;
  if (changes) {
    changes->clear();
    sql::Statement s1(db_.GetCachedStatement(
        SQL_FROM_HERE, "SELECT * FROM logins WHERE id = ?"));
    s1.BindInt(0, primary_key);
    if (!s1.Step()) {
      return false;
    }
    int db_primary_key = -1;
    EncryptionResult result = InitPasswordFormFromStatement(
        s1, /*decrypt_and_fill_password_value=*/false, &db_primary_key, &form);
    DCHECK_EQ(result, ENCRYPTION_RESULT_SUCCESS);
    DCHECK_EQ(db_primary_key, primary_key);
  }

#if defined(OS_IOS)
  DeleteEncryptedPasswordById(primary_key);
#endif
  DCHECK(!delete_by_id_statement_.empty());
  sql::Statement s2(
      db_.GetCachedStatement(SQL_FROM_HERE, delete_by_id_statement_.c_str()));
  s2.BindInt(0, primary_key);
  if (!s2.Run() || db_.GetLastChangeCount() == 0) {
    return false;
  }
  if (changes) {
    changes->emplace_back(PasswordStoreChange::REMOVE, std::move(form),
                          primary_key);
  }
  return true;
}

bool LoginDatabase::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeList* changes) {
  if (changes) {
    changes->clear();
  }
  PrimaryKeyToFormMap key_to_form_map;
  ScopedTransaction transaction(this);
  if (!GetLoginsCreatedBetween(delete_begin, delete_end, &key_to_form_map)) {
    return false;
  }

#if defined(OS_IOS)
  for (const auto& pair : key_to_form_map) {
    DeleteEncryptedPassword(*pair.second);
  }
#endif

  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  s.BindInt64(0, delete_begin.ToInternalValue());
  s.BindInt64(1, delete_end.is_null() ? std::numeric_limits<int64_t>::max()
                                      : delete_end.ToInternalValue());

  if (!s.Run()) {
    return false;
  }
  if (changes) {
    for (const auto& pair : key_to_form_map) {
      changes->emplace_back(PasswordStoreChange::REMOVE,
                            /*form=*/std::move(*pair.second),
                            /*primary_key=*/pair.first);
    }
  }
  return true;
}

bool LoginDatabase::GetAutoSignInLogins(PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(key_to_form_map);
  DCHECK(!autosignin_statement_.empty());
  key_to_form_map->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, autosignin_statement_.c_str()));
  FormRetrievalResult result = StatementToForms(&s, nullptr, key_to_form_map);
  return result == FormRetrievalResult::kSuccess;
}

bool LoginDatabase::DisableAutoSignInForOrigin(const GURL& origin) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE logins SET skip_zero_click = 1 WHERE origin_url = ?;"));
  s.BindString(0, origin.spec());

  return s.Run();
}

LoginDatabase::EncryptionResult LoginDatabase::InitPasswordFormFromStatement(
    const sql::Statement& s,
    bool decrypt_and_fill_password_value,
    int* primary_key,
    PasswordForm* form) const {
  std::string encrypted_password;
  s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
  base::string16 decrypted_password;
  if (decrypt_and_fill_password_value) {
    EncryptionResult encryption_result =
        DecryptedString(encrypted_password, &decrypted_password);
    if (encryption_result != ENCRYPTION_RESULT_SUCCESS) {
      VLOG(0) << "Password decryption failed, encryption_result is "
              << encryption_result;
      return encryption_result;
    }
  }

  *primary_key = s.ColumnInt(COLUMN_ID);
  std::string tmp = s.ColumnString(COLUMN_ORIGIN_URL);
  form->origin = GURL(tmp);
  tmp = s.ColumnString(COLUMN_ACTION_URL);
  form->action = GURL(tmp);
  form->username_element = s.ColumnString16(COLUMN_USERNAME_ELEMENT);
  form->username_value = s.ColumnString16(COLUMN_USERNAME_VALUE);
  form->password_element = s.ColumnString16(COLUMN_PASSWORD_ELEMENT);
  form->password_value = decrypted_password;
  form->submit_element = s.ColumnString16(COLUMN_SUBMIT_ELEMENT);
  tmp = s.ColumnString(COLUMN_SIGNON_REALM);
  form->signon_realm = tmp;
  form->preferred = (s.ColumnInt(COLUMN_PREFERRED) > 0);
  form->date_created =
      base::Time::FromInternalValue(s.ColumnInt64(COLUMN_DATE_CREATED));
  form->blacklisted_by_user = (s.ColumnInt(COLUMN_BLACKLISTED_BY_USER) > 0);
  int scheme_int = s.ColumnInt(COLUMN_SCHEME);
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
  DCHECK(autofill::mojom::IsKnownEnumValue(form->scheme));
  int type_int = s.ColumnInt(COLUMN_PASSWORD_TYPE);
  form->type = static_cast<PasswordForm::Type>(type_int);
  DCHECK(autofill::mojom::IsKnownEnumValue(form->type));
  if (s.ColumnByteLength(COLUMN_POSSIBLE_USERNAME_PAIRS)) {
    base::Pickle pickle(
        static_cast<const char*>(s.ColumnBlob(COLUMN_POSSIBLE_USERNAME_PAIRS)),
        s.ColumnByteLength(COLUMN_POSSIBLE_USERNAME_PAIRS));
    form->all_possible_usernames = DeserializeValueElementPairs(pickle);
  }
  form->times_used = s.ColumnInt(COLUMN_TIMES_USED);
  if (s.ColumnByteLength(COLUMN_FORM_DATA)) {
    base::Pickle form_data_pickle(
        static_cast<const char*>(s.ColumnBlob(COLUMN_FORM_DATA)),
        s.ColumnByteLength(COLUMN_FORM_DATA));
    base::PickleIterator form_data_iter(form_data_pickle);
    bool success =
        autofill::DeserializeFormData(&form_data_iter, &form->form_data);
    metrics_util::FormDeserializationStatus status =
        success ? metrics_util::LOGIN_DATABASE_SUCCESS
                : metrics_util::LOGIN_DATABASE_FAILURE;
    metrics_util::LogFormDataDeserializationStatus(status);
  }
  form->date_synced =
      base::Time::FromInternalValue(s.ColumnInt64(COLUMN_DATE_SYNCED));
  form->display_name = s.ColumnString16(COLUMN_DISPLAY_NAME);
  form->icon_url = GURL(s.ColumnString(COLUMN_ICON_URL));
  form->federation_origin =
      url::Origin::Create(GURL(s.ColumnString(COLUMN_FEDERATION_URL)));
  form->skip_zero_click = (s.ColumnInt(COLUMN_SKIP_ZERO_CLICK) > 0);
  int generation_upload_status_int =
      s.ColumnInt(COLUMN_GENERATION_UPLOAD_STATUS);
  form->generation_upload_status =
      static_cast<PasswordForm::GenerationUploadStatus>(
          generation_upload_status_int);
  form->date_last_used = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(s.ColumnInt64(COLUMN_DATE_LAST_USED)));
  DCHECK(autofill::mojom::IsKnownEnumValue(form->generation_upload_status));
  return ENCRYPTION_RESULT_SUCCESS;
}

bool LoginDatabase::GetLogins(
    const PasswordStore::FormDigest& form,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(forms);
  forms->clear();

  const GURL signon_realm(form.signon_realm);
  std::string registered_domain = GetRegistryControlledDomain(signon_realm);
  const bool should_PSL_matching_apply =
      !registered_domain.empty() && form.scheme == PasswordForm::Scheme::kHtml;
  const bool should_federated_apply =
      form.scheme == PasswordForm::Scheme::kHtml;
  DCHECK(!get_statement_.empty());
  DCHECK(!get_statement_psl_.empty());
  DCHECK(!get_statement_federated_.empty());
  DCHECK(!get_statement_psl_federated_.empty());
  const std::string* sql_query = &get_statement_;
  if (should_PSL_matching_apply && should_federated_apply)
    sql_query = &get_statement_psl_federated_;
  else if (should_PSL_matching_apply)
    sql_query = &get_statement_psl_;
  else if (should_federated_apply)
    sql_query = &get_statement_federated_;

  // TODO(nyquist) Consider usage of GetCachedStatement when
  // http://crbug.com/248608 is fixed.
  sql::Statement s(db_.GetUniqueStatement(sql_query->c_str()));
  s.BindString(0, form.signon_realm);
  int placeholder = 1;

  // PSL matching only applies to HTML forms.
  if (should_PSL_matching_apply) {
    // We are extending the original SQL query with one that includes more
    // possible matches based on public suffix domain matching. Using a regexp
    // here is just an optimization to not have to parse all the stored entries
    // in the |logins| table. The result (scheme, domain and port) is verified
    // further down using GURL. See the functions SchemeMatches,
    // RegistryControlledDomainMatches and PortMatches.
    // We need to escape . in the domain. Since the domain has already been
    // sanitized using GURL, we do not need to escape any other characters.
    base::ReplaceChars(registered_domain, ".", "\\.", &registered_domain);
    std::string scheme = signon_realm.scheme();
    // We need to escape . in the scheme. Since the scheme has already been
    // sanitized using GURL, we do not need to escape any other characters.
    // The scheme soap.beep is an example with '.'.
    base::ReplaceChars(scheme, ".", "\\.", &scheme);
    const std::string port = signon_realm.port();
    // For a signon realm such as http://foo.bar/, this regexp will match
    // domains on the form http://foo.bar/, http://www.foo.bar/,
    // http://www.mobile.foo.bar/. It will not match http://notfoo.bar/.
    // The scheme and port has to be the same as the observed form.
    std::string regexp = "^(" + scheme + ":\\/\\/)([\\w-]+\\.)*" +
                         registered_domain + "(:" + port + ")?\\/$";
    s.BindString(placeholder++, regexp);

    if (should_federated_apply) {
      // This regex matches any subdomain of |registered_domain|, in particular
      // it matches the empty subdomain. Hence exact domain matches are also
      // retrieved.
      s.BindString(placeholder++,
                   "^federation://([\\w-]+\\.)*" + registered_domain + "/.+$");
    }
  } else if (should_federated_apply) {
    std::string expression =
        base::StringPrintf("federation://%s/%%", form.origin.host().c_str());
    s.BindString(placeholder++, expression);
  }

  if (!should_PSL_matching_apply && !should_federated_apply) {
    // Otherwise the histogram is reported in StatementToForms.
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                              PSL_DOMAIN_MATCH_NOT_USED,
                              PSL_DOMAIN_MATCH_COUNT);
  }
  PrimaryKeyToFormMap key_to_form_map;
  FormRetrievalResult result = StatementToForms(
      &s, should_PSL_matching_apply || should_federated_apply ? &form : nullptr,
      &key_to_form_map);
  if (result != FormRetrievalResult::kSuccess) {
    return false;
  }
  for (auto& pair : key_to_form_map) {
    forms->push_back(std::move(pair.second));
  }
  return true;
}

bool LoginDatabase::GetLoginsByPassword(
    const base::string16& plain_text_password,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) {
  DCHECK(forms);
  forms->clear();

  // Get all autofillable (not blacklisted) logins.
  DCHECK(!blacklisted_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, blacklisted_statement_.c_str()));
  s.BindInt(0, 0);  // blacklisted = false

  // Apply query, check status and copy results if successful.
  PrimaryKeyToFormMap key_to_form_map;
  FormRetrievalResult result =
      StatementToForms(&s, /*matched_form=*/nullptr, &key_to_form_map);

  if (result != FormRetrievalResult::kSuccess) {
    return false;
  }
  for (auto& pair : key_to_form_map) {
    if (pair.second->password_value == plain_text_password) {
      // Only add the form the result if the password value matches.
      forms->push_back(std::move(pair.second));
    }
  }
  return true;
}

bool LoginDatabase::GetLoginsCreatedBetween(
    const base::Time begin,
    const base::Time end,
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(key_to_form_map);
  DCHECK(!created_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, created_statement_.c_str()));
  s.BindInt64(0, begin.ToInternalValue());
  s.BindInt64(1, end.is_null() ? std::numeric_limits<int64_t>::max()
                               : end.ToInternalValue());

  return StatementToForms(&s, nullptr, key_to_form_map) ==
         FormRetrievalResult::kSuccess;
}

FormRetrievalResult LoginDatabase::GetAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(key_to_form_map);
  key_to_form_map->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, "SELECT * FROM logins"));

  return StatementToForms(&s, nullptr, key_to_form_map);
}

bool LoginDatabase::GetAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetAllLoginsWithBlacklistSetting(false, forms);
}

bool LoginDatabase::GetBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  return GetAllLoginsWithBlacklistSetting(true, forms);
}

bool LoginDatabase::GetAllLoginsWithBlacklistSetting(
    bool blacklisted,
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(forms);
  DCHECK(!blacklisted_statement_.empty());
  forms->clear();

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, blacklisted_statement_.c_str()));
  s.BindInt(0, blacklisted ? 1 : 0);

  PrimaryKeyToFormMap key_to_form_map;

  if (StatementToForms(&s, nullptr, &key_to_form_map) !=
      FormRetrievalResult::kSuccess) {
    return false;
  }

  for (auto& pair : key_to_form_map) {
    forms->push_back(std::move(pair.second));
  }
  return true;
}

bool LoginDatabase::IsEmpty() {
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, "SELECT COUNT(*) FROM logins"));
  return s.Step() && s.ColumnInt(0) == 0;
}

bool LoginDatabase::DeleteAndRecreateDatabaseFile() {
  DCHECK(db_.is_open());
  meta_table_.Reset();
  db_.Close();
  sql::Database::Delete(db_path_);
  return Init();
}

DatabaseCleanupResult LoginDatabase::DeleteUndecryptableLogins() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // If the Keychain is unavailable, don't delete any logins.
  if (!OSCrypt::IsEncryptionAvailable()) {
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kEncryptionUnavailable);
    return DatabaseCleanupResult::kEncryptionUnavailable;
  }

  DCHECK(db_.is_open());

  // Get all autofillable (not blacklisted) logins.
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, blacklisted_statement_.c_str()));
  s.BindInt(0, 0);  // blacklisted = false

  std::vector<PasswordForm> forms_to_be_deleted;

  while (s.Step()) {
    std::string encrypted_password;
    s.ColumnBlobAsString(COLUMN_PASSWORD_VALUE, &encrypted_password);
    base::string16 decrypted_password;
    if (DecryptedString(encrypted_password, &decrypted_password) ==
        ENCRYPTION_RESULT_SUCCESS)
      continue;

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
    DCHECK(password_recovery_util_);
    password_recovery_util_->RecordPasswordRecovery();
    metrics_util::LogDeleteUndecryptableLoginsReturnValue(
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted);
    UMA_HISTOGRAM_COUNTS_100("PasswordManager.CleanedUpPasswords",
                             forms_to_be_deleted.size());
  }
#endif

  return DatabaseCleanupResult::kSuccess;
}

std::string LoginDatabase::GetEncryptedPassword(
    const PasswordForm& form) const {
  DCHECK(!encrypted_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, encrypted_statement_.c_str()));

  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);

  std::string encrypted_password;
  if (s.Step()) {
    s.ColumnBlobAsString(0, &encrypted_password);
  }
  return encrypted_password;
}

std::unique_ptr<syncer::MetadataBatch> LoginDatabase::GetAllSyncMetadata() {
  std::unique_ptr<syncer::MetadataBatch> metadata_batch =
      GetAllSyncEntityMetadata();
  if (metadata_batch == nullptr) {
    return nullptr;
  }

  std::unique_ptr<sync_pb::ModelTypeState> model_type_state =
      GetModelTypeState();
  if (model_type_state == nullptr) {
    return nullptr;
  }

  metadata_batch->SetModelTypeState(*model_type_state);
  return metadata_batch;
}

void LoginDatabase::DeleteAllSyncMetadata() {
  ClearAllSyncMetadata(&db_);
}

bool LoginDatabase::UpdateSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);

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

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "INSERT OR REPLACE INTO sync_entities_metadata "
                             "(storage_key, metadata) VALUES(?, ?)"));

  s.BindInt(0, storage_key_int);
  s.BindString(1, encrypted_metadata);

  return s.Run();
}

bool LoginDatabase::ClearSyncMetadata(syncer::ModelType model_type,
                                      const std::string& storage_key) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  int storage_key_int = 0;
  if (!base::StringToInt(storage_key, &storage_key_int)) {
    DLOG(ERROR) << "Invalid storage key. Failed to convert the storage key to "
                   "an integer.";
    return false;
  }

  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "DELETE FROM sync_entities_metadata WHERE "
                             "storage_key=?"));
  s.BindInt(0, storage_key_int);

  return s.Run();
}

bool LoginDatabase::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  // Make sure only one row is left by storing it in the entry with id=1
  // every time.
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO sync_model_metadata (id, model_metadata) "
      "VALUES(1, ?)"));
  s.BindString(0, model_type_state.SerializeAsString());

  return s.Run();
}

bool LoginDatabase::ClearModelTypeState(syncer::ModelType model_type) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);

  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM sync_model_metadata WHERE id=1"));

  return s.Run();
}

bool LoginDatabase::BeginTransaction() {
  return db_.BeginTransaction();
}

void LoginDatabase::RollbackTransaction() {
  db_.RollbackTransaction();
}

bool LoginDatabase::CommitTransaction() {
  return db_.CommitTransaction();
}

int LoginDatabase::GetPrimaryKey(const PasswordForm& form) const {
  DCHECK(!id_statement_.empty());
  sql::Statement s(
      db_.GetCachedStatement(SQL_FROM_HERE, id_statement_.c_str()));

  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString(4, form.signon_realm);

  if (s.Step()) {
    return s.ColumnInt(0);
  }
  return -1;
}

std::unique_ptr<syncer::MetadataBatch>
LoginDatabase::GetAllSyncEntityMetadata() {
  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
                                          "SELECT storage_key, metadata FROM "
                                          "sync_entities_metadata"));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string encrypted_serialized_metadata = s.ColumnString(1);
    std::string decrypted_serialized_metadata;
    if (!OSCrypt::DecryptString(encrypted_serialized_metadata,
                                &decrypted_serialized_metadata)) {
      DLOG(WARNING) << "Failed to decrypt PASSWORD model type "
                       "sync_pb::EntityMetadata.";
      return nullptr;
    }

    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(decrypted_serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize PASSWORD model type "
                       "sync_pb::EntityMetadata.";
      return nullptr;
    }
  }
  if (!s.Succeeded()) {
    return nullptr;
  }
  return metadata_batch;
}

std::unique_ptr<sync_pb::ModelTypeState> LoginDatabase::GetModelTypeState() {
  auto state = std::make_unique<sync_pb::ModelTypeState>();
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT model_metadata FROM sync_model_metadata WHERE id=1"));

  if (!s.Step()) {
    if (s.Succeeded())
      return state;
    else
      return nullptr;
  }

  std::string serialized_state = s.ColumnString(0);
  if (state->ParseFromString(serialized_state)) {
    return state;
  }
  return nullptr;
}

FormRetrievalResult LoginDatabase::StatementToForms(
    sql::Statement* statement,
    const PasswordStore::FormDigest* matched_form,
    PrimaryKeyToFormMap* key_to_form_map) {
  PSLDomainMatchMetric psl_domain_match_metric = PSL_DOMAIN_MATCH_NONE;

  std::vector<PasswordForm> forms_to_be_deleted;

  key_to_form_map->clear();
  while (statement->Step()) {
    auto new_form = std::make_unique<PasswordForm>();
    new_form->from_store = is_account_store()
                               ? PasswordForm::Store::kAccountStore
                               : PasswordForm::Store::kProfileStore;
    int primary_key = -1;
    EncryptionResult result = InitPasswordFormFromStatement(
        *statement, /*decrypt_and_fill_password_value=*/true, &primary_key,
        new_form.get());
    if (result == ENCRYPTION_RESULT_SERVICE_FAILURE)
      return FormRetrievalResult::kEncrytionServiceFailure;
    if (result == ENCRYPTION_RESULT_ITEM_FAILURE) {
      if (IsUsingCleanupMechanism())
        forms_to_be_deleted.push_back(GetFormForRemoval(*statement));
      continue;
    }
    DCHECK_EQ(ENCRYPTION_RESULT_SUCCESS, result);

    if (matched_form) {
      switch (GetMatchResult(*new_form, *matched_form)) {
        case MatchResult::NO_MATCH:
          continue;
        case MatchResult::EXACT_MATCH:
          break;
        case MatchResult::PSL_MATCH:
          psl_domain_match_metric = PSL_DOMAIN_MATCH_FOUND;
          new_form->is_public_suffix_match = true;
          break;
        case MatchResult::FEDERATED_MATCH:
          break;
        case MatchResult::FEDERATED_PSL_MATCH:
          psl_domain_match_metric = PSL_DOMAIN_MATCH_FOUND_FEDERATED;
          new_form->is_public_suffix_match = true;
          break;
      }
    }

    key_to_form_map->emplace(primary_key, std::move(new_form));
  }

  if (matched_form) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.PslDomainMatchTriggering",
                              psl_domain_match_metric, PSL_DOMAIN_MATCH_COUNT);
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Remove corrupted passwords.
  size_t count_removed_logins = 0;
  for (const auto& form : forms_to_be_deleted) {
    if (RemoveLogin(form, nullptr)) {
      count_removed_logins++;
    }
  }

  if (count_removed_logins > 0) {
    UMA_HISTOGRAM_COUNTS_100("PasswordManager.RemovedCorruptedPasswords",
                             count_removed_logins);
  }

  if (count_removed_logins != forms_to_be_deleted.size()) {
    metrics_util::LogDeleteCorruptedPasswordsResult(
        metrics_util::DeleteCorruptedPasswordsResult::kItemFailure);
  } else if (count_removed_logins > 0) {
    DCHECK(password_recovery_util_);
    password_recovery_util_->RecordPasswordRecovery();
    metrics_util::LogDeleteCorruptedPasswordsResult(
        metrics_util::DeleteCorruptedPasswordsResult::kSuccessPasswordsDeleted);
  }
#endif

  if (!statement->Succeeded())
    return FormRetrievalResult::kDbError;
  return FormRetrievalResult::kSuccess;
}

void LoginDatabase::InitializeStatementStrings(const SQLTableBuilder& builder) {
  // This method may be called multiple times, if Chrome switches backends and
  // LoginDatabase::DeleteAndRecreateDatabaseFile ends up being called. In those
  // case do not recompute the SQL statements, because they would end up the
  // same.
  if (!add_statement_.empty())
    return;

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
  DCHECK(created_statement_.empty());
  created_statement_ =
      "SELECT " + all_column_names +
      " FROM logins WHERE date_created >= ? AND date_created < "
      "? ORDER BY origin_url";
  DCHECK(blacklisted_statement_.empty());
  blacklisted_statement_ =
      "SELECT " + all_column_names +
      " FROM logins WHERE blacklisted_by_user == ? ORDER BY origin_url";
  DCHECK(encrypted_statement_.empty());
  encrypted_statement_ =
      "SELECT password_value FROM logins WHERE " + all_unique_key_column_names;
  DCHECK(encrypted_password_statement_by_id_.empty());
  encrypted_password_statement_by_id_ =
      "SELECT password_value FROM logins WHERE id=?";
  DCHECK(id_statement_.empty());
  id_statement_ = "SELECT id FROM logins WHERE " + all_unique_key_column_names;
}

bool LoginDatabase::IsUsingCleanupMechanism() const {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  return base::FeatureList::IsEnabled(features::kDeleteCorruptedPasswords);
#else
  return false;
#endif
}

}  // namespace password_manager
