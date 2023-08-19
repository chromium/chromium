// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASHPAD_FAKE_OBJECTS_H_
#define COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASHPAD_FAKE_OBJECTS_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/util/process/process_memory.h"

namespace crashpad::test {

class BufferExtensionStreamDataSourceDelegate final
    : public MinidumpUserExtensionStreamDataSource::Delegate {
 public:
  bool ExtensionStreamDataSourceRead(const void* data, size_t size) override;

  std::vector<uint8_t> GetMessage() const;

 private:
  std::vector<uint8_t> data_;
};

class TestProcessMemory final : public ::crashpad::ProcessMemory {
 public:
  using CallbackType = base::RepeatingCallback<
      ssize_t(VMAddress address, size_t size, void* buffer)>;

  TestProcessMemory(ssize_t return_value);
  explicit TestProcessMemory(CallbackType handler_function);

  ~TestProcessMemory() override;

 protected:
  ssize_t ReadUpTo(VMAddress address, size_t size, void* buffer) const override;

 private:
  const base::RepeatingCallback<
      ssize_t(VMAddress address, size_t size, void* buffer)>
      handler_function_;
};

}  // namespace crashpad::test

#endif  // COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASHPAD_FAKE_OBJECTS_H_
