// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "chrome/utility/safe_browsing/mac/hfs.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  safe_browsing::dmg::MemoryReadStream input(data, size);
  safe_browsing::dmg::HFSIterator hfs_iterator(&input);

  if (!hfs_iterator.Open())
    return 0;

  std::vector<uint8_t> buffer(getpagesize(), 0);

  while (hfs_iterator.Next()) {
    // Test accessing properties.
    std::ignore = hfs_iterator.IsSymbolicLink();
    std::ignore = hfs_iterator.IsDecmpfsCompressed();
    std::ignore = hfs_iterator.GetPath();

    if (hfs_iterator.IsDirectory() || hfs_iterator.IsHardLink())
      continue;

    // Read out file contents.
    std::unique_ptr<safe_browsing::dmg::ReadStream> file(
        hfs_iterator.GetReadStream());
    size_t read_this_pass = 0;
    do {
      if (!file->Read(buffer.data(), buffer.size(), &read_this_pass))
        break;
    } while (read_this_pass != 0);
  }

  return 0;
}
