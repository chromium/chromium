// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_

#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
class V5StoreFileFormat;

// TODO(crbug.com/362791941): replace all |comments| with `comments`.
namespace safe_browsing {

namespace V5 {
class HashList;
}

class V4StoreFileFormat;
class SBStore;
class ListInfo;
class HashPrefixContainer;

struct SBStoreDeleter;
using SBStorePtr = std::unique_ptr<SBStore, SBStoreDeleter>;

// Enumerate different events while applying the update fetched from the server
// for logging purposes.
enum class SBStoreUpdateResult {
  // No errors.
  kSuccess = 0,

  // The update received from the server contains a prefix that's already
  // present in the store.
  kAdditionsHasExistingPrefixFailure = 1,

  // One of more index(es) in removals field of the response is greater than
  // the number of hash prefixes currently in the (old) store.
  kRemovalsIndexTooLargeFailure = 2,

  // The state of the store did not match the expected checksum sent by the
  // server.
  kChecksumMismatchFailure = 3,
};

// Enumerate different failure events while writing the file to disk.
enum class SBStoreWriteResult {
  // An unexpected error occurred while writing the file.
  kUnexpectedWriteFailure = 0,

  // Number of bytes written to disk was different from the size of the proto.
  kUnexpectedBytesWrittenFailure = 1,

  // Renaming the temporary file to store file failed.
  kUnableToRenameFailure = 2,
};

// A ZeroCopyOutputStream that writes to a file using base::File. Any errors
// during serialization close the file.
class BaseFileOutputStream
    : public google::protobuf::io::CopyingOutputStreamAdaptor {
 public:
  // Creates and opens `output_file`, overwriting any previous contents.
  explicit BaseFileOutputStream(const base::FilePath& output_file);
  BaseFileOutputStream(const BaseFileOutputStream&) = delete;
  BaseFileOutputStream& operator=(const BaseFileOutputStream&) = delete;

  // Closes the file, if it was still open.
  ~BaseFileOutputStream() override;

  // Returns `base::File::FILE_OK` if no error and the file is still open; else
  // the error that led to closure of the file.
  base::File::Error GetError() const;

 private:
  class CopyingBaseFileOutputStream
      : public google::protobuf::io::CopyingOutputStream {
   public:
    explicit CopyingBaseFileOutputStream(const base::FilePath& output_file);
    CopyingBaseFileOutputStream(const CopyingBaseFileOutputStream&) = delete;
    CopyingBaseFileOutputStream& operator=(const CopyingBaseFileOutputStream&) =
        delete;
    ~CopyingBaseFileOutputStream() override;

    base::File::Error GetError() const;

    // google::protobuf::io::CopyingOutputStream:
    bool Write(const void* buffer, int size) override;

   private:
    base::File file_;
  };

  CopyingBaseFileOutputStream stream_;
};

class SBStoreFactory {
 public:
  virtual ~SBStoreFactory() = default;
  virtual SBStorePtr CreateStore(
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::FilePath& base_path,
      const ListInfo& list_info) = 0;
};

// This will have either a `v4_response` or a `v5_response`. Having a shared
// wrapper struct allows for more code reuse during the v4 -> v5 transition.
struct SBUpdateResponse {
  SBUpdateResponse();
  ~SBUpdateResponse();

  std::unique_ptr<ListUpdateResponse> v4_response;
  std::unique_ptr<V5::HashList> v5_response;
};

using SBUpdateResponseMap =
    std::unordered_map<ListIdentifier, std::unique_ptr<SBUpdateResponse>>;
using UpdatedStoreReadyCallback =
    base::OnceCallback<void(SBStorePtr new_store)>;

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

  // The file is in a pre-mmap migration format, which is no longer supported.
  PRE_MMAP_MIGRATION_FILE_FORMAT_FAILURE = 10,

  // Failed to migrate from v5 to v4.
  V5_TO_V4_MIGRATION_FAILURE = 11,

  // V5 to V4 migration was ineligible, and wiping V5 succeeded.
  V5_TO_V4_MIGRATION_WIPED_SUCCESSFULLY = 12,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  STORE_READ_RESULT_MAX
};

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
// LINT.IfChange(V5StoreReadResult)
enum class V5StoreReadResult {
  // No errors.
  kReadSuccess = 0,

  // Reserved for errors in parsing this enum.
  kUnexpectedReadFailure = 1,

  // The store file could not be opened (e.g. missing, access denied).
  kFileOpenFailure = 2,

  // The file was found to be empty.
  kFileEmptyFailure = 3,

  // The contents of the file could not be interpreted as a valid
  // V5StoreFileFormat proto.
  kProtoParsingFailure = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash prefixes.
  kUnexpectedMagicNumberFailure = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  kFileVersionIncompatibleFailure = 6,

