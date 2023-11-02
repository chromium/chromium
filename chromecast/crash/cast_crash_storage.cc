// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_storage.h"

#include "base/no_destructor.h"
#include "chromecast/crash/cast_crash_storage_impl.h"

namespace chromecast {

CastCrashStorage* CastCrashStorage::GetInstance() {
  static base::NoDestructor<CastCrashStorageImpl> storage;
  return storage.get();
}

}  // namespace chromecast
