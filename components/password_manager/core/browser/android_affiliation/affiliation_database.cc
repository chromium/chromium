// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_database.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace password_manager {

namespace {

// The current version number of the affiliation database schema.
const int kVersion = 2;

// The oldest version of the schema such that a legacy Chrome client using that
// version can still read/write the current database.
const int kCompatibleVersion = 1;

}  // namespace

AffiliationDatabase::AffiliationDatabase() {
}

AffiliationDatabase::~AffiliationDatabase() {
}

bool AffiliationDatabase::Init(const base::FilePath& path) {
  sql_connection_.reset(new sql::Database);
  sql_connection_->set_histogram_tag("Affiliation");
  sql_connection_->set_error_callback(base::Bind(
      &AffiliationDatabase::SQLErrorCallback, base::Unretained(this)));

  if (!sql_connection_->Open(path))
    return false;

  if (!sql_connection_->Execute("PRAGMA foreign_keys=1")) {
    sql_connection_->Poison();
    return false;
  }

  sql::MetaTable metatable;
  if (!metatable.Init(sql_connection_.get(), kVersion, kCompatibleVersion)) {
    sql_connection_->Poison();
    return false;
  }

  if (metatable.GetCompatibleVersionNumber() > kVersion) {
    LOG(WARNING) << "AffiliationDatabase is too new.";
    sql_connection_->Poison();
    return false;
  }

  SQLTableBuilder eq_classes_builder("eq_classes");
  SQLTableBuilder eq_class_members_builder("eq_class_members");
  InitializeTableBuilders(&eq_classes_builder, &eq_class_members_builder);

  if (!CreateTables(eq_classes_builder, eq_class_members_builder)) {
    LOG(WARNING) << "Failed to create tables.";
    sql_connection_->Poison();
    return false;
  }

  int version = metatable.GetVersionNumber();
  if (version < kVersion) {
    if (!MigrateTablesFrom(eq_classes_builder, eq_class_members_builder,
                           version)) {
      LOG(WARNING) << "Failed to migrate tables from version " << version
                   << ".";
      sql_connection_->Poison();
      return false;
    }

    // Set the current version number is case of a successful migration.
    metatable.SetVersionNumber(kVersion);
  }

  return true;
}

bool AffiliationDatabase::GetAffiliationsAndBrandingForFacetURI(
    const FacetURI& facet_uri,
    AffiliatedFacetsWithUpdateTime* result) const {
  DCHECK(result);
  result->facets.clear();

  sql::Statement statement(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m2.facet_uri, m2.facet_display_name, m2.facet_icon_url,"
      "    c.last_update_time "
      "FROM eq_class_members m1, eq_class_members m2, eq_classes c "
      "WHERE m1.facet_uri = ? AND m1.set_id = m2.set_id AND m1.set_id = c.id"));
  statement.BindString(0, facet_uri.canonical_spec());

  while (statement.Step()) {
    result->facets.push_back(
        {FacetURI::FromCanonicalSpec(statement.ColumnString(0)),
         FacetBrandingInfo{
             statement.ColumnString(1), GURL(statement.ColumnString(2)),
         }});
    result->last_update_time =
        base::Time::FromInternalValue(statement.ColumnInt64(3));
  }

  return !result->facets.empty();
}

void AffiliationDatabase::GetAllAffiliationsAndBranding(
    std::vector<AffiliatedFacetsWithUpdateTime>* results) const {
  DCHECK(results);
  results->clear();

  sql::Statement statement(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m.facet_uri, m.facet_display_name, m.facet_icon_url,"
      "    c.last_update_time, c.id "
      "FROM eq_class_members m, eq_classes c "
      "WHERE m.set_id = c.id "
      "ORDER BY c.id"));

  int64_t last_eq_class_id = 0;
  while (statement.Step()) {
    int64_t eq_class_id = statement.ColumnInt64(4);
    if (results->empty() || eq_class_id != last_eq_class_id) {
      results->push_back(AffiliatedFacetsWithUpdateTime());
      last_eq_class_id = eq_class_id;
    }
    results->back().facets.push_back(
        {FacetURI::FromCanonicalSpec(statement.ColumnString(0)),
         FacetBrandingInfo{
             statement.ColumnString(1), GURL(statement.ColumnString(2)),
         }});
    results->back().last_update_time =
        base::Time::FromInternalValue(statement.ColumnInt64(3));
  }
}

void AffiliationDatabase::DeleteAffiliationsAndBrandingForFacetURI(
    const FacetURI& facet_uri) {
  sql::Transaction transaction(sql_connection_.get());
  if (!transaction.Begin())
    return;

  sql::Statement statement_lookup(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT m.set_id FROM eq_class_members m "
      "WHERE m.facet_uri = ?"));
  statement_lookup.BindString(0, facet_uri.canonical_spec());

  // No such |facet_uri|, nothing to do.
  if (!statement_lookup.Step())
    return;

  int64_t eq_class_id = statement_lookup.ColumnInt64(0);

  // Children will get deleted due to 'ON DELETE CASCADE'.
  sql::Statement statement_parent(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM eq_classes WHERE eq_classes.id = ?"));
  statement_parent.BindInt64(0, eq_class_id);
  if (!statement_parent.Run())
    return;

  transaction.Commit();
}

