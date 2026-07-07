// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_list.h"
#include "components/safe_browsing/core/browser/db/sb_store.h"
#include "components/safe_browsing/core/browser/db/sb_store_file_format.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace safe_browsing {

namespace V5 {
class HashList;
}

// Enumerate different results of the migration attempt from v4 to v5.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(V4ToV5MigrationResult)
enum class V4ToV5MigrationResult {
  // The disk is already in v5 format (migration not needed).
  kDiskAlreadyV5 = 0,

  // The migration from v4 to v5 completed successfully.
  kV4ToV5MigrationSucceeded = 1,

  // The v4 store file was not found on disk.
  kV4StoreNotFound = 2,

  // Failed to read or validate the v4 store file from disk.
  kReadV4Failed = 3,

  // The v4 store file has multiple hash files, which is not supported.
  kMultipleHashFilesFailure = 4,

  // The prefix size in v4 hash file doesn't match the expected V5 prefix size.
  kPrefixSizeMismatchFailure = 5,

  // The referenced v4 hash file is missing from disk.
  kHashFileMissingFailure = 6,

  // Failed to rename/move the v4 hash file to the v5 path.
  kRenameHashFileFailure = 7,

  // Failed to write the new V5StoreFileFormat proto to disk.
  kWriteV5FileFailure = 8,

  // Failed to serialize the V5StoreFileFormat proto.
  kProtoSerializationFailure = 9,

  // Failed to parse or validate the extension of the v4 hash file.
  kExtensionParsingFailure = 10,

  // Failed to rename the temp V5 store file to the final path.
  kRenameV5StoreFileFailure = 11,

  // V4 to V5 migration was ineligible, and wiping V4 failed.
  kStoreIneligibleWipeFailed = 12,

  // V4 to V5 migration was ineligible, and wiping V4 succeeded.
  kStoreIneligibleWipeSucceeded = 13,

  // Failed to migrate extensions blocklist due to conversion or write failure.
  kExtensionBlocklistMigrationFailed = 14,

  kMaxValue = kExtensionBlocklistMigrationFailed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:V4ToV5MigrationResult)

// Enumerate different results of converting the extensions blocklist from v4 to
// v5.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ConvertExtensionBlocklistV4ToV5Result)
enum class ConvertExtensionBlocklistV4ToV5Result {
  // Conversion completed successfully.
  kSuccess = 0,

  // Failed to read the V4 hash file.
  kReadV4Failed = 1,

  // The V4 hash file size was not a multiple of 32 bytes.
  kInvalidFileSize = 2,

  // An extension ID in the V4 hash file was invalid.
  kInvalidExtensionId = 3,

  // Failed to write the converted V5 hash file.
  kWriteV5Failed = 4,

  // The V4 store's checksum did not match the V4 data on disk.
  kV4ChecksumMismatch = 5,

  kMaxValue = kV4ChecksumMismatch
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:ConvertExtensionBlocklistV4ToV5Result)

// Enumerate different failure events while writing the file to disk after
// applying updates for histogramming purposes.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(V5StoreWriteResult)
enum class V5StoreWriteResult {
  // No errors.
  kWriteSuccess = 0,

  // An unexpected error occurred while writing the file.
  kUnexpectedWriteFailure = 1,

  // Number of bytes written to disk was different from the size of the proto.
  kUnexpectedBytesWrittenFailure = 2,

  // Renaming the temporary file to store file failed.
  kUnableToRenameFailure = 3,

  kMaxValue = kUnableToRenameFailure
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5StoreWriteResult)

