// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_delegate.h"

namespace file_access {
// static
ScopedFileAccessDelegate* ScopedFileAccessDelegate::Get() {
  return scoped_file_access_delegate_;
}

// static
bool ScopedFileAccessDelegate::HasInstance() {
  return scoped_file_access_delegate_;
}

// static
void ScopedFileAccessDelegate::DeleteInstance() {
  if (scoped_file_access_delegate_) {
    delete scoped_file_access_delegate_;
    scoped_file_access_delegate_ = nullptr;
  }
}

// static
void ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(ScopedFileAccess)> callback) {
  if (request_files_access_for_system_io_callback_) {
    request_files_access_for_system_io_callback_->Run(files,
                                                      std::move(callback));
  } else {
    std::move(callback).Run(ScopedFileAccess::Allowed());
  }
}

// static
ScopedFileAccessDelegate::RequestFilesAccessForSystemIOCallback*
ScopedFileAccessDelegate::SetRequestFilesAccessForSystemIOCallbackForTesting(
    RequestFilesAccessForSystemIOCallback callback) {
  auto* old_ptr = request_files_access_for_system_io_callback_;
  request_files_access_for_system_io_callback_ =
      new RequestFilesAccessForSystemIOCallback(std::move(callback));
  return old_ptr;
}

ScopedFileAccessDelegate::ScopedFileAccessDelegate() {
  if (scoped_file_access_delegate_) {
    delete scoped_file_access_delegate_;
  }
  scoped_file_access_delegate_ = this;
}

ScopedFileAccessDelegate::~ScopedFileAccessDelegate() {
  if (scoped_file_access_delegate_ == this) {
    scoped_file_access_delegate_ = nullptr;
  }
}

// static
ScopedFileAccessDelegate*
    ScopedFileAccessDelegate::scoped_file_access_delegate_ = nullptr;

// static
ScopedFileAccessDelegate::RequestFilesAccessForSystemIOCallback*
    ScopedFileAccessDelegate::request_files_access_for_system_io_callback_ =
        nullptr;

}  // namespace file_access