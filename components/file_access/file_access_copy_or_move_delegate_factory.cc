// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/file_access/file_access_copy_or_move_delegate_factory.h"

namespace file_access {

// static
FileAccessCopyOrMoveDelegateFactory*
FileAccessCopyOrMoveDelegateFactory::Get() {
  return file_access_copy_or_move_delegate_factory_;
}

// static
bool FileAccessCopyOrMoveDelegateFactory::HasInstance() {
  return file_access_copy_or_move_delegate_factory_;
}

// static
void FileAccessCopyOrMoveDelegateFactory::DeleteInstance() {
  if (file_access_copy_or_move_delegate_factory_) {
    delete file_access_copy_or_move_delegate_factory_;
    file_access_copy_or_move_delegate_factory_ = nullptr;
  }
}

FileAccessCopyOrMoveDelegateFactory::FileAccessCopyOrMoveDelegateFactory() {
  if (file_access_copy_or_move_delegate_factory_) {
    delete file_access_copy_or_move_delegate_factory_;
  }
  file_access_copy_or_move_delegate_factory_ = this;
}

FileAccessCopyOrMoveDelegateFactory::~FileAccessCopyOrMoveDelegateFactory() {
  if (file_access_copy_or_move_delegate_factory_ == this) {
    file_access_copy_or_move_delegate_factory_ = nullptr;
  }
}

// static
FileAccessCopyOrMoveDelegateFactory* FileAccessCopyOrMoveDelegateFactory::
    file_access_copy_or_move_delegate_factory_ = nullptr;

}  // namespace file_access
