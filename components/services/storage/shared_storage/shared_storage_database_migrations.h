// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_MIGRATIONS_H_
#define COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_MIGRATIONS_H_

#include "base/time/clock.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace storage {

// Changes to the SQL database schema or data format must be accompanied by
// a database migration. This includes new columns, new tables, or changes to
// existing stored data. Data loss must be avoided during migrations, because
// the impact could extend to weeks of data.
//
// To do a migration, add a new function `MigrateToVersionN()` which performs
// the modifications to the old database, and increment `kCurrentVersionNumber`
// in shared_storage_database.cc.
//
// Generate a new sql file which will hold the new database schema (with `N`
// replaced by the new version number):
//  * Build and open the Chromium executable
//  * Go to a site which uses shared storage to init the database
//   (e.g. https://shared-storage-demo.web.app/).
//  * Go to chrome://version if needed to confirm/copy the Profile Path
//    ($USERDATADIR below).
//  * Build the sqlite_shell executable:
//    > autoninja -C out/Default sqlite_shell
//  * Using the sqlite_shell executable do the following:
//    > out/Default/sqlite_shell
//    >> .open $USER_DATA_DIR/SharedStorage
//    >> .output version_N.sql
//    >> .dump
//    >> .quit
//  * Find file version_N.sql in your current directory.
//  * Run bash command:
//    > mv version_N.sql components/test/data/storage/shared_storage.vN.sql
//  * Replace any rows with relevant test data needed for below.
//  * Add newlines between statements as necessary.
//
// Add the new test file to the target "tests_bundle_data" in
// components/services/storage/BUILD.gn.
//
// Add a new test to `shared_storage_database_migration_unittest.cc` named
// "MigrateVersionNToCurrent" where N is the previous database version.
//
// Update other tests in `shared_storage_database_migration_unittest.cc` as
// necessary; in particular, "Migrate*ToCurrent" may need to be updated.
//
// Update the expectations in `VerifySharedStorageTablesAndColumns()` of
// `components/services/storage/shared_storage/shared_storage_test_utils.cc`
// for the new schema and indices.
//
// You will need to increment the `last_compatible_version` in
// `components/test/data/storage/shared_storage.init_too_new.sql`
//
// Other non-migration test files in `components/test/data/storage/` that must
// be using at least a compatible version are the following (files not listed
// below should not be updated). Update them if you have time. If you are
// deprecating the version that they're using, however, then you MUST update
// them (and ensure that any test data file name changes are also reflected in
// the code of the unit tests that load them):
//  * shared_storage.v*.filescheme.sql
//  * shared_storage.v*.single_origin.sql
//  * shared_storage.v*.iterator.sql
//  * shared_storage.v*.empty_values_mapping.5origins.sql
//  * shared_storage.v*.empty_values_mapping.6origins.sql
//  * shared_storage.v*.empty_values_mapping.7origins.sql
//  * shared_storage.v*.empty_values_mapping.8origins.sql

// Upgrades `db` to the latest schema, and updates the version stored in
// `meta_table` accordingly. Must be called with an open `db`. Returns false on
// failure.
[[nodiscard]] bool UpgradeSharedStorageDatabaseSchema(
    sql::Database& db,
    sql::MetaTable& meta_table,
    base::Clock* clock);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SHARED_STORAGE_SHARED_STORAGE_DATABASE_MIGRATIONS_H_
