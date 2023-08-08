// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "chrome/common/ini_parser.h"

// Uses already existing DictionaryValueINIParser
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  DictionaryValueINIParser target = DictionaryValueINIParser();
  std::string input(reinterpret_cast<const char*>(data), size);
  target.Parse(input);
  return 0;
}
