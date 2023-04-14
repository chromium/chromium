// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "chrome/utility/safe_browsing/mac/hfs.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"

namespace safe_browsing {
namespace dmg {

DMGIterator::DMGIterator(ReadStream* stream)
    : udif_(stream),
      partitions_(),
      current_partition_(0),
      hfs_() {
}

DMGIterator::~DMGIterator() {}

bool DMGIterator::Open() {
  if (!udif_.Parse()) {
    return false;
  }

  // Collect all the HFS partitions up-front. The data are accessed lazily, so
  // this is relatively inexpensive.
  for (size_t i = 0; i < udif_.GetNumberOfPartitions(); ++i) {
    std::unique_ptr<ReadStream> partition = udif_.GetPartitionReadStream(i);
    HFSIterator hfs(partition.get());
    if (hfs.Open()) {
      partitions_.push_back(std::move(partition));
    }
  }

  return true;
}

const std::vector<uint8_t>& DMGIterator::GetCodeSignature() {
  return udif_.GetCodeSignature();
}

bool DMGIterator::Next() {
  // Iterate through all the HFS partitions in the DMG file.
  for (; current_partition_ < partitions_.size(); ++current_partition_) {
    if (!hfs_) {
      hfs_ =
          std::make_unique<HFSIterator>(partitions_[current_partition_].get());
      if (!hfs_->Open())
        continue;
    }

    // Iterate through the HFS filesystem until a concrete file is found.
    while (true) {
      if (!hfs_->Next())
        break;

      // Skip directories and symlinks.
      if (hfs_->IsDirectory() || hfs_->IsSymbolicLink())
        continue;

      // Hard links are not supported by the HFSIterator.
      if (hfs_->IsHardLink())
        continue;

      // Decmpfs compression is not supported by the HFSIterator.
      if (hfs_->IsDecmpfsCompressed())
        continue;

      // This must be a normal file!
      return true;
    }

    hfs_.reset();
  }

  return false;
}

std::u16string DMGIterator::GetPath() {
  return hfs_->GetPath();
}

std::unique_ptr<ReadStream> DMGIterator::GetReadStream() {
  return hfs_->GetReadStream();
}

bool DMGIterator::IsEmpty() {
  return partitions_.empty();
}

}  // namespace dmg
}  // namespace safe_browsing
