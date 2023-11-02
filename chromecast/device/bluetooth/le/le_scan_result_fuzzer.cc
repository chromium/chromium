// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/le_scan_result.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::span<const uint8_t> adv_data(data, size);
  chromecast::bluetooth::LeScanResult res;
  res.SetAdvData(adv_data);
  return 0;
}
