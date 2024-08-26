// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/files/memory_mapped_file.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace safe_browsing {

// Enumerate different events while applying the update fetched fom the server
// for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES.
enum ApplyUpdateResult {
  // No errors.
  APPLY_UPDATE_SUCCESS = 0,

  // Reserved for errors in parsing this enum.
  UNEXPECTED_APPLY_UPDATE_FAILURE = 1,

  // Prefix size smaller than 4 (which is the lowest expected).
  PREFIX_SIZE_TOO_SMALL_FAILURE = 2,

  // Prefix size larger than 32 (length of a full SHA256 hash).
  PREFIX_SIZE_TOO_LARGE_FAILURE = 3,

  // The number of bytes in additions isn't a multiple of prefix size.
  ADDITIONS_SIZE_UNEXPECTED_FAILURE = 4,

  // The update received from the server contains a prefix that's already
  // present in the map.
  ADDITIONS_HAS_EXISTING_PREFIX_FAILURE = 5,

  // The server sent a response_type that the client did not expect.
  UNEXPECTED_RESPONSE_TYPE_FAILURE = 6,

  // One of more index(es) in removals field of the response is greater than
  // the number of hash prefixes currently in the (old) store.
  REMOVALS_INDEX_TOO_LARGE_FAILURE = 7,

  // Failed to decode the Rice-encoded additions/removals field.
  RICE_DECODING_FAILURE = 8,

  // Compression type other than RAW and RICE for additions.
  UNEXPECTED_COMPRESSION_TYPE_ADDITIONS_FAILURE = 9,

  // Compression type other than RAW and RICE for removals.
  UNEXPECTED_COMPRESSION_TYPE_REMOVALS_FAILURE = 10,

  // The state of the store did not match the expected checksum sent by the
  // server.
  CHECKSUM_MISMATCH_FAILURE = 11,

  // There was a failure trying to map the file.
  MMAP_FAILURE = 12,

  // The hash prefixes were not sorted when reading from dis.
  READ_FAILURE_NOT_SORTED = 13,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  APPLY_UPDATE_RESULT_MAX
};

// The sorted list of hash prefixes.
using HashPrefixes = std::string;

using HashPrefixesView = std::string_view;
using HashPrefixMapView = std::unordered_map<PrefixSize, HashPrefixesView>;

// Set a common sense limit on the store file size we try to read.
// The maximum store file size, as of today, is about 6MB.
constexpr size_t kMaxStoreSizeBytes = 50 * 1000 * 1000;

// Stores the list of sorted hash prefixes, by size.
// For instance: {4: ["abcd", "bcde", "cdef", "gggg"], 5: ["fffff"]}
// Maps will be stored a separate file for hash prefix lists of each
// prefix size. These will be mapped into memory on initialization.
class HashPrefixMap {
 public:
  explicit HashPrefixMap(
      const base::FilePath& store_path,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr,
      size_t buffer_size = 1024 * 512);

  virtual ~HashPrefixMap();

  // Clears the underlying map.
  void Clear();

  // Returns a read-only view of the data stored in this map.
  HashPrefixMapView view() const;

  // Returns the prefix at `size`.
  HashPrefixesView at(PrefixSize size) const;

  // Appends |prefix| to the prefix list of size |size|.
  void Append(PrefixSize size, HashPrefixesView prefix);

  // Reserves space for the prefix list of size |size|.
  virtual void Reserve(PrefixSize size, size_t capacity);

  // Reads the map from disk.
  ApplyUpdateResult ReadFromDisk(const V4StoreFileFormat& file_format);

  class WriteSession {
   public:
    WriteSession(const WriteSession&) = delete;
    WriteSession& operator=(const WriteSession&) = delete;
    virtual ~WriteSession() = default;

   protected:
    WriteSession() = default;
  };

  // Write the map to disk. Returns null in case of error, or a session instance
  // that must be kept alive until `file_format` is committed to disk.
  // Implementations may lend some internal state to `file_format` so that it
  // can be written to disk with minimal overhead.
  std::unique_ptr<WriteSession> WriteToDisk(V4StoreFileFormat* file_format);

  // Returns true if the data in this map is valid and can be used.
  ApplyUpdateResult IsValid() const;

  // Returns a hash prefix if it matches the prefixes stored in this map.
  HashPrefixStr GetMatchingHashPrefix(std::string_view full_hash);

  // Migrates the file format between the different types of HashPrefixMap.
  enum class MigrateResult { kUnknown, kSuccess, kFailure, kNotNeeded };
  MigrateResult MigrateFileFormat(const base::FilePath& store_path,
                                  V4StoreFileFormat* file_format);

  // Collects debug information about the prefixes in the map.
  void GetPrefixInfo(google::protobuf::RepeatedPtrField<
                     DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
                         prefix_sets);

  static base::FilePath GetPath(const base::FilePath& store_path,
                                const std::string& extension);

  const std::string& GetExtensionForTesting(PrefixSize size);
  void ClearAndWaitForTesting();

 private:
  class BufferedFileWriter;
  class FileInfo {
   public:
    FileInfo(const base::FilePath& store_path, PrefixSize size);
    ~FileInfo();

    bool Initialize(const HashFile& hash_file);
    bool Finalize(HashFile* hash_file);

    HashPrefixesView GetView() const;
    bool IsReadable() const { return file_.IsValid(); }
    HashPrefixStr Matches(std::string_view full_hash) const;
    BufferedFileWriter* GetOrCreateWriter(size_t buffer_size);

    const std::string& GetExtensionForTesting() const;

   private:
    const base::FilePath store_path_;
    const PrefixSize prefix_size_;

    base::MemoryMappedFile file_;
    std::unique_ptr<BufferedFileWriter> writer_;
    std::vector<uint32_t> offsets_;
  };

  FileInfo& GetFileInfo(PrefixSize size);
  void ClearOnTaskRunner();

  base::FilePath store_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unordered_map<PrefixSize, FileInfo> map_;
  size_t buffer_size_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
