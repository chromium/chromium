// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ITERATOR_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ITERATOR_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "chrome/utility/safe_browsing/mac/udif.h"

namespace safe_browsing {
namespace dmg {

class HFSIterator;
class ReadStream;

// DMGIterator provides iterator access over all of the concrete files located
// on HFS+ and HFSX volumes in a UDIF/DMG file. This class maintains the
// limitations of its composed components.
class DMGIterator {
 public:
  // Creates a DMGIterator from a ReadStream. In most cases, this will be a
  // FileReadStream opened from a DMG file. This does not take ownership
  // of the stream.
  explicit DMGIterator(ReadStream* stream);

  DMGIterator(const DMGIterator&) = delete;
  DMGIterator& operator=(const DMGIterator&) = delete;

  virtual ~DMGIterator();

  // Opens the DMG file for iteration. This must be called before any other
  // method. If this returns false, it is illegal to call any other methods
  // on this class. If this returns true, the iterator is advanced to an
  // invalid element before the first item.
  virtual bool Open();

  // Returns the raw code signature file metadata. This will be empty for DMGs
  // that are not signed.
  virtual const std::vector<uint8_t>& GetCodeSignature();

  // Advances the iterator to the next file item. Returns true on success
  // and false on end-of-iterator.
  virtual bool Next();

  // Returns the full path in a DMG filesystem to the current file item.
  virtual std::u16string GetPath();

  // Returns a ReadStream for the current file item.
  virtual std::unique_ptr<ReadStream> GetReadStream();

  // Returns true when the DMG file has no HFS+ or HFSX partitions.
  virtual bool IsEmpty();

 private:
  UDIFParser udif_;  // The UDIF parser that accesses the partitions.
  // Streams for all the HFS partitions.
  std::vector<std::unique_ptr<ReadStream>> partitions_;
  size_t current_partition_;  // The index in |partitions_| of the current one.
  std::unique_ptr<HFSIterator>
      hfs_;  // The HFSIterator for |current_partition_|.
};

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ITERATOR_H_
