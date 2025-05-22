// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unowned_user_data/scoped_unowned_user_data.h"

#include "chrome/browser/ui/unowned_user_data/unowned_user_data_host.h"

namespace internal {

ScopedUnownedUserDataBase::ScopedUnownedUserDataBase(UnownedUserDataHost& host,
                                                     const char* key,
                                                     void* data)
    : host_(host), key_(key), data_(data) {
  host_->Set(PassKey(), key_, data_);
}

ScopedUnownedUserDataBase::~ScopedUnownedUserDataBase() {
  host_->Erase(PassKey(), key_);
}

// static
void* ScopedUnownedUserDataBase::GetInternal(UnownedUserDataHost& host,
                                             const char* key) {
  return host.Get(PassKey(), key);
}

}  // namespace internal
