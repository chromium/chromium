// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/util.h"

#include <pwd.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/version.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"

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

bool PathOwnedByUser(const base::FilePath& path) {
  struct passwd* result = nullptr;
  struct passwd user_info = {};
  char pwbuf[2048] = {};
  uid_t user_uid = geteuid();

  int error = getpwuid_r(user_uid, &user_info, pwbuf, sizeof(pwbuf), &result);

  if (error) {
    VLOG(1) << "Failed to get user info.";
    return true;
  }

  if (result == nullptr) {
    VLOG(1) << "No entry for user.";
    return true;
  }

  base::stat_wrapper_t stat_info = {};
  if (base::File::Lstat(path.value().c_str(), &stat_info) != 0) {
    DPLOG(ERROR) << "Failed to get information on path " << path.value();
    return false;
  }

  if (S_ISLNK(stat_info.st_mode)) {
    DLOG(ERROR) << "Path " << path.value() << " is a symbolic link.";
    return false;
  }

  if (stat_info.st_uid != user_uid) {
    DLOG(ERROR) << "Path " << path.value() << " is owned by the wrong user.";
    return false;
  }

  return true;
}

}  // namespace updater