class V5Store : public SBStore {
 public:
  // The `task_runner` is used to ensure that the operations in this file are
  // performed on the correct thread. `store_path` specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // `v4_store_path` is the store path for the corresponding v4 store; this is
  // used for migrating from a v4 database to a v5 database.
  // `is_eligible_for_v4_to_v5_disk_migration` specifies whether this store is
  // eligible to migrate its old V4 disk format to V5.
  // `is_extensions_blocklist` specifies whether this store is for the
  // extensions blocklist.
  // If the store is being created to apply an update to the old store, then
  // `old_file_size` is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  V5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          PrefixSize prefix_size,
          const base::FilePath& v4_store_path,
          bool is_eligible_for_v4_to_v5_disk_migration,
          bool is_extensions_blocklist,
          int64_t old_file_size = 0);
  ~V5Store() override;

  const std::string& version() const { return version_; }

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

  int64_t RecordAndReturnFileSize(const std::string& base_metric) override;

  void Reset() override;

  bool VerifyChecksum() override;

  void CollectStoreInfo(
      DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info) override;

  const std::string& GetStoreState() const override;

  HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash) override;

  void ApplyUpdate(std::unique_ptr<SBUpdateResponse> response,
                   const scoped_refptr<base::SequencedTaskRunner>& runner,
                   UpdatedStoreReadyCallback callback) override;

 protected:
  std::string GetMetricPrefix() const override;

 private:
  friend class V5StoreTest;

  // Reads the state of the store from the file on disk, attempting migration
  // from v4 if necessary. Returns the reason for the failure or reports
  // success.
  V5StoreReadResult ReadFromDisk();

  // Reads the state of the store from the v5 file on disk directly. Returns the
  // reason for the failure or reports success.
  V5StoreReadResult ReadFromDiskInternal();

  // Helper method for performing the actual checksum verification logic.
  // Returns true if the calculated checksum matches the expected checksum.
  bool VerifyChecksumInternal();

  // Attempts to migrate the store from v4 to v5 if eligible and needed. Returns
  // the reason for the failure or reports success.
  V4ToV5MigrationResult AttemptV4ToV5Migration();

  // Performs the actual migration steps from the v4 store to v5. Returns the
  // reason for the failure or reports success.
  V4ToV5MigrationResult MigrateFromV4(const base::FilePath& v4_store_path);

  // Wipes the V4 store file and its associated hash files.
  // `v4_store_path` is the path of the V4 store to delete.
  // Returns true if both the store file and all of its associated hash files
  // are successfully deleted; false otherwise.
  bool WipeV4Store(const base::FilePath& v4_store_path);

  // Converts the extensions blocklist hash file from V4 to V5 format.
  // `v4_hash_file_path` is the path to the existing V4 hash file.
  // `v5_hash_file_path` is the path where the converted V5 hash file should be
  // written.
  // `checksum_sha256` is an in-out parameter. On input, it contains the
  // expected V4 checksum (if any) used to verify the V4 data before
  // conversion. On successful conversion, it is overwritten in-place with the
  // newly calculated V5 checksum of the converted data. Can be nullptr if no
  // checksum is expected.
  // `file_size` is an output parameter that will be populated with the size of
  // the converted hash data.
  // Returns `ConvertExtensionBlocklistV4ToV5Result::kSuccess` on success, or an
  // appropriate error code on failure.
  ConvertExtensionBlocklistV4ToV5Result ConvertExtensionsBlocklistFromV4ToV5(
      const base::FilePath& v4_hash_file_path,
      const base::FilePath& v5_hash_file_path,
      std::string* checksum_sha256,
      uint64_t* file_size);

  // Helper method to apply an update. It performs the update processing,
  // writes the result to disk, and validates the written file.
  //  - `v5_response` contains the V5 HashList update.
  //  - `metric` is the base metric string to be used for histograms.
  // Returns the new store pointer on success, or the update result error on
  // failure.
  base::expected<SBStorePtr, V5ApplyUpdateResult> ApplyUpdateInternal(
      std::unique_ptr<V5::HashList> v5_response,
      const std::string& metric);

  // Common update processing logic for both full and partial updates.
  //  - `response` contains the V5 HashList update.
  //  - `metric` is the base metric string to be used for histograms.
  //  - `is_full_update` is true if the update is a full update, false if it is
  //    a partial update. This is used for metrics.
  //  - `old_prefixes_list` contains the existing sorted hash prefixes in the
  //    store (for partial updates).
  // Returns `V5ApplyUpdateResult::kSuccess` if the update is successfully
  // processed and merged. Returns other `V5ApplyUpdateResult` values on failure
  // (e.g. decoding failure, checksum mismatch).
  V5ApplyUpdateResult ProcessUpdate(std::unique_ptr<V5::HashList> response,
                                    const std::string& metric,
                                    bool is_full_update,
                                    HashPrefixesView old_prefixes_list);

  // Writes the `hash_prefix_list_` to disk as a V5StoreFileFormat proto.
  V5StoreWriteResult WriteToDisk();

  std::unique_ptr<HashPrefixList> hash_prefix_list_;

  // The expected prefix size for the hash prefixes in this store.
  const PrefixSize prefix_size_;

  // The path to the corresponding v4 store file on disk.
  const base::FilePath v4_store_path_;

  // Whether this store is eligible for v4 to v5 disk migration.
  const bool is_eligible_for_migration_ = true;

  // Whether this store is for the extensions blocklist.
  const bool is_extensions_blocklist_ = false;

  // The version of the store as returned by the PVer5 server in the last
  // applied update response.
  std::string version_;

  // The checksum value as read from the disk. It is cleared if the store is
  // invalid. It is updated if there is a new checksum provided in an update.
  std::string expected_checksum_;

  // Records the status of the update being applied to the database.
  V5ApplyUpdateResult last_apply_update_result_ = V5ApplyUpdateResult::kUnknown;
};

using V5StorePtr = std::unique_ptr<V5Store, SBStoreDeleter>;

// Factory for creating V5Store instances. Implements SBStoreFactory.
class V5StoreFactory : public SBStoreFactory {
 public:
  ~V5StoreFactory() override = default;

  // SBStoreFactory implementation:
  SBStorePtr CreateStore(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfo& list_info) override;

  // Creates a V5Store.
  virtual V5StorePtr CreateV5Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path,
      PrefixSize prefix_size,
      const base::FilePath& v4_store_path,
      bool is_eligible_for_v4_to_v5_disk_migration,
      bool is_extensions_blocklist);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_
