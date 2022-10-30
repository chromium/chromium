// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/subresource_filter/tools/rule_parser/rule_parser.h"

using subresource_filter::RuleParser;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  RuleParser parser;
  while (provider.remaining_bytes() > 0)
    std::ignore = parser.Parse(provider.ConsumeRandomLengthString());

  return 0;
}
