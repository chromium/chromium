// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO: crbug.com/352295124 - Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <optional>

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

namespace password_manager {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string payload(reinterpret_cast<const char*>(data), size);
  std::string key;
  std::optional<std::string> result =
      password_manager::CipherEncrypt(payload, &key);
  return 0;
}

}  // namespace password_manager
