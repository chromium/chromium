// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/chrome_elf/third_party_dlls/status_codes.h"

#include <assert.h>

namespace third_party_dlls {

const wchar_t kStatusCodesRegValue[] = L"StatusCodes";

void AddStatusCodeToBuffer(ThirdPartyStatus code,
                           std::vector<uint8_t>* buffer) {
  assert(buffer);

  // If the existing buffer of status codes is corrupt, overwrite it.
  if (buffer->size() % sizeof(ThirdPartyStatus) != 0) {
    buffer->clear();
  }

  size_t original_size_bytes = buffer->size();
  // Add space for an additional status code.
  buffer->resize(original_size_bytes + sizeof(ThirdPartyStatus));
  // Append the status code in an endian-agnostic way.
  ThirdPartyStatus* temp_array =
      reinterpret_cast<ThirdPartyStatus*>(&(*buffer)[0]);
  temp_array[original_size_bytes / sizeof(ThirdPartyStatus)] = code;
}

void ConvertBufferToStatusCodes(const std::vector<uint8_t>& buffer,
                                std::vector<ThirdPartyStatus>* codes) {
  assert(codes);

  codes->clear();
  if (buffer.size() && (buffer.size() % sizeof(ThirdPartyStatus) == 0)) {
    codes->resize(buffer.size() / sizeof(ThirdPartyStatus));
    ::memcpy(&(*codes)[0], &buffer[0], buffer.size());
  }
}

}  // namespace third_party_dlls
