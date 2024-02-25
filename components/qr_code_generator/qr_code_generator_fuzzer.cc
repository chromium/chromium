// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/qr_code_generator/qr_code_generator.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  int min_version = provider.ConsumeIntegral<int>();
  auto qr_data = provider.ConsumeRemainingBytes<uint8_t>();

  std::ignore = qr_code_generator::GenerateCode(qr_data, min_version);
  return 0;
}
