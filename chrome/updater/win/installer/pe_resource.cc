// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/pe_resource.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace updater {

PEResource::PEResource(const wchar_t* name, const wchar_t* type, HMODULE module)
    : module_(module) {
  resource_ = ::FindResource(module, name, type);
}

bool PEResource::IsValid() const {
  return nullptr != resource_;
}

size_t PEResource::Size() const {
  return ::SizeofResource(module_, resource_);
}

bool PEResource::WriteToDisk(const wchar_t* full_path) {
  // Resource handles are not real HGLOBALs so do not attempt to close them.
  // Resources are freed when the containing module is unloaded.
  HGLOBAL data_handle = ::LoadResource(module_, resource_);
  if (nullptr == data_handle) {
    return false;
  }

  const char* data = reinterpret_cast<const char*>(::LockResource(data_handle));
  if (nullptr == data) {
    return false;
  }

  base::span<const char> UNSAFE_BUFFERS(data_span(data, Size()));

  HANDLE out_file = ::CreateFile(full_path, GENERIC_WRITE, 0, nullptr,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (INVALID_HANDLE_VALUE == out_file) {
    return false;
  }

  // Don't write all of the data at once because this can lead to kernel
  // address-space exhaustion on 32-bit Windows (see https://crbug.com/1001022
  // for details).
  static constexpr size_t kMaxWriteAmount = 8 * 1024 * 1024;
  for (size_t total_written = 0; total_written < data_span.size(); /**/) {
    const size_t write_amount =
        std::min(kMaxWriteAmount, data_span.size() - total_written);
    base::span<const char> data_to_write =
        data_span.subspan(total_written, write_amount);
    DWORD written = 0;
    if (!::WriteFile(out_file, data_to_write.data(),
                     static_cast<DWORD>(data_to_write.size()), &written,
                     nullptr)) {
      ::CloseHandle(out_file);
      return false;
    }
    total_written += data_to_write.size();
  }

  return ::CloseHandle(out_file);
}

}  // namespace updater
