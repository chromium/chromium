// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Design doc: go/design-doc-v4store

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_STORE_H_

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"
#include "components/safe_browsing/core/browser/db/sb_store.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_rice.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace safe_browsing {

class V4Store;
class ListInfo;

using V4StorePtr = std::unique_ptr<V4Store, SBStoreDeleter>;

// Stores the iterator to the last element merged from the HashPrefixMap for a
// given prefix size.
// For instance: {4:iter(3), 5:iter(1)} means that we have already merged
// 3 hash prefixes of length 4, and 1 hash prefix of length 5.
using IteratorMap =
    std::unordered_map<PrefixSize, HashPrefixesView::const_iterator>;

// Enumerate different failure events while writing the file to disk after
// applying updates for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreWriteResult {
  // No errors.
  WRITE_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_WRITE_FAILURE = 1,

  // The proto being written to disk wasn't a FULL_UPDATE proto.
  INVALID_RESPONSE_TYPE_FAILURE = 2,

  // Number of bytes written to disk was different from the size of the proto.
  UNEXPECTED_BYTES_WRITTEN_FAILURE = 3,

  // Renaming the temporary file to store file failed.
  UNABLE_TO_RENAME_FAILURE = 4,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_WRITE_RESULT_MAX
};

// Factory for creating V4Store instances. Implements SBStoreFactory. Tests
// implement this factory to create fake stores for testing.
class V4StoreFactory : public SBStoreFactory {
 public:
  ~V4StoreFactory() override = default;

  // SBStoreFactory implementation:
  SBStorePtr CreateStore(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfo& list_info) override;

  // Creates a V4Store.
  // |task_runner| is used to ensure operations are done on the correct thread.
  // |store_path| specifies the location on disk for this store.
  // |v5_prefix_size| is the prefix size of the corresponding V5 store if
  // we plan to migrate.
  // |is_eligible_for_migration| specifies whether this store is eligible
  // to migrate between V4 and V5 disk formats.
  // |is_extensions_blocklist| specifies whether this store is for the
  // extensions blocklist.
  virtual V4StorePtr CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path,
      PrefixSize v5_prefix_size,
      bool is_eligible_for_migration,
      bool is_extensions_blocklist);
};

// Enumerate different results of the migration attempt from v5 to v4.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(V5ToV4MigrationResult)
enum class V5ToV4MigrationResult {
  // The disk is already in v4 format (migration not needed).
  kDiskAlreadyV4 = 0,

  // The migration from v5 to v4 completed successfully.
  kV5ToV4MigrationSucceeded = 1,

  // The v5 store file was not found on disk.
  kV5StoreNotFound = 2,

  // Failed to read or validate the v5 store file from disk.
  kReadV5Failed = 3,

  // The prefix size in v5 hash file doesn't match the expected V4 prefix size.
  kPrefixSizeMismatchFailure = 4,

  // The referenced v5 hash file is missing from disk.
  kHashFileMissingFailure = 5,

  // Failed to rename/move the v5 hash file to the v4 path.
  kRenameHashFileFailure = 6,

  // Failed to write the new V4StoreFileFormat proto to disk.
  kWriteV4FileFailure = 7,

  // Failed to rename the temp V4 store file to the final path.
  kRenameV4StoreFileFailure = 8,

  // Failed to serialize the new V4StoreFileFormat proto.
  kProtoSerializationFailure = 9,

  // V5 to V4 migration was ineligible, and wiping V5 failed.
  kStoreIneligibleWipeFailed = 10,

  // V5 to V4 migration was ineligible, and wiping V5 succeeded.
  kStoreIneligibleWipeSucceeded = 11,

  // Failed to migrate extensions blocklist due to conversion or write failure.
  kExtensionBlocklistMigrationFailed = 12,

  kMaxValue = kExtensionBlocklistMigrationFailed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:V5ToV4MigrationResult)

// Enumerate different results of converting the extensions blocklist from v5 to
// v4. These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ConvertExtensionBlocklistV5ToV4Result)
enum class ConvertExtensionBlocklistV5ToV4Result {
  // Conversion succeeded.
  kSuccess = 0,

  // Failed to read the v5 hash file.
  kReadV5Failed = 1,

  // The v5 hash file size is not a multiple of the expected hash size.
  kInvalidFileSize = 2,

  // Failed to write the converted v4 hash file.
  kWriteV4Failed = 3,

  // The V5 store's checksum did not match the V5 data on disk.
  kV5ChecksumMismatch = 4,

  kMaxValue = kV5ChecksumMismatch
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:ConvertExtensionBlocklistV5ToV4Result)

