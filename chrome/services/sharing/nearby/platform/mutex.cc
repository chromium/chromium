// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/mutex.h"

namespace nearby::chrome {

Mutex::Mutex() = default;

Mutex::~Mutex() = default;

void Mutex::Lock() EXCLUSIVE_LOCK_FUNCTION() {
  lock_.Acquire();
}

void Mutex::Unlock() UNLOCK_FUNCTION() {
  lock_.Release();
}

}  // namespace nearby::chrome
