// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "components/qr_code_generator/qr_code_generator.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  constexpr uint8_t kMaxMask = 7;

  int min_version = provider.ConsumeIntegral<int>();
  uint8_t mask = provider.ConsumeIntegralInRange<uint8_t>(0, kMaxMask);
  auto qr_data = provider.ConsumeRemainingBytes<uint8_t>();

  QRCodeGenerator qr;
  std::ignore = qr.Generate(qr_data, min_version, mask);
  return 0;
}
