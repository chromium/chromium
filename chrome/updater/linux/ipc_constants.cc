// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/ipc_constants.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kUserSocketsRelDir[] = FILE_PATH_LITERAL(
    ".local/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING "/");
constexpr base::FilePath::CharType kSystemSocketsDir[] = FILE_PATH_LITERAL(
    "/run/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING "/");

base::FilePath GetSocketsDir(UpdaterScope scope) {
  return scope == UpdaterScope::kSystem
             ? base::FilePath(kSystemSocketsDir)
             : base::GetHomeDir().Append(kUserSocketsRelDir);
}

}  // namespace

base::FilePath GetActiveDutySocketPath(UpdaterScope scope) {
  return GetSocketsDir(scope).Append(
      FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".sk"));
}

base::FilePath GetActiveDutyInternalSocketPath(UpdaterScope scope) {
  return GetSocketsDir(scope).AppendASCII(
      base::StrCat({PRODUCT_FULLNAME_STRING, kUpdaterVersion, ".sk"}));
}

base::FilePath GetActivationSocketPath(UpdaterScope scope) {
  return GetSocketsDir(scope).Append(
      FILE_PATH_LITERAL(PRODUCT_FULLNAME_STRING ".activation.sk"));
}

}  // namespace updater
