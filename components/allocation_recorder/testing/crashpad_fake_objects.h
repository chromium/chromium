// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASHPAD_FAKE_OBJECTS_H_
#define COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASHPAD_FAKE_OBJECTS_H_

#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"

#include <cstdint>
#include <vector>

namespace crashpad::test {

class BufferExtensionStreamDataSourceDelegate final
    : public MinidumpUserExtensionStreamDataSource::Delegate {
 public:
  bool ExtensionStreamDataSourceRead(const void* data, size_t size) override;

  std::vector<uint8_t> GetMessage() const;

 private:
  std::vector<uint8_t> data_;
};

}  // namespace crashpad::test

#endif  // COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASHPAD_FAKE_OBJECTS_H_
