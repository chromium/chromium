// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/util.h"

#include <unistd.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/version.h"
#include "chrome/updater/updater_version.h"

namespace updater {
namespace {

base::FilePath GetUpdateFolderName() {
  return base::FilePath(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

base::FilePath ExecutableFolderPath() {
  return base::FilePath(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".app"))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"));
}

}  // namespace

bool IsSystemInstall() {
  return geteuid() == 0;
}

base::FilePath GetLibraryFolderPath() {
  if (!IsSystemInstall())
    return base::mac::GetUserLibraryPath();

  base::FilePath local_library_path;
  if (base::mac::GetLocalDirectory(NSLibraryDirectory, &local_library_path)) {
    return local_library_path;
  }
  VLOG(1) << "Could not get local library path";
  return base::FilePath();
}

base::FilePath GetUpdaterFolderPath() {
  return GetLibraryFolderPath().Append(GetUpdateFolderName());
}

base::FilePath GetVersionedUpdaterFolderPathForVersion(
    const base::Version& version) {
  return GetUpdaterFolderPath().AppendASCII(version.GetString());
}

base::FilePath GetVersionedUpdaterFolderPath() {
  return GetUpdaterFolderPath().AppendASCII(UPDATER_VERSION_STRING);
}

base::FilePath GetExecutableFolderPathForVersion(const base::Version& version) {
  return GetVersionedUpdaterFolderPathForVersion(version).Append(
      ExecutableFolderPath());
}

base::FilePath GetUpdaterExecutablePath() {
  return GetVersionedUpdaterFolderPath()
      .Append(ExecutableFolderPath())
      .Append(FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING));
}

}  // namespace updater
