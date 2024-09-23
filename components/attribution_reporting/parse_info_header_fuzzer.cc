// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <tuple>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "components/attribution_reporting/registration_info.h"

namespace {

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    base::i18n::InitializeICU();
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  for (const bool cross_app_web_enabled : {false, true}) {
    std::ignore = attribution_reporting::RegistrationInfo::ParseInfo(
        std::string_view(reinterpret_cast<const char*>(data), size),
        cross_app_web_enabled);
  }
  return 0;
}
