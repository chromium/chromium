// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/posix_util.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/updater/util/linux_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_util.h"
#endif

namespace updater {

// Recursively delete a folder and its contents, returning `true` on success.
bool DeleteFolder(const absl::optional<base::FilePath>& installed_path) {
  if (!installed_path)
    return false;
  if (!base::DeletePathRecursively(*installed_path)) {
    PLOG(ERROR) << "Deleting " << *installed_path << " failed";
    return false;
  }
  return true;
}

bool DeleteCandidateInstallFolder(UpdaterScope scope) {
  return DeleteFolder(GetVersionedInstallDirectory(scope));
}

base::FilePath GetUpdaterFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

}  // namespace updater
