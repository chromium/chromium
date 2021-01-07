// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/activity_impl.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/updater/updater_branding.h"

namespace updater {
namespace {

base::FilePath GetActiveFile(const std::string& id) {
  return base::GetHomeDir()
      .AppendASCII("Library")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(COMPANY_SHORTNAME_STRING "SoftwareUpdate")
      .AppendASCII("Actives")
      .AppendASCII(id);
}

}  // namespace

bool GetActiveBit(const std::string& id, bool is_machine_) {
  if (is_machine_) {
    // TODO(crbug.com/1096654): Add support for the machine case. Machine
    // installs must look for values in each home dir.
    return false;
  } else {
    base::FilePath path = GetActiveFile(id);
    return base::PathExists(path) && base::PathIsWritable(path);
  }
}

void ClearActiveBit(const std::string& id, bool is_machine_) {
  if (is_machine_) {
    // TODO(crbug.com/1096654): Add support for the machine case. Machine
    // installs must clear values in each home dir.
  } else {
    if (!base::DeleteFile(GetActiveFile(id)))
      VLOG(2) << "Failed to clear activity bit at " << GetActiveFile(id);
  }
}

}  // namespace updater