class V4Store : public SBStore {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // |v5_prefix_size| is the prefix size of the corresponding V5 store if
  // we plan to migrate.
  // |is_eligible_for_migration| specifies whether this store is eligible
  // to migrate between V4 and V5 disk formats.
  // |is_extensions_blocklist| specifies whether this store is for the
  // extensions blocklist.
  // If the store is being created to apply an update to the old store, then
  // |old_file_size| is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          PrefixSize v5_prefix_size,
          bool is_eligible_for_migration,
          bool is_extensions_blocklist,
          int64_t old_file_size = 0);
  ~V4Store() override;

  HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash) override;

  const std::string& state() const { return state_; }

  void ApplyUpdate(std::unique_ptr<SBUpdateResponse> response,
                   const scoped_refptr<base::SequencedTaskRunner>& runner,
                   UpdatedStoreReadyCallback callback) override;

  int64_t RecordAndReturnFileSize(const std::string& base_metric) override;

  std::string DebugString() const;

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

  void Reset() override;

  bool VerifyChecksum() override;

  void CollectStoreInfo(
      DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info) override;

  const std::string& GetStoreState() const override;

 protected:
  std::string GetMetricPrefix() const override;

  std::unique_ptr<HashPrefixMap> hash_prefix_map_;

 private:
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromEmptyFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromAbsentFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromInvalidContentsFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromUnexpectedMagicFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromLowVersionFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromNoHashPrefixInfoFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromNoHashPrefixesFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWriteNoResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWritePartialResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestWriteFullResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestReadFromFileWithUnknownProto);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestAddUnlumpedHashesWithInvalidAddition);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestAddUnlumpedHashes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestAddUnlumpedHashesWithEmptyString);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestAddUnlumpedHashesWithTooSmallPrefixSize);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestAddUnlumpedHashesWithTooLargePrefixSize);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestGetNextSmallestUnmergedPrefixWithEmptyPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestGetNextSmallestUnmergedPrefix);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesWithSameSizesInEachMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesWithDifferentSizesInEachMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesOldMapRunsOutFirst);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesAdditionsMapRunsOutFirst);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesFailsForRepeatedHashPrefix);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesFailsWhenRemovalsIndexTooLarge);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesFailsWhenRemovalsIndexNegative);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdateFastPathWithRemovals);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdateFastPathEmptyLists);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdateFastPathMultipleRemovalsInARow);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestVerifyChecksumFastPath);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestVerifyChecksumValidStoreChecksumEmptyHistogram);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesOnlyElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesFirstElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesMiddleElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesRemovesLastElement);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesRemovesWhenOldHasDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMergeUpdatesRemovesMultipleAcrossDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestReadFullResponseWithValidHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestReadFullResponseWithInvalidHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestWriteFullResponseWithInvalidHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsAtTheBeginning);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsInTheMiddle);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsAtTheEnd);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixExistsAtTheBeginningOfEven);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestHashPrefixExistsAtTheEndOfEven);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixDoesNotExistInConcatenatedList);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestFullHashExistsInMapWithSingleSize);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestFullHashExistsInMapWithDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixExistsInMapWithSingleSize);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixExistsInMapWithDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestHashPrefixDoesNotExistInMapWithDifferentSizes);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, GetMatchingHashPrefixSize32Or21);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestAdditionsWithRiceEncodingFailsWithInvalidInput);
  FRIEND_TEST_ALL_PREFIXES(
      V4StoreTest,
      TestAdditionsWithRiceEncodingFailsWithInvalidCompressionType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestAdditionsWithRiceEncodingSucceeds);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestRemovalsWithRiceEncodingSucceeds);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesFailsChecksum);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestChecksumErrorOnStartup);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, WriteToDiskFails);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, FullUpdateFailsChecksumSynchronously);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           ApplyUpdateFailsWithInvalidResponseType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           ApplyUpdateRemovalsFailsWithInvalidCompressionType);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, VerifyChecksumMmapFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, FailedMmapOnRead);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, MigrateToMmap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, MigrateFileOffsets);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, CleanUpOldFiles);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, FileSizeIncludesHashFiles);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, MergeUpdatesWithHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StorePerftest, StressTest);
  FRIEND_TEST_ALL_PREFIXES(V4StorePerftest, VerifyChecksumFast);
  FRIEND_TEST_ALL_PREFIXES(V4StorePerftest, MergeUpdateFast);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, PreMmapMigrationFileFormatFails);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationAlreadyV4);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationDisabled);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationNotEligible_WipeSucceeds);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationNotEligible_WipeFails);
  FRIEND_TEST_ALL_PREFIXES(
      V4StoreTest,
      TestMigrationNotEligible_WipeHashFileFails_WipeStoreFileSucceeds);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationV5NotFound);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationSuccess);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationSuccessNoHashFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationPrefixSizeMismatch);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationHashFileMissing);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureRename);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureWrite);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureRenameV4Store);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationSuccessButReadFailure);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureInvalidV5);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureOpenFailureV5);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureEmptyV5);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureCorruptedV5);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMigrationFailureIncompatibleVersionV5);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationFailureMissingDetailsV5);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest,
                           TestMigrationInterruptedWipesEverything);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMigrationLogsResult);

  friend class V4StoreTest;
  friend class V4StoreFuzzer;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(ApplyUpdateType)
  enum class ApplyUpdateType {
    kInvalid = 0,
    kFull = 1,
    kPartial = 2,
    kMaxValue = kPartial,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV4UpdateType)

  // If |prefix_size| is within expected range, and |raw_hashes_length| is a
  // multiple of prefix_size, then it sets the string of length
  // |raw_hashes_length| starting at |raw_hashes_begin| as the value at key
  // |prefix_size| in |additions_map|
  static ApplyUpdateResult AddUnlumpedHashes(
      PrefixSize prefix_size,
      const char* raw_hashes_begin,
      const size_t raw_hashes_length,
      std::unordered_map<PrefixSize, HashPrefixes>* additions_map);

  // An overloaded version of AddUnlumpedHashes that allows passing in a
  // std::string object.
  static ApplyUpdateResult AddUnlumpedHashes(
      PrefixSize prefix_size,
      const std::string& raw_hashes,
      std::unordered_map<PrefixSize, HashPrefixes>* additions_map);

  // Get the next unmerged hash prefix in dictionary order from
  // |hash_prefix_map|. |iterator_map| is used to determine which hash prefixes
  // have been merged already. Returns true if there are any unmerged hash
  // prefixes in the list.
  static bool GetNextSmallestUnmergedPrefix(
      const HashPrefixMapView& hash_prefix_map,
      const IteratorMap& iterator_map,
      HashPrefixStr* smallest_hash_prefix);

  // For each key in |hash_prefix_map|, sets the iterator at that key
  // |iterator_map| to hash_prefix_map[key].begin().
  static void InitializeIteratorMap(const HashPrefixMapView& hash_prefix_map,
                                    IteratorMap* iterator_map);

  // Same as the public GetMatchingHashPrefix method, but takes a
  // std::string_view, for performance reasons.
  HashPrefixStr GetMatchingHashPrefix(std::string_view full_hash);

  // Merges the prefix map from the old store (|old_hash_prefix_map|) and the
  // update (additions_map) to populate the prefix map for the current store.
  // The indices in the |raw_removals| list, which may be NULL, are not merged.
  // The SHA256 checksum of the final list of hash prefixes, in
  // lexicographically sorted order, must match |expected_checksum| (if it's not
  // empty).
  ApplyUpdateResult MergeUpdate(
      const HashPrefixMapView& old_hash_prefix_map,
      const HashPrefixMapView& additions_map,
      const ::google::protobuf::RepeatedField<int32_t>* raw_removals,
      const std::string& expected_checksum);

  // Fast path for MergeUpdate when both maps have exactly one prefix size.
  ApplyUpdateResult MergeUpdateFast(
      const HashPrefixMapView& old_hash_prefix_map,
      const HashPrefixMapView& additions_map,
      const ::google::protobuf::RepeatedField<int32_t>* raw_removals,
      const std::string& expected_checksum);

  // Fast path for VerifyChecksum when the map has exactly one prefix size.
  bool VerifyChecksumFast(const HashPrefixMapView& hash_prefix_map);

  // Processes the FULL_UPDATE |response| from the server, and writes the
  // merged V4Store to disk. If processing the |response| succeeds, it returns
  // APPLY_UPDATE_SUCCESS. The UMA metrics for all interesting sub-operations
  // use the prefix |metric|.
  // This method is only called when we receive a FULL_UPDATE from the server.
  ApplyUpdateResult ProcessFullUpdateAndWriteToDisk(
      const std::string& metric,
      std::unique_ptr<ListUpdateResponse> response);

  // Processes a FULL_UPDATE |response| and updates the V4Store. If processing
  // the |response| succeeds, it returns APPLY_UPDATE_SUCCESS.
  // This method is called when we receive a FULL_UPDATE from the server, and
  // when we read a store file from disk on startup. The UMA metrics for all
  // interesting sub-operations use the prefix |metric|. Delays the checksum
  // check if |delay_checksum_check| is true.
  ApplyUpdateResult ProcessFullUpdate(
      const std::string& metric,
      const std::unique_ptr<ListUpdateResponse>& response,
      bool delay_checksum_check);

  // Merges the hash prefixes in |hash_prefix_map_old| and |response|, updates
  // the |hash_prefix_map_| and |state_| in the V4Store, and writes the merged
  // store to disk. If processing succeeds, it returns APPLY_UPDATE_SUCCESS.
  // This method is only called when we receive a PARTIAL_UPDATE from the
  // server. The UMA metrics for all interesting sub-operations use the prefix
  // |metric|.
  ApplyUpdateResult ProcessPartialUpdateAndWriteToDisk(
      const std::string& metric,
      const HashPrefixMapView& hash_prefix_map_old,
      std::unique_ptr<ListUpdateResponse> response);

  // Merges the hash prefixes in |hash_prefix_map_old| and |response|, and
  // updates the |hash_prefix_map_| and |state_| in the V4Store. If processing
  // succeeds, it returns APPLY_UPDATE_SUCCESS. The UMA metrics for all
  // interesting sub-operations use the prefix |metric|. Delays the checksum
  // check if |delay_checksum_check| is true.
  ApplyUpdateResult ProcessUpdate(
      const std::string& metric,
      const HashPrefixMapView& hash_prefix_map_old,
      const std::unique_ptr<ListUpdateResponse>& response,
      bool delay_checksum_check);

  // Reads the state of the store from the file on disk and returns the reason
  // for the failure or reports success.
  StoreReadResult ReadFromDisk();

  // Reads the state of the store from the v4 file on disk directly. Returns the
  // reason for the failure or reports success.
  StoreReadResult ReadFromDiskInternal();

  // Attempts to migrate the store from v5 to v4 if needed. Returns the reason
  // for the failure or reports success.
  V5ToV4MigrationResult AttemptV5ToV4Migration();

  // Performs the actual migration steps from the v5 store to v4.
  // |v5_store_path| is the path to the V5 store file to migrate.
  // Returns the reason for the failure or reports success.
  V5ToV4MigrationResult MigrateFromV5(const base::FilePath& v5_store_path);

  // Converts the extensions blocklist from v5 hash file format to v4 ID file
  // format.
  // |v5_hash_file_path| is the path to the source v5 hash file
  // containing 16-byte hashes.
  // |v4_hash_file_path| is the path where the
  // converted 32-byte hex IDs should be written.
  // |checksum_sha256| is an in-out parameter. On input, it contains the
  // expected V5 checksum (if any) used to verify the V5 data before conversion.
  // On successful conversion, it is overwritten in-place with the newly
  // calculated V4 checksum of the converted data. Can be nullptr if no checksum
  // is expected.
  // |file_size| will be populated with the size of the converted file in
  // bytes. Returns the granular result of the conversion attempt.
  ConvertExtensionBlocklistV5ToV4Result ConvertExtensionsBlocklistFromV5ToV4(
      const base::FilePath& v5_hash_file_path,
      const base::FilePath& v4_hash_file_path,
      std::string* checksum_sha256,
      uint64_t* file_size);

  // Wipes the V5 store file and its associated hash files.
  // |v5_store_path| is the path of the V5 store to delete.
  // Returns true if both the store file and all of its associated hash files
  // are successfully deleted; false otherwise.
  bool WipeV5Store(const base::FilePath& v5_store_path);

  // Updates the |additions_map| with the additions received in the partial
  // update from the server. The UMA metrics for all interesting sub-operations
  // use the prefix |metric|.
  ApplyUpdateResult UpdateHashPrefixMapFromAdditions(
      const std::string& metric,
      const ::google::protobuf::RepeatedPtrField<ThreatEntrySet>& additions,
      std::unordered_map<PrefixSize, HashPrefixes>* additions_map);

  // Writes the hash_prefix_map_ to disk as a V4StoreFileFormat proto.
  // |checksum| is used to set the |checksum| field in the final proto.
  StoreWriteResult WriteToDisk(const Checksum& checksum);

  // Same as above but uses a pre-populated |file_format|.
  StoreWriteResult WriteToDisk(V4StoreFileFormat* file_format);

  static void RecordDecodeAdditionsResult(const std::string& base_metric,
                                          V4DecodeResult result,
                                          const base::FilePath& file_path);

  static void RecordDecodeRemovalsResult(const std::string& base_metric,
                                         V4DecodeResult result,
                                         const base::FilePath& file_path);

  // Records the status of the update being applied to the database.
  ApplyUpdateResult last_apply_update_result_ = APPLY_UPDATE_RESULT_MAX;

  // The checksum value as read from the disk, until it is verified. Once
  // verified, it is cleared.
  std::string expected_checksum_;



  // Records the number of times we have looked up the store.
  size_t checks_attempted_ = 0;

  // The expected prefix size for the hash prefixes in V5 store.
  const PrefixSize v5_prefix_size_ = 0;

  // Whether this store is eligible for v5 to v4 disk migration.
  const bool is_eligible_for_migration_ = true;

  // Whether this store is for the extensions blocklist.
  const bool is_extensions_blocklist_ = false;

  // The state of the store as returned by the PVer4 server in the last applied
  // update response.
  std::string state_;
};

std::ostream& operator<<(std::ostream& os, const V4Store& store);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_STORE_H_
