// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/scoped_file_access_delegate.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/file_access/scoped_file_access.h"

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
void ScopedFileAccessDelegate::RequestDefaultFilesAccessIO(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(ScopedFileAccess)> callback) {
  if (request_files_access_for_system_io_callback_) {
    request_files_access_for_system_io_callback_->Run(
        files, std::move(callback), /*check_default=*/true);
  } else {
    std::move(callback).Run(ScopedFileAccess::Allowed());
  }
}

// static
void ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(ScopedFileAccess)> callback) {
  if (request_files_access_for_system_io_callback_) {
    request_files_access_for_system_io_callback_->Run(
        files, std::move(callback), /*check_default=*/false);
  } else {
    std::move(callback).Run(ScopedFileAccess::Allowed());
  }
}

// static
ScopedFileAccessDelegate::RequestFilesAccessIOCallback
ScopedFileAccessDelegate::GetCallbackForSystem() {
  return base::BindRepeating(
      [](const std::vector<base::FilePath>& file_paths,
         base::OnceCallback<void(ScopedFileAccess)> callback) {
        if (request_files_access_for_system_io_callback_) {
          request_files_access_for_system_io_callback_->Run(
              file_paths, std::move(callback), /*check_default=*/false);
        } else {
          std::move(callback).Run(ScopedFileAccess::Allowed());
        }
      });
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
ScopedFileAccessDelegate::RequestFilesAccessCheckDefaultCallback*
    ScopedFileAccessDelegate::request_files_access_for_system_io_callback_ =
        nullptr;

ScopedFileAccessDelegate::ScopedRequestFilesAccessCallbackForTesting::
    ScopedRequestFilesAccessCallbackForTesting(
        RequestFilesAccessIOCallback callback,
        bool restore_original_callback)
    : restore_original_callback_(restore_original_callback) {
  original_callback_ =
      base::WrapUnique(request_files_access_for_system_io_callback_);
  request_files_access_for_system_io_callback_ =
      new RequestFilesAccessCheckDefaultCallback(base::BindRepeating(
          [](RequestFilesAccessIOCallback callback,
             const std::vector<base::FilePath>& files,
             base::OnceCallback<void(ScopedFileAccess)> cb,
             bool check_default) { callback.Run(files, std::move(cb)); },
          std::move(callback)));
}

ScopedFileAccessDelegate::ScopedRequestFilesAccessCallbackForTesting::
    ~ScopedRequestFilesAccessCallbackForTesting() {
  if (request_files_access_for_system_io_callback_) {
    delete request_files_access_for_system_io_callback_;
  }
  if (!restore_original_callback_ && original_callback_) {
    original_callback_.reset();
  }
  request_files_access_for_system_io_callback_ = original_callback_.release();
}

void ScopedFileAccessDelegate::ScopedRequestFilesAccessCallbackForTesting::
    RunOriginalCallback(
        const std::vector<base::FilePath>& path,
        base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  original_callback_->Run(path, std::move(callback), /*check_default=*/false);
}

void RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  if (ScopedFileAccessDelegate::HasInstance()) {
    ScopedFileAccessDelegate::Get()->RequestFilesAccess(files, destination_url,
                                                        std::move(callback));
  } else {
    std::move(callback).Run(ScopedFileAccess::Allowed());
  }
}

void RequestFilesAccessForSystem(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  if (ScopedFileAccessDelegate::HasInstance()) {
    ScopedFileAccessDelegate::Get()->RequestFilesAccessForSystem(
        files, std::move(callback));
  } else {
    std::move(callback).Run(ScopedFileAccess::Allowed());
  }
}

ScopedFileAccessDelegate::RequestFilesAccessIOCallback CreateFileAccessCallback(
    const GURL& destination) {
  if (ScopedFileAccessDelegate::HasInstance()) {
    return ScopedFileAccessDelegate::Get()->CreateFileAccessCallback(
        destination);
  }
  return base::BindRepeating(
      [](const GURL& destination, const std::vector<base::FilePath>& files,
         base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
        std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
      },
      destination);
}

}  // namespace file_access
