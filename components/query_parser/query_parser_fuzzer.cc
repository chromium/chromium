// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/query_parser/query_parser.h"

struct Environment {
  Environment() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  constexpr size_t kMaxSize = 1 << 16;
  if (size > kMaxSize)
    return 0;

  FuzzedDataProvider data_provider(data, size);
  const query_parser::MatchingAlgorithm matching_alg =
      data_provider.ConsumeEnum<query_parser::MatchingAlgorithm>();
  const std::u16string query16 = base::UTF8ToUTF16(
      data_provider.ConsumeBytesAsString(data_provider.remaining_bytes()));

  std::vector<std::u16string> words;
  query_parser::QueryParser::ParseQueryWords(query16, matching_alg, &words);

  return 0;
}
