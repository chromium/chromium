// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/testing/crashpad_fake_objects.h"

namespace crashpad::test {

bool BufferExtensionStreamDataSourceDelegate::ExtensionStreamDataSourceRead(
    const void* data,
    size_t size) {
  const uint8_t* const typed_data = reinterpret_cast<const uint8_t*>(data);
  data_.insert(data_.end(), typed_data, typed_data + size);

  return true;
}

std::vector<uint8_t> BufferExtensionStreamDataSourceDelegate::GetMessage()
    const {
  return data_;
}
}  // namespace crashpad::test