  // The rest of the file could not be parsed.
  kHashPrefixInfoMissingFailure = 7,

  // Unable to generate the hash prefix list from the updates on disk.
  kHashPrefixListGenerationFailure = 8,

  // A read error occurred while parsing the file.
  kFileReadFailure = 9,

  // Failed to migrate from v4 to v5.
  kV4ToV5MigrationFailure = 10,

  // Migration was needed but the store was ineligible, and wiping V4 succeeded.
  kV4ToV5MigrationWipedSuccessfully = 11,

  kMaxValue = kV4ToV5MigrationWipedSuccessfully
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5StoreReadResult)

// A ZeroCopyInputStream that reads from a file using base::File. Any errors
// during deserialization close the file.
class BaseFileInputStream : public google::protobuf::io::ZeroCopyInputStream {
 public:
  // Creates and opens `input_file`.
  explicit BaseFileInputStream(const base::FilePath& input_file);
  BaseFileInputStream(const BaseFileInputStream&) = delete;
  BaseFileInputStream& operator=(const BaseFileInputStream&) = delete;

  // Closes the file, if it was still open.
  ~BaseFileInputStream() override;

  // Returns `base::File::FILE_OK` if no error and the file is still open; else
  // the error that led to closure of the file.
  base::File::Error GetError() const;

  // google::protobuf::io::ZeroCopyInputStream:
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64_t ByteCount() const override;

 private:
  class CopyingBaseFileInputStream
      : public google::protobuf::io::CopyingInputStream {
   public:
    explicit CopyingBaseFileInputStream(const base::FilePath& input_file);
    CopyingBaseFileInputStream(const CopyingBaseFileInputStream&) = delete;
    CopyingBaseFileInputStream& operator=(const CopyingBaseFileInputStream&) =
        delete;
    ~CopyingBaseFileInputStream() override;

    base::File::Error GetError() const;

    // google::protobuf::io::CopyingInputStream:
    int Read(void* buffer, int size) override;
    int Skip(int count) override;

   private:
    base::File file_;
  };

  CopyingBaseFileInputStream stream_;
  google::protobuf::io::CopyingInputStreamAdaptor impl_;
};

// The base class for the Safe Browsing V4 and V5 stores.
class SBStore {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // If the store is being created to apply an update to the old store, then
  // |old_file_size| is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  SBStore(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          int64_t old_file_size = 0);
  virtual ~SBStore();

  // True if this store has valid contents, either from a successful read
  // from disk or a full update.  This does not mean the checksum was verified.
  virtual bool HasValidData();

  const base::FilePath& store_path() const { return store_path_; }

  int64_t file_size() const { return file_size_; }

  // Records (in kilobytes) and returns the size of the file on disk for this
  // store using |base_metric| as prefix and the filename as suffix.
  virtual int64_t RecordAndReturnFileSize(const std::string& base_metric) = 0;

  // Reset internal state.
  virtual void Reset() = 0;

  // TODO(crbug.com/362791941): All comments in sb_* files should use the modern
  // `code` format rather than the older |code| format.
  // Scheduled after reading the store file from disk on startup. When run, it
  // ensures that the checksum of the hash prefixes in lexicographical sorted
  // order matches the expected value in |expected_checksum_|. Returns true if
  // it matches; false otherwise. Checksum verification can take a long time,
  // so it is performed outside of the hotpath of loading SafeBrowsing database,
  // which blocks resource loads.
  virtual bool VerifyChecksum() = 0;

  // Populates the DatabaseInfo message.
  virtual void CollectStoreInfo(
      DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info);

  // Updates the SBStore with the response received from the SafeBrowsing
  // service. `response` contains the protocol-specific update payload. `runner`
  // is the task runner on which the callback should be run. `callback` is
  // scheduled once the update has been processed.
  virtual void ApplyUpdate(
      std::unique_ptr<SBUpdateResponse> response,
      const scoped_refptr<base::SequencedTaskRunner>& runner,
      UpdatedStoreReadyCallback callback) = 0;

  // If a hash prefix in this store matches `full_hash`, returns that hash
  // prefix; otherwise returns an empty hash prefix.
  virtual HashPrefixStr GetMatchingHashPrefix(const FullHashStr& full_hash) = 0;

  // Returns the state of the store (i.e. state for V4, version for V5).
  virtual const std::string& GetStoreState() const = 0;

 protected:
  // Converts a 32-character V4 extension ID string into its raw 16-byte V5
  // binary hash representation.
  // `v4_id` is the base-16 string extension ID to convert. Must be exactly 32
  // characters long.
  // Returns a string containing the raw 16 binary bytes.
  static std::string ExtensionV4IdToV5Hash(std::string_view v4_id);

