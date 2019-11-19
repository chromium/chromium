// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/tokenized_string.h"
#include "base/strings/string16.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1 || size % 2 != 0)
    return 0;

  // Test for base::string16 if size is even.
  base::string16 string_input16(reinterpret_cast<const base::char16*>(data),
                                size / 2);
  ash::TokenizedString tokenized_string_from_string16(string_input16);
  return 0;
}
