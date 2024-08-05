// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "chrome/utility/safe_browsing/mac/hfs.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "chrome/utility/safe_browsing/mac/udif.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::CommandLine::Init(*argc, *argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_NONE;
  logging::SetMinLogLevel(logging::LOGGING_FATAL);
  return InitLogging(settings);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // SAFETY: libfuzzer guarantees a valid pointer and size pair.
  safe_browsing::dmg::MemoryReadStream input(
      UNSAFE_BUFFERS(base::span(data, size)));
  safe_browsing::dmg::UDIFParser udif_parser(&input);

  if (!udif_parser.Parse())
    return 0;

  std::vector<uint8_t> buffer(getpagesize(), 0);

  for (size_t i = 0; i < udif_parser.GetNumberOfPartitions(); ++i) {
    std::unique_ptr<safe_browsing::dmg::ReadStream> partition(
        udif_parser.GetPartitionReadStream(i));
    safe_browsing::dmg::HFSIterator iterator(partition.get());

    if (!iterator.Open())
      continue;

    while (iterator.Next()) {
      if (iterator.IsHardLink() ||
          iterator.IsDecmpfsCompressed() ||
          iterator.IsDirectory()) {
        continue;
      }

      std::unique_ptr<safe_browsing::dmg::ReadStream> file(
          iterator.GetReadStream());
      size_t read_this_pass = 0;
      do {
        if (!file->Read(buffer, &read_this_pass)) {
          break;
        }
      } while (read_this_pass != 0);
    }
  }

  return 0;
}
