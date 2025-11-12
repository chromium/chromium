// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

namespace {

// Start the update service.
bool DialUpdateService(UpdaterScope scope, bool internal) {
  // TODO(crbug.com/456542123): Implement the dialer.
  return false;
}

}  // namespace

bool DialUpdateService(UpdaterScope scope) {
  return DialUpdateService(scope, false);
}

bool DialUpdateInternalService(UpdaterScope scope) {
  return DialUpdateService(scope, true);
}

}  // namespace updater
