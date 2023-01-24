// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl_qualifying.h"

#include "chrome/updater/updater_scope.h"

namespace updater {

bool DoPlatformSpecificHealthChecks(UpdaterScope scope) {
  return true;
}

}  // namespace updater
