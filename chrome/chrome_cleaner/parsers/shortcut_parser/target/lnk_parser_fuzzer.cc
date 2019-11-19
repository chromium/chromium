// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::vector<BYTE> file_buffer(data, data + size);

  chrome_cleaner::ParsedLnkFile parsed_shortcut;

  (void)chrome_cleaner::internal::ParseLnkBytes(file_buffer, &parsed_shortcut);
  return 0;
}
