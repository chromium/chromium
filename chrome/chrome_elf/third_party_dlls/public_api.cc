// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/public_api.h"

#include <stddef.h>

namespace third_party_dlls {

uint32_t GetLogEntrySize(uint32_t path_len) {
  // Include padding in the size to fill it to a 32-bit boundary (add one byte
  // for the string terminator and up to three bytes of padding, which will be
  // truncated down to the proper amount).
  return ((offsetof(LogEntry, path) + path_len + 4) & ~3U);
}

}  // namespace third_party_dlls
