// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_piece.h"
#include "components/attribution_reporting/eligibility.h"

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
  // Ignore the result; the input may very well be invalid, and the test is
  // concerned about the side effects of calling the parse method (buffer
  // overruns, crashing, et cetera).
  (void)attribution_reporting::ParseEligibleHeader(
      base::StringPiece(reinterpret_cast<const char*>(data), size));
  return 0;
}
