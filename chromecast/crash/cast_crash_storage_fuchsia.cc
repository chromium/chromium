// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "base/no_destructor.h"
#include "chromecast/crash/fuchsia/cast_crash_storage_impl_fuchsia.h"

namespace chromecast {

CastCrashStorage* CastCrashStorage::GetInstance() {
  static base::NoDestructor<CastCrashStorageImplFuchsia> storage(
      base::ComponentContextForProcess()->svc().get());
  return storage.get();
}

}  // namespace chromecast
