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
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace safe_browsing {

class V4Store;

struct V4StoreDeleter;
using V4StorePtr = std::unique_ptr<V4Store, V4StoreDeleter>;

using UpdatedStoreReadyCallback =
    base::OnceCallback<void(V4StorePtr new_store)>;

// Stores the iterator to the last element merged from the HashPrefixMap for a
// given prefix size.
// For instance: {4:iter(3), 5:iter(1)} means that we have already merged
// 3 hash prefixes of length 4, and 1 hash prefix of length 5.
using IteratorMap =
    std::unordered_map<PrefixSize, HashPrefixesView::const_iterator>;

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes.  DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum StoreReadResult {
  // No errors.
  READ_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_READ_FAILURE = 1,

  // The contents of the file could not be read.
  FILE_UNREADABLE_FAILURE = 2,

  // The file was found to be empty.
  FILE_EMPTY_FAILURE = 3,

  // The contents of the file could not be interpreted as a valid
  // V4StoreFileFormat proto.
  PROTO_PARSING_FAILURE = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash prefixes.
  UNEXPECTED_MAGIC_NUMBER_FAILURE = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  FILE_VERSION_INCOMPATIBLE_FAILURE = 6,

  // The rest of the file could not be parsed as a ListUpdateResponse protobuf.
  // This can happen if the machine crashed before the file was fully written to
  // disk or if there was disk corruption.
  HASH_PREFIX_INFO_MISSING_FAILURE = 7,

  // Unable to generate the hash prefix map from the updates on disk.
  HASH_PREFIX_MAP_GENERATION_FAILURE = 8,

  // There was a failure migrating between in-memory and mmap file formats.
  MIGRATION_FAILURE = 9,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_READ_RESULT_MAX
};

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

// Factory for creating V4Store. Tests implement this factory to create fake
// stores for testing.
class V4StoreFactory {
 public:
  virtual ~V4StoreFactory() {}

  virtual V4StorePtr CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path);
};

class V4Store {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // If the store is being created to apply an update to the old store, then
  // |old_file_size| is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          int64_t old_file_size = 0);
  virtual ~V4Store();

  // If a hash prefix in this store matches |full_hash|, returns that hash
  // prefix; otherwise returns an empty hash prefix.
  virtual HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash);

  // True if this store has valid contents, either from a successful read
  // from disk or a full update.  This does not mean the checksum was verified.
  virtual bool HasValidData();

  const std::string& state() const { return state_; }

  const base::FilePath& store_path() const { return store_path_; }

  int64_t file_size() const { return file_size_; }

  void ApplyUpdate(std::unique_ptr<ListUpdateResponse> response,
                   const scoped_refptr<base::SequencedTaskRunner>& runner,
                   UpdatedStoreReadyCallback callback);

  // Records (in kilobytes) and returns the size of the file on disk for this
  // store using |base_metric| as prefix and the filename as suffix.
  int64_t RecordAndReturnFileSize(const std::string& base_metric);

  std::string DebugString() const;

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

  // Reset internal state.
  void Reset();

  // Scheduled after reading the store file from disk on startup. When run, it
  // ensures that the checksum of the hash prefixes in lexicographical sorted
  // order matches the expected value in |expected_checksum_|. Returns true if
  // it matches; false otherwise. Checksum verification can take a long time,
  // so it is performed outside of the hotpath of loading SafeBrowsing database,
  // which blocks resource loads.
  bool VerifyChecksum();

  // Populates the DatabaseInfo message.
  void CollectStoreInfo(
      DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info,
      const std::string& base_metric);

  HashPrefixMap::MigrateResult migrate_result() const {
    return migrate_result_;
  }

 protected:
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
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestAdditionsWithRiceEncodingSucceeds);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestRemovalsWithRiceEncodingSucceeds);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestMergeUpdatesFailsChecksum);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, TestChecksumErrorOnStartup);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, WriteToDiskFails);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, FullUpdateFailsChecksumSynchronously);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, VerifyChecksumMmapFile);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, FailedMmapOnRead);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, MigrateToMmap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, MigrateFileOffsets);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, CleanUpOldFiles);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, FileSizeIncludesHashFiles);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, ReserveSpaceInPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StoreTest, MergeUpdatesWithHashPrefixMap);
  FRIEND_TEST_ALL_PREFIXES(V4StorePerftest, StressTest);

  friend class V4StoreTest;
  friend class V4StoreFuzzer;

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

  // Reserve the appropriate string size so that the string size of the merged
  // list is exact. This ignores the space that would otherwise be released by
  // deletions specified in the update because it is non-trivial to calculate
  // those deletions upfront. This isn't so bad since deletions are supposed to
  // be small and infrequent.
  static void ReserveSpaceInPrefixMap(const HashPrefixMapView& old_map,
                                      const HashPrefixMapView& additions_map,
                                      size_t removals_count,
                                      HashPrefixMap* prefix_map_to_update);

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
      const ::google::protobuf::RepeatedField<::google::protobuf::int32>*
          raw_removals,
      const std::string& expected_checksum);

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

  // Migrates between in-memory and on-disk file formats.
  HashPrefixMap::MigrateResult MigrateFileFormatIfNeeded(
      V4StoreFileFormat* file_format);

  // Records the status of the update being applied to the database.
  ApplyUpdateResult last_apply_update_result_ = APPLY_UPDATE_RESULT_MAX;

  // Records the time when the store was last updated.
  base::Time last_apply_update_time_millis_;

  // The checksum value as read from the disk, until it is verified. Once
  // verified, it is cleared.
  std::string expected_checksum_;

  // The size of the file on disk for this store.
  int64_t file_size_;

  // A counter used to manage how frequently the value of `has_valid_data_`
  // below is recorded.
  uint8_t record_has_valid_data_counter_ = 0;

  // True if the file was successfully read+parsed or was populated from
  // a full update.
  bool has_valid_data_;

  // Records the number of times we have looked up the store.
  size_t checks_attempted_ = 0;

  // The state of the store as returned by the PVer4 server in the last applied
  // update response.
  std::string state_;
  const base::FilePath store_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HashPrefixMap::MigrateResult migrate_result_ =
      HashPrefixMap::MigrateResult::kUnknown;
};

std::ostream& operator<<(std::ostream& os, const V4Store& store);

struct V4StoreDeleter {
  explicit V4StoreDeleter(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~V4StoreDeleter();

  V4StoreDeleter(V4StoreDeleter&&);
  V4StoreDeleter& operator=(V4StoreDeleter&&);

  void operator()(const V4Store* ptr) {
    if (ptr) {
      if (task_runner_->RunsTasksInCurrentSequence()) {
        delete ptr;
      } else {
        task_runner_->DeleteSoon(FROM_HERE, ptr);
      }
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_STORE_H_
