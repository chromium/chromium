// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

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

  // One or more indices in removals field of the response is negative.
  REMOVALS_INDEX_NEGATIVE_FAILURE = 14,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  APPLY_UPDATE_RESULT_MAX
};

// Set a common sense limit on the store file size we try to read.
// The maximum store file size, as of today, is about 6MB.
constexpr size_t kMaxStoreSizeBytes = 50 * 1000 * 1000;

// Stores the list of sorted hash prefixes, by size.
// For instance: {4: ["abcd", "bcde", "cdef", "gggg"], 5: ["fffff"]}
// Maps will be stored a separate file for hash prefix lists of each
// prefix size. These will be mapped into memory on initialization.
class HashPrefixMap : public HashPrefixContainer {
 public:
  explicit HashPrefixMap(
      const base::FilePath& store_path,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr,
      size_t buffer_size = kDefaultBufferSize);

  ~HashPrefixMap() override;

  // HashPrefixContainer:
  void Clear() override;

  HashPrefixMapView view() const override;

  void Append(PrefixSize size, HashPrefixesView prefix) override;

  // Reads the map from disk.
  ApplyUpdateResult ReadFromDisk(const SBStoreFileFormat& file_format);

  std::unique_ptr<HashPrefixContainer::WriteSession> WriteToDisk(
      SBStoreFileFormat& file_format) override;

  // Returns true if the data in this map is valid and can be used.
  ApplyUpdateResult IsValid() const;

  HashPrefixStr GetMatchingHashPrefix(std::string_view full_hash) override;

  void GetPrefixInfo(google::protobuf::RepeatedPtrField<
                     DatabaseManagerInfo::DatabaseInfo::StoreInfo::PrefixSet>*
                         prefix_sets) override;

  std::vector<base::FilePath> GetPaths() const override;

  const std::string& GetExtensionForTesting(PrefixSize size);
  void ClearAndWaitForTesting();

 private:
  FileInfo& GetFileInfo(PrefixSize size);
  void ClearOnTaskRunner();

  std::unordered_map<PrefixSize, FileInfo> map_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_HASH_PREFIX_MAP_H_
