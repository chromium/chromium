// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_CONTAINER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_CONTAINER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/sb_store_file_format.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"

namespace safe_browsing {

// TODO(crbug.com/372395685): Delete hash prefix container files and move into
// hash prefix list files.
// The sorted list of hash prefixes.
using HashPrefixes = std::string;

using HashPrefixesView = std::string_view;
using HashPrefixMapView = std::unordered_map<PrefixSize, HashPrefixesView>;

class HashPrefixContainer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(WriteError)
  enum class WriteError {
    kFileWriteError = 0,
    kInvalidTotalSize = 1,
    kFileNotFound = 2,
    kFileSizeMismatch = 3,
    kFailedMmap = 4,
    kMmapSizeMismatch = 5,
    kMaxValue = kMmapSizeMismatch,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SBHashPrefixWriteError)

  // Metadata for a hash prefix file that has been written out.
  struct FinalizedFileInfo {
    // The extension generated for the written file.
    std::string extension;
    // The total size in bytes of the written file.
    uint64_t file_size;
  };

  // Represents an active disk write session. Instances returned by
  // WriteToDisk() must be kept alive until the corresponding
  // SBStoreFileFormat is fully committed to disk.
  class WriteSession {
   public:
    WriteSession(const WriteSession&) = delete;
    WriteSession& operator=(const WriteSession&) = delete;
    virtual ~WriteSession() = default;

   protected:
    WriteSession() = default;
  };

  // Buffers disk writes to avoid issuing a file write call for each hash
  // prefix.
  class BufferedFileWriter {
   public:
    BufferedFileWriter(const base::FilePath& store_path,
                       PrefixSize prefix_size,
                       size_t buffer_size,
                       const std::string& extension,
                       std::string_view metric_prefix);

    ~BufferedFileWriter();

    void Write(HashPrefixesView data);
    bool Finish();

    size_t GetFileSize() const;

    const std::string& extension() const;

    bool has_error() const;

   private:
    void Flush();
    void WriteToFile(HashPrefixesView data);

    const std::string extension_;
    const base::FilePath path_;
    const size_t prefix_size_;
    const size_t buffer_size_;
    size_t cur_size_ = 0;
    base::File file_;
    std::string buffer_;
    bool has_error_;
    const std::string metric_prefix_;
  };

  // Default buffer size for writing hash prefix files.
  static constexpr size_t kDefaultBufferSize = 1024 * 512;

  // Manages the memory-mapped file for a specific prefix size and handles
  // writing out updates.
  class FileInfo {
   public:
    FileInfo(const base::FilePath& store_path, PrefixSize size);
    ~FileInfo();

    // `initialize_after_write` will control some extra logging for
    // investigating https://crbug.com/393395944.
    // TODO(crbug.com/393395944): Remove `initialize_after_write`.
    bool Initialize(const std::string& extension,
                    uint64_t expected_file_size,
                    bool initialize_after_write,
                    std::string_view metric_prefix);

    std::optional<FinalizedFileInfo> Finalize(std::string_view metric_prefix);

    HashPrefixesView GetView() const;
    bool IsReadable() const;
    HashPrefixStr Matches(std::string_view full_hash) const;
    BufferedFileWriter* GetOrCreateWriter(size_t buffer_size,
                                          const std::string& extension,
                                          std::string_view metric_prefix);

    const std::string& GetExtensionForTesting() const;

    // Returns the file path for this FileInfo.
    base::FilePath GetPath() const;

   private:
    const base::FilePath store_path_;
    const PrefixSize prefix_size_;
    std::string extension_;

    base::MemoryMappedFile file_;
    std::unique_ptr<BufferedFileWriter> writer_;
  };

  explicit HashPrefixContainer(
      const base::FilePath& store_path,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr,
      size_t buffer_size = kDefaultBufferSize);

  virtual ~HashPrefixContainer();

  // Clears the underlying container.
  virtual void Clear() = 0;

  // Returns a read-only view of the data stored in this container.
  // TODO(crbug.com/372395685): There shouldn't be any map references once
  // HashPrefixList is all that remains.
  virtual HashPrefixMapView view() const = 0;

  // Appends |prefix| to the prefix list of size |size|.
  virtual void Append(PrefixSize size, HashPrefixesView prefix) = 0;

  // Write the container to disk. Returns null in case of error, or a session
  // instance that must be kept alive until `file_format` is committed to disk.
  virtual std::unique_ptr<WriteSession> WriteToDisk(
      SBStoreFileFormat& file_format) = 0;

  // Returns a hash prefix if it matches the prefixes stored in this container.
  virtual HashPrefixStr GetMatchingHashPrefix(std::string_view full_hash) = 0;

  // Collects debug information about the prefixes in the container.
  virtual void GetPrefixInfo(
      google::protobuf::RepeatedPtrField<
          DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
          prefix_sets) = 0;

  // Returns all active hash prefix file paths currently managed by this
  // container.
  // TODO(crbug.com/372395685): This just returns a single item once v4 is
  // deprecated, so stop returning a vector.
  virtual std::vector<base::FilePath> GetPaths() const = 0;

  // Generates the file path for a hash prefix file.
  // `store_path`: The base path of the store file.
  // `extension`: The file extension to append.
  static base::FilePath GetPath(const base::FilePath& store_path,
                                const std::string& extension);

 protected:
  base::FilePath store_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  size_t buffer_size_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_CONTAINER_H_
