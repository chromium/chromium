// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "base/logging.h"
#include "chromeos/network/onc/variable_expander.h"

namespace chromeos {
namespace variable_expander {

// Disable logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  const std::string machine_name = data_provider.ConsumeRandomLengthString(32);
  std::string str_to_expand = data_provider.ConsumeRemainingBytesAsString();

  VariableExpander expander({{"machine_name", machine_name}});
  expander.ExpandString(&str_to_expand);
  return 0;
}

}  // namespace variable_expander
}  // namespace chromeos
