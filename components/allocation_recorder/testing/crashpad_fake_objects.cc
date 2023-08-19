// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/testing/crashpad_fake_objects.h"

namespace crashpad::test {
namespace {
TestProcessMemory::CallbackType CreateHandlerReturningValue(
    ssize_t return_value) {
  return base::BindRepeating(
      [](ssize_t return_value, VMAddress address, size_t size, void* buffer) {
        return return_value;
      },
      return_value);
}
}  // namespace

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

TestProcessMemory::TestProcessMemory(ssize_t return_value)
    : TestProcessMemory(CreateHandlerReturningValue(return_value)) {}

TestProcessMemory::TestProcessMemory(CallbackType handler_function)
    : handler_function_(std::move(handler_function)) {}

TestProcessMemory::~TestProcessMemory() = default;

ssize_t TestProcessMemory::ReadUpTo(VMAddress address,
                                    size_t size,
                                    void* buffer) const {
  return handler_function_.Run(address, size, buffer);
}

}  // namespace crashpad::test
