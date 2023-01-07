// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/preg_parser.h"
#include "components/policy/core/common/registry_dict.h"

namespace {

const char16_t kRegistryChromePolicyKey[] = u"SOFTWARE\\Policies\\Chromium";

}  // namespace

namespace policy {
namespace preg_parser {

// Disable logging.
struct Environment {
  Environment() : root(kRegistryChromePolicyKey) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }

  const std::u16string root;
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Note: Don't use PolicyLoadStatusUmaReporter here, it leaks!
  PolicyLoadStatusSampler status;
  RegistryDict dict;
  ReadDataInternal(data, size, env->root, &dict, &status, "data");
  return 0;
}

}  // namespace preg_parser
}  // namespace policy
