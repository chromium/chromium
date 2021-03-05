// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity_impl.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/updater/mac/mac_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
namespace {

base::FilePath GetActiveFile(UpdaterScope scope, const std::string& id) {
  // TODO(crbug.com/1096654): Add support for UpdaterScope::kSystem.
  return base::GetHomeDir()
      .AppendASCII("Library")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

}  // namespace

bool GetActiveBit(UpdaterScope scope, const std::string& id) {
  const base::FilePath path = GetActiveFile(scope, id);
  return base::PathExists(path) && base::PathIsWritable(path);
}

void ClearActiveBit(UpdaterScope scope, const std::string& id) {
  if (!base::DeleteFile(GetActiveFile(scope, id)))
    VLOG(2) << "Failed to clear activity bit at " << GetActiveFile(scope, id);
}

}  // namespace updater
