// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos/ash/components/string_matching/tokenized_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1 || size % 2 != 0)
    return 0;

  // Test for std::u16string if size is even.
  std::u16string string_input16(reinterpret_cast<const char16_t*>(data),
                                size / 2);
  ash::string_matching::TokenizedString tokenized_string_from_string16(
      string_input16);
  return 0;
}
