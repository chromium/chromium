// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_file.h"

#include <utility>

namespace mini_installer {

MiniFile::MiniFile() = default;

MiniFile::~MiniFile() {
  Close();
}

MiniFile& MiniFile::operator=(MiniFile&& other) noexcept {
  Close();
  path_.assign(other.path_);
  other.path_.clear();
  handle_ = std::exchange(other.handle_, INVALID_HANDLE_VALUE);
  return *this;
}

bool MiniFile::Create(const wchar_t* path) {
  Close();
  if (!path_.assign(path))
    return false;
  handle_ =
      ::CreateFileW(path_.get(), DELETE | GENERIC_WRITE, FILE_SHARE_DELETE,
                    nullptr, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (handle_ != INVALID_HANDLE_VALUE)
    return true;
  path_.clear();
  return false;
}

bool MiniFile::IsValid() const {
  return handle_ != INVALID_HANDLE_VALUE;
}

bool MiniFile::DeleteOnClose() {
  FILE_DISPOSITION_INFO disposition = {/*DeleteFile=*/TRUE};
  return IsValid() &&
         ::SetFileInformationByHandle(handle_, FileDispositionInfo,
                                      &disposition, sizeof(disposition));
}

void MiniFile::Close() {
  if (IsValid())
    ::CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE));
  path_.clear();
}

HANDLE MiniFile::DuplicateHandle() const {
  if (!IsValid())
    return INVALID_HANDLE_VALUE;
  HANDLE handle = INVALID_HANDLE_VALUE;
  return ::DuplicateHandle(::GetCurrentProcess(), handle_,
                           ::GetCurrentProcess(), &handle,
                           /*dwDesiredAccess=*/0,
                           /*bInerhitHandle=*/FALSE, DUPLICATE_SAME_ACCESS)
             ? handle
             : INVALID_HANDLE_VALUE;
}

HANDLE MiniFile::GetHandleUnsafe() const {
  return handle_;
}

}  // namespace mini_installer
