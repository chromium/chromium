// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "chrome/services/ipp_parser/public/cpp/ipp_converter.h"

// This turns off service error logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  auto ipp = ipp_converter::ParseIppMessage({data, size});
  ipp_converter::ConvertIppToMojo(ipp.get());

  return 0;
}
