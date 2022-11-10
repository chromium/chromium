// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/linux/ipc_constants.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/version.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

constexpr char kUserSocketsRelDir[] =
    ".local/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING "/";
constexpr char kSystemSocketsDir[] =
    "/run/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING "/";

absl::optional<base::FilePath> GetSocketsDir(UpdaterScope scope) {
  base::FilePath path;
  switch (scope) {
    case UpdaterScope::kUser:
      if (!base::PathService::Get(base::DIR_HOME, &path)) {
        return absl::nullopt;
      }
      return path.AppendASCII(kUserSocketsRelDir);
    case UpdaterScope::kSystem:
      return base::FilePath(kSystemSocketsDir);
  }
}

}  // namespace

absl::optional<base::FilePath> GetActiveDutySocketPath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetSocketsDir(scope);
  return path ? absl::make_optional<base::FilePath>(
                    path->AppendASCII(PRODUCT_FULLNAME_STRING ".sk"))
              : absl::nullopt;
}

absl::optional<base::FilePath> GetActiveDutyInternalSocketPath(
    UpdaterScope scope,
    const base::Version& version) {
  absl::optional<base::FilePath> path = GetSocketsDir(scope);
  return path ? path->AppendASCII(base::StrCat(
                    {PRODUCT_FULLNAME_STRING, version.GetString(), ".sk"}))
              : absl::optional<base::FilePath>();
}

}  // namespace updater