bool AffiliationDatabase::Store(
    const AffiliatedFacetsWithUpdateTime& affiliated_facets) {
  DCHECK(!affiliated_facets.facets.empty());

  sql::Statement statement_parent(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO eq_classes(last_update_time) VALUES (?)"));

  sql::Statement statement_child(sql_connection_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO "
      "eq_class_members(facet_uri, facet_display_name, facet_icon_url, set_id) "
      "VALUES (?, ?, ?, ?)"));

  sql::Transaction transaction(sql_connection_.get());
  if (!transaction.Begin())
    return false;

  statement_parent.BindInt64(
      0, affiliated_facets.last_update_time.ToInternalValue());
  if (!statement_parent.Run())
    return false;

  int64_t eq_class_id = sql_connection_->GetLastInsertRowId();
  for (const Facet& facet : affiliated_facets.facets) {
    statement_child.Reset(true);
    statement_child.BindString(0, facet.uri.canonical_spec());
    statement_child.BindString(1, facet.branding_info.name);
    statement_child.BindString(
        2, facet.branding_info.icon_url.possibly_invalid_spec());
    statement_child.BindInt64(3, eq_class_id);
    if (!statement_child.Run())
      return false;
  }

  return transaction.Commit();
}

void AffiliationDatabase::StoreAndRemoveConflicting(
    const AffiliatedFacetsWithUpdateTime& affiliation,
    std::vector<AffiliatedFacetsWithUpdateTime>* removed_affiliations) {
  DCHECK(!affiliation.facets.empty());
  DCHECK(removed_affiliations);
  removed_affiliations->clear();

  sql::Transaction transaction(sql_connection_.get());
  if (!transaction.Begin())
    return;

  for (const Facet& facet : affiliation.facets) {
    AffiliatedFacetsWithUpdateTime old_affiliation;
    if (GetAffiliationsAndBrandingForFacetURI(facet.uri, &old_affiliation)) {
      if (!AreEquivalenceClassesEqual(old_affiliation.facets,
                                      affiliation.facets)) {
        removed_affiliations->push_back(old_affiliation);
      }
      DeleteAffiliationsAndBrandingForFacetURI(facet.uri);
    }
  }

  if (!Store(affiliation))
    NOTREACHED();

  transaction.Commit();
}

// static
void AffiliationDatabase::Delete(const base::FilePath& path) {
  bool success = sql::Database::Delete(path);
  DCHECK(success);
}

int AffiliationDatabase::GetDatabaseVersionForTesting() {
  sql::MetaTable metatable;
  // The second and third parameters to |MetaTable::Init| are ignored, given
  // that a metatable already exists. Hence they are not influencing the version
  // of the underlying database.
  DCHECK(sql::MetaTable::DoesTableExist(sql_connection_.get()));
  metatable.Init(sql_connection_.get(), 1, 1);
  return metatable.GetVersionNumber();
}

// static
void AffiliationDatabase::InitializeTableBuilders(
    SQLTableBuilder* eq_classes_builder,
    SQLTableBuilder* eq_class_members_builder) {
  // Version 1 of the affiliation database.
  eq_classes_builder->AddPrimaryKeyColumn("id");
  eq_classes_builder->AddColumn("last_update_time", "INTEGER");
  // The first call to |SealVersion| sets the version to 0, that's why it is
  // repeated.
  eq_classes_builder->SealVersion();
  unsigned eq_classes_version = eq_classes_builder->SealVersion();
  DCHECK_EQ(1u, eq_classes_version);

  eq_class_members_builder->AddPrimaryKeyColumn("id");
  eq_class_members_builder->AddColumnToUniqueKey("facet_uri",
                                                 "LONGVARCHAR NOT NULL");
  eq_class_members_builder->AddColumn(
      "set_id", "INTEGER NOT NULL REFERENCES eq_classes(id) ON DELETE CASCADE");
  // An index on eq_class_members.facet_uri is automatically created due to the
  // UNIQUE constraint, however, we must create one on eq_class_members.set_id
  // manually (to prevent linear scan when joining).
  eq_class_members_builder->AddIndex("index_on_eq_class_members_set_id",
                                     {"set_id"});
  // The first call to |SealVersion| sets the version to 0, that's why it is
  // repeated.
  eq_class_members_builder->SealVersion();
  unsigned eq_class_members_version = eq_class_members_builder->SealVersion();
  DCHECK_EQ(1u, eq_class_members_version);

  // Version 2 of the affiliation database.
  eq_classes_version = eq_classes_builder->SealVersion();
  DCHECK_EQ(2u, eq_classes_version);

  eq_class_members_builder->AddColumn("facet_display_name", "VARCHAR");
  eq_class_members_builder->AddColumn("facet_icon_url", "VARCHAR");
  eq_class_members_version = eq_class_members_builder->SealVersion();
  DCHECK_EQ(2u, eq_class_members_version);
}

bool AffiliationDatabase::CreateTables(
    const SQLTableBuilder& eq_classes_builder,
    const SQLTableBuilder& eq_class_members_builder) {
  return eq_classes_builder.CreateTable(sql_connection_.get()) &&
         eq_class_members_builder.CreateTable(sql_connection_.get());
}

bool AffiliationDatabase::MigrateTablesFrom(
    const SQLTableBuilder& eq_classes_builder,
    const SQLTableBuilder& eq_class_members_builder,
    unsigned version) {
  return eq_classes_builder.MigrateFrom(version, sql_connection_.get()) &&
         eq_class_members_builder.MigrateFrom(version, sql_connection_.get());
}

void AffiliationDatabase::SQLErrorCallback(int error,
                                           sql::Statement* statement) {
  if (sql::IsErrorCatastrophic(error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndClose() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    sql_connection_->RazeAndClose();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(error))
    DLOG(FATAL) << sql_connection_->GetErrorMessage();
}

}  // namespace password_manager
