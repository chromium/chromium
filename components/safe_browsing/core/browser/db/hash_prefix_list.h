// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_LIST_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_LIST_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"

namespace safe_browsing {

// Enumerate different events while applying the update fetched from the server
// for V5 HashPrefixList. These values are persisted to logs. Entries should not
// be renumbered and numeric values should never be reused.

// LINT.IfChange(V5ApplyUpdateResult)
enum class V5ApplyUpdateResult {
  // Unknown or uninitialized state.
  kUnknown = 0,

  // The operation completed successfully.
  kSuccess = 1,

  // Failed to memory-map the file or initialize the FileInfo.
  kMmapFailure = 2,

  // The file size is not a multiple of the expected prefix size.
  kFileSizeNotMultipleOfPrefixSize = 3,

  // The expected checksum did not match the actual hash file's checksum.
  kChecksumMismatchFailure = 4,

  // Failed to decode the Rice-encoded additions.
  kRiceDecodingAdditionsFailure = 5,

  // Failed to decode the Rice-encoded removals.
  kRiceDecodingRemovalsFailure = 6,

  // Merging additions failed because a prefix already existed.
  kAdditionsHasExistingPrefixFailure = 7,

  // Merging removals failed because an index was out of bounds.
  kRemovalsIndexTooLargeFailure = 8,

  // Failed to write the store file or hash file to disk.
  kWriteFailure = 9,

  kMaxValue = kWriteFailure,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5ApplyUpdateResult)

inline std::ostream& operator<<(std::ostream& os, V5ApplyUpdateResult result) {
  return os << static_cast<int>(result);
}

// Stores the list of sorted hash prefixes, e.g.: ["abcd","bcde","cdef","gggg"]
// The prefixes are stored in a separate file which is mapped into memory on
// initialization.
class HashPrefixList : public HashPrefixContainer {
 public:
  explicit HashPrefixList(
      const base::FilePath& store_path,
      PrefixSize prefix_size,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr,
      size_t buffer_size = kDefaultBufferSize);

  ~HashPrefixList() override;

  // HashPrefixContainer:
  void Clear() override;
  HashPrefixMapView view() const override;
  void Append(PrefixSize size, HashPrefixesView prefix) override;
  V5ApplyUpdateResult ReadFromDisk(const SBStoreFileFormat& file_format);
  std::unique_ptr<HashPrefixContainer::WriteSession> WriteToDisk(
      SBStoreFileFormat& file_format) override;
  HashPrefixStr GetMatchingHashPrefix(std::string_view full_hash) override;
  void GetPrefixInfo(google::protobuf::RepeatedPtrField<
                     DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
                         prefix_sets) override;

  // Returns a flat string view of all the hash prefixes in this list.
  // The prefixes are concatenated in sorted order.
  HashPrefixesView GetRawView() const;

  // Returns whether all underlying mmap-backed files are readable.
  V5ApplyUpdateResult IsValid() const;

  // Test-only method to fetch the file extension.
  const std::string& GetExtensionForTesting();

 private:
  // Gets or creates a wrapper around the file.
  FileInfo& GetFileInfo();
  // Resets the file info. Must be run on the task runner sequence.
  void ClearOnTaskRunner();

  // The size of the prefixes for this particular list.
  const PrefixSize prefix_size_;
  // Contains the mmap-ed file and how to interact with it.
  std::unique_ptr<FileInfo> file_info_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_LIST_H_