  static constexpr uint32_t kFileMagic = 0x600D71FE;
  static constexpr uint32_t kV4FileVersion = 9;
  static constexpr uint32_t kV5FileVersion = 10;

  // Parses and validates a v4 store file format from disk.
  static StoreReadResult ParseAndValidateV4StoreFileFormat(
      const base::FilePath& store_path,
      V4StoreFileFormat& file_format,
      int64_t* file_size = nullptr);

  // Parses and validates a v5 store file format from disk.
  static V5StoreReadResult ParseAndValidateV5StoreFileFormat(
      const base::FilePath& store_path,
      V5StoreFileFormat& file_format,
      int64_t* file_size = nullptr);

  // Helper template method to write the store to disk.
  // It handles writing to a temporary file, committing the write session
  // for the `container`, renaming the file to the final destination, and
  // cleaning up on error.
  //  - `store_path`: The path where the store file should be written.
  //  - `file_format`: The protobuf representation of the file format (V4 or
  //    V5).
  //  - `container`: The container holding the hash prefixes (HashPrefixMap or
  //    HashPrefixList).
  //  - `set_file_metadata`: Callback to set file-specific metadata (e.g. magic
  //    number, version).
  //  - `cleanup_on_error`: Callback to perform cleanup (e.g. delete written
  //    hash files, clear container) if writing fails. Takes the temporary
  //    store file path as argument.
  //  - `get_hash_files_size`: Callback to calculate the total size of the hash
  //    files.
  //  - `cleanup_extra_files`: Callback to clean up any old/temporary files.
  // Returns the final size of the written file on success, or an
  // SBStoreWriteResult indicating the specific failure reason on failure.
  // TODO(crbug.com/372395685): Collapse + simplify this method into v5
  // implementation.
  template <typename FileFormat, typename Container>
  static base::expected<int64_t, SBStoreWriteResult> WriteToDiskLoop(
      const base::FilePath& store_path,
      FileFormat* file_format,
      Container* container,
      base::FunctionRef<void()> set_file_metadata,
      base::FunctionRef<void(const base::FilePath&)> cleanup_on_error,
      base::FunctionRef<int64_t()> get_hash_files_size,
      base::FunctionRef<void()> cleanup_extra_files);

  // Helper template method to merge additions and removals into
  // `out_container`. It performs a merge sort of `old_prefixes` and
  // `new_prefixes`, applying removals specified in `raw_removals`. It also
  // verifies the checksum of the merged prefixes if `expected_checksum` is
  // provided.
  //  - `prefix_size`: The size of each hash prefix.
  //  - `old_prefixes`: Span of existing sorted hash prefixes in the store.
  //  - `new_prefixes`: Span of new sorted hash prefixes to add.
  //  - `raw_removals`: Pointer to container of indices of prefixes to remove
  //    (from the old list).
  //  - `expected_checksum`: The expected SHA256 checksum of the merged
  //    prefixes.
  //  - `out_container`: The container where merged prefixes will be appended.
  // Returns SBStoreUpdateResult indicating success or specific failure reason.
  // TODO(crbug.com/372395685): Collapse + simplify this method into v5
  // implementation.
  template <typename RemovalsContainer>
  static SBStoreUpdateResult MergeUpdateLoop(
      PrefixSize prefix_size,
      base::span<const uint8_t> old_prefixes,
      base::span<const uint8_t> new_prefixes,
      const RemovalsContainer* raw_removals,
      const std::string& expected_checksum,
      HashPrefixContainer* out_container);

  // Returns the name of the temporary file used to buffer data for
  // `filename`.
  static const base::FilePath TemporaryFileForFilename(
      const base::FilePath& filename);

  virtual std::string GetMetricPrefix() const = 0;

  // The size of the file on disk for this store.
  int64_t file_size_;

  // True if the file was successfully read+parsed or was populated from
  // a full update.
  bool has_valid_data_;

  // Records the time when the store was last updated.
  base::Time last_apply_update_time_millis_;

  const base::FilePath store_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

 private:
  friend class V4StoreTest;
  friend class V5StoreTest;

  static void RecordBooleanWithAndWithoutSuffix(const std::string& metric,
                                                bool value,
                                                const std::string& suffix);

  void LogHasValidDataHistograms();

  // A counter used to manage how frequently the value of `has_valid_data_`
  // below is recorded.
  uint8_t record_has_valid_data_counter_ = 0;
};

struct SBStoreDeleter {
  explicit SBStoreDeleter(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SBStoreDeleter();

  SBStoreDeleter(SBStoreDeleter&&);
  SBStoreDeleter& operator=(SBStoreDeleter&&);

  void operator()(const SBStore* ptr) {
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

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_
