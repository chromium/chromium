// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/autofill/core/common/autocomplete_parsing_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  const std::string autocomplete_attribute =
      provider.ConsumeRandomLengthString();
  const int64_t field_max_length =
      provider.ConsumeIntegralInRange<int64_t>(0, 100);
  std::ignore = autofill::ParseAutocompleteAttribute(autocomplete_attribute,
                                                     field_max_length);

  return 0;
}
