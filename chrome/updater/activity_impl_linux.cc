// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity.h"

#include <string>

#include "base/notreached.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

// TODO(crbug.com/1276080) : needs implementations for actives API.

bool GetActiveBit(UpdaterScope /*scope*/, const std::string& /*id*/) {
  NOTIMPLEMENTED();
  return true;
}

void ClearActiveBit(UpdaterScope /*scope*/, const std::string& /*id*/) {
  NOTIMPLEMENTED();
}

}  // namespace updater
