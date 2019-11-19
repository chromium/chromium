// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/hash/hash.h"
#include "chrome/chrome_elf/dll_hash/dll_hash.h"

int DllNameToHash(const std::string& dll_name) {
  uint32_t data = base::Hash(dll_name);

  // Strip off the signed bit because UMA doesn't support negative values,
  // but takes a signed int as input.
  return static_cast<int>(data & 0x7fffffff);
}
