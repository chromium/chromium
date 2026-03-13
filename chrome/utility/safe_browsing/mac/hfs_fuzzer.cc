// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/hfs.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  safe_browsing::dmg::MemoryReadStream input(data);
  safe_browsing::dmg::HFSIterator hfs_iterator(&input);

  if (!hfs_iterator.Open()) {
    return 0;
  }

  std::vector<uint8_t> buffer(getpagesize(), 0);

  while (hfs_iterator.Next()) {
    // Test accessing properties.
    std::ignore = hfs_iterator.IsSymbolicLink();
    std::ignore = hfs_iterator.IsDecmpfsCompressed();
    std::ignore = hfs_iterator.GetPath();

    if (hfs_iterator.IsDirectory() || hfs_iterator.IsHardLink()) {
      continue;
    }

    // Read out file contents.
    std::unique_ptr<safe_browsing::dmg::ReadStream> file(
        hfs_iterator.GetReadStream());
    size_t read_this_pass = 0;
    do {
      if (!file->Read(buffer, &read_this_pass)) {
        break;
      }
    } while (read_this_pass != 0);
  }

  return 0;
}
