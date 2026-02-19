// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/core/common/translate_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace {

void GetTranslateSecurityOriginDoesNotCrash(std::string fuzzed_origin) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      translate::switches::kTranslateSecurityOrigin, fuzzed_origin);
  translate::GetTranslateSecurityOrigin();
}

FUZZ_TEST(TranslateUtilFuzzTest, GetTranslateSecurityOriginDoesNotCrash);

}  // namespace
