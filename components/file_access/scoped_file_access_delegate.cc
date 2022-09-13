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

}  // namespace file_access