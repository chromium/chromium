// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_line_reader.h"

#include <memory>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>
namespace {

constexpr int kUpperMaxLineLengthBound = 1024;

}  // namespace

namespace chromeos {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  const size_t line_length =
      data_provider.ConsumeIntegralInRange<size_t>(0, kUpperMaxLineLengthBound);
  const std::string contents = data_provider.ConsumeRemainingBytesAsString();

  std::unique_ptr<PpdLineReader> ppd_line_reader =
      PpdLineReader::Create(contents, line_length);

  std::string line;
  while (ppd_line_reader->NextLine(&line)) {
    // Call NextLine() until we hit the end of the file or an error.
  }

  return 0;
}

}  // namespace chromeos