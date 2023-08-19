// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_UDIF_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_UDIF_H_

#include <CoreFoundation/CoreFoundation.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"

namespace safe_browsing {
namespace dmg {

class ReadStream;
class UDIFBlock;

// UDIFParser parses a Universal Disk Image Format file, allowing access to the
// name, types, and data of the partitions held within the file. There is no
// canonical documentation for UDIF, and not all disk images use UDIF (though
// the majority do). Note that this implementation only handles UDIF and SPARSE
// image types, not SPARSEBUNDLE.
//
// No guarantees are made about the position of ReadStream when using an
// instance of this class.
//
// Note that this implementation relies on the UDIF blkx table to provide
// accurate partition information. Otherwise, Apple Partition Map and
// GUID would need to be parsed. See:
//   - http://opensource.apple.com/source/IOStorageFamily/IOStorageFamily-182.1.1/IOApplePartitionScheme.h
//   - http://opensource.apple.com/source/IOStorageFamily/IOStorageFamily-182.1.1/IOGUIDPartitionScheme.h
//
// The following references are useful for understanding the UDIF format:
//   - http://newosxbook.com/DMG.html
//   - http://www.macdisk.com/dmgen.php
class UDIFParser {
 public:
  // Constructs an instance from a stream.
  explicit UDIFParser(ReadStream* stream);

  UDIFParser(const UDIFParser&) = delete;
  UDIFParser& operator=(const UDIFParser&) = delete;

  ~UDIFParser();

  // Parses the UDIF file. This method must be called before any other method.
  // If this returns false, it is not legal to call any other methods.
  bool Parse();

  // Returns the blob of DMG signature data.
  const std::vector<uint8_t>& GetCodeSignature();

  // Returns the number of partitions in this UDIF image.
  size_t GetNumberOfPartitions();

  // Returns the partition name for a given partition number, in the range of
  // [0,GetNumberOfPartitions()). This will include the number, name, and type
  // of partition. E.g., "disk image (Apple_HFS : 2)".
  std::string GetPartitionName(size_t part_number);

  // Returns the partition type as a string for the given partition number.
  // E.g., "Apple_HFS" and "Apple_Free".
  std::string GetPartitionType(size_t part_number);

  // Returns the size of the partition in bytes.
  size_t GetPartitionSize(size_t part_number);

  // Returns a stream of the raw partition data for the given partition
  // number.
  std::unique_ptr<ReadStream> GetPartitionReadStream(size_t part_number);

 private:
  // Parses the blkx plist trailer structure.
  bool ParseBlkx();

  const raw_ptr<ReadStream>
      stream_;  // The stream backing the UDIF image. Weak.
  std::vector<std::string> partition_names_;  // The names of all partitions.
  // All blocks in the UDIF image.
  std::vector<std::unique_ptr<const UDIFBlock>> blocks_;
  uint16_t block_size_;  // The image's block size, in bytes.
  std::vector<uint8_t> signature_blob_;  // DMG signature.
};

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_UDIF_H_
