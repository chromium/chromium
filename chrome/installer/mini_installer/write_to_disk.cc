// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/mini_installer/write_to_disk.h"

#include <windows.h>

#include <stddef.h>

#include <algorithm>

#include "chrome/installer/mini_installer/memory_range.h"
#include "chrome/installer/mini_installer/mini_file.h"

namespace mini_installer {

bool WriteToDisk(const MemoryRange& data, const wchar_t* full_path) {
  MiniFile file;
  if (!file.Create(full_path)) {
    return false;
  }

  // Don't write all of the data at once because this can lead to kernel
  // address-space exhaustion on 32-bit Windows (see https://crbug.com/1001022
  // for details).
  constexpr size_t kMaxWriteAmount = 8 * 1024 * 1024;
  for (size_t total_written = 0; total_written < data.size; /**/) {
    const size_t write_amount =
        std::min(kMaxWriteAmount, data.size - total_written);
    DWORD written = 0;
    if (!::WriteFile(file.GetHandleUnsafe(), data.data + total_written,
                     static_cast<DWORD>(write_amount), &written, nullptr)) {
      const auto write_error = ::GetLastError();

      // Delete the file since the write failed.
      file.DeleteOnClose();
      file.Close();

      ::SetLastError(write_error);
      return false;
    }
    total_written += write_amount;
  }
  return true;
}

}  // namespace mini_installer
