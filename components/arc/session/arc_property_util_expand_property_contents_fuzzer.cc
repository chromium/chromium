// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_property_util.h"

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <map>
#include <string>

#include "base/command_line.h"
#include "components/arc/test/fake_cros_config.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

namespace {
constexpr size_t kMaxInputSize = 256 * 1024;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::CommandLine::Init(*argc, *argv);
  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Limit the input size to avoid timing out on ClusterFuzz.
  if (size > kMaxInputSize)
    return 0;

  FuzzedDataProvider data_provider(data, size);

  std::string content = data_provider.ConsumeRandomLengthString(size);

  arc::FakeCrosConfig config;
  while (data_provider.remaining_bytes()) {
    // Cannot use |ConsumeRandomLengthString| in a loop because it can enter an
    // infinite loop by always returning an empty string.

    size_t path_size = data_provider.ConsumeIntegralInRange<size_t>(
        0, data_provider.remaining_bytes());
    std::string path =
        std::string("/") + data_provider.ConsumeBytesAsString(path_size);

    if (data_provider.remaining_bytes() == 0)
      break;

    size_t property_size = data_provider.ConsumeIntegralInRange<size_t>(
        1, data_provider.remaining_bytes());
    std::string property = data_provider.ConsumeBytesAsString(property_size);

    if (data_provider.remaining_bytes() == 0)
      break;

    size_t val_size = data_provider.ConsumeIntegralInRange<size_t>(
        1, data_provider.remaining_bytes());
    std::string val = data_provider.ConsumeBytesAsString(val_size);

    config.SetString(path, property, val);
  }

  std::string expanded_content;
  arc::ExpandPropertyContentsForTesting(content, &config, &expanded_content);

  return 0;
}
