# leveldb_proto Initialization State Enum Descriptions

[TOC]

## Initial State

### kSharedDbInitAttempted | Starting: DB init attempted

Recorded in `ProtoDatabaseSelector::InitUniqueOrShared` when starting, next step
is to initialize the unique database.

### kLegacyInitCalled | Legacy init called

Recorded in `ProtoDatabaseSelector::InitWithDatabase`, which is only used by
unit tests and perf tests, not a final state.

## Error States

### kFailureUniqueDbCorrupted | Failed: unique DB corruption

Recorded in `ProtoDatabaseSelector::OnInitUniqueDB` when unique database is
corrupted, executing callback with `kCorrupt`.

### kFailureNoSharedDBProviderUniqueFailed | Failed: No shared DB provider provided, unique DB failed

Recorded in `ProtoDatabaseSelector::OnInitUniqueDB` when unique DB fails to open
and `ProtoDatabaseImpl` is created without a shared DB provider, should only
happen in FakeDB and Perftests. executing callback with `kError`.

### kBothUniqueAndSharedFailedOpen | Failed: open for both unique and shared

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when both unique and
shared databases fail to open, shared DB can fail to open because
`SharedProtoDatabase::GetClientAsync` returns an error or when
`SharedProtoDatabaseProvider` returns no `SharedProtoDatabase` instance.
callback is executed with `kError`.

### kSharedDbClientMissingInitFailed | Failed: Shared DB client is missing

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when there's no shared
DB client and a shared database is requested, callback is executed with
`kError`.

### kSharedDbOpenFailed | Failed: Shared DB failed to open

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when there's no shared
DB client because it failed to open and a unique DB was requested, callback is
executed with `kError` because the shared DB could contain unmigrated data.

### kUniqueDbOpenFailed | Failed: Unique DB open failed

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when there’s no unique
DB because it failed to open, the callback is executed with `kError` as the
unique DB could contain unmigrated data.

## Success States

### kSuccessNoSharedDBProviderUniqueSucceeded | Success: No shared DB provider provided, unique DB OK

Recorded in `ProtoDatabaseSelector::OnInitUniqueDB` when unique DB opens
successfully but `ProtoDatabaseImpl` is created without a shared DB provider,
should only happen in FakeDB and Perftests. executing callback with `kOK`.

### kSharedDbClientMissingUniqueReturned | Success: Unique DB, no shared db present

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when there's no shared
DB client because it doesn't exist in disk and a unique database was requested,
callback is executed with `kOK` and the unique DB is used as this is expected to
happen before any migrations.

### kUniqueDbMissingSharedReturned | Success: Shared DB no unique db present

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when there's no unique
DB because it doesn't exist and the shared DB metadata reports that no migration
has been attempted, we set the database as migrated to shared, execute the
callback with `kOK` and use the shared DB.

### kMigratedSharedDbOpened | Success: opened migrated shared DB

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` when the unique DB
fails to open but the metadata reports that data is now in the shared DB.
callback is executed with `kOK` and the shared DB is used. This is expected to
happen after migrating and deleting the unique DB.

### kDeletionOfOldDataFailed | Success: failed to delete old data

Recorded in `ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld` when data
deletion fails before a migration, we’ll attempt to delete again next time,
callback is called with `kOK` and the old data is used.

### kMigrateToSharedFailed | Success: failed to migrate, using unique

Recorded in `ProtoDatabaseSelector::OnMigrationTransferComplete` when
`MigrationDelegate::DoMigration` returns an error, callback is executed with
`kOK` and the unique DB is used.

### kMigrateToUniqueFailed | Success: failed to migrate, using shared

Recorded in `ProtoDatabaseSelector::OnMigrationTransferComplete` when
`MigrationDelegate::DoMigration` returns an error, callback is executed with
`kOK` and the shared DB is used.

### kMigrateToSharedCompleteDeletionFailed | Success: migrated to shared, deletion failed

Recorded in `ProtoDatabaseSelector::OnMigrationCleanupComplete` after a
successful migration but old data fails to be deleted, callback is executed with
`kOK` and shared DB is used.

### kMigrateToUniqueCompleteDeletionFailed | Success: migrated to unique, deletion failed

Recorded in `ProtoDatabaseSelector::OnMigrationCleanupComplete` after a
successful migration but old data fails to be deleted, callback is executed with
`kOK` and unique DB is used.

### kMigrateToSharedSuccess | Success: migrated to shared

Recorded in `ProtoDatabaseSelector::OnMigrationCleanupComplete` after a
successful migration and data deletion, callback is executed with `kOK` and
shared DB is used.

### kMigrateToUniqueSuccess | Success: migrated to unique

Recorded in `ProtoDatabaseSelector::OnMigrationCleanupComplete` after a
successful migration and data deletion, callback is executed with `kOK` and
unique DB is used.

## Intermediate States

### kMigrateToSharedAttempted | Migration to shared attempted

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` and
`ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld`, indicates that data will
be migrated from unique to shared, not a final state. Next step is calling
`MigrationDelegate::DoMigration`.

### kMigrateToUniqueAttempted | Migration to unique attempted

Recorded in `ProtoDatabaseSelector::OnGetSharedDBClient` and
`ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld`, indicates that data will
be migrated from shared to unique, not a final state. Next step is calling
`MigrationDelegate::DoMigration`.

## Shared DB States

### kSharedDbMetadataLoadFailed | Shared metadata db open failed

Recorded in `SharedProtoDatabase::OnGetClientMetadata` when the database for
`SharedDBMetadataProto` fails to load metadata for the specified database,
expected for the first time, not a final state.

### kSharedDbMetadataWriteFailed | Shared metadata db has no migration status

Recorded in `SharedProtoDatabase::OnGetClientMetadata` when metadata is
retrieved for a client and it has no migration state, we write
`MIGRATION_NOT_ATTEMPTED` back to the database and regardless of the DB’s result
we report that no migration has been attempted.

### kSharedDbClientCorrupt | Shared db corruption for client

Recorded in `SharedProtoDatabase::OnGetClientMetadata` when the shared database
experienced data corruption in an earlier initialization, shared DB callback is
executed with `kCorrupt`.

### kSharedDbClientSuccess | Shared db client created

Recorded in `SharedProtoDatabase::OnGetClientMetadata` when the shared database
opened successfully without corruption, the shared DB callback is executed with
`kOK`.

### kSharedLevelDbInitFailure | Shared leveldb failed to open

Recorded in `SharedProtoDatabase::Init` and
`SharedProtoDatabase::CheckCorruptionAndRunInitCallback` when the shared proto
DB failed to open, the shared DB callback is executed with `kError` (or any
other non `kOK` result)

### kSharedDbClientMissing | Shared leveldb does not exist

Recorded in `SharedProtoDatabase::Init` when the shared proto DB does not exist,
the shared DB callback is executed with `kInvalidOperation`.
