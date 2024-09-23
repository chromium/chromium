// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer_paths.h"

#import <Foundation/Foundation.h>

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"

namespace enterprise_companion {

const char kExecutableName[] = "enterprise_companion";

std::optional<base::FilePath> GetInstallDirectory() {
  base::FilePath application_support_path;
  if (!base::apple::GetLocalDirectory(NSApplicationSupportDirectory,
                                      &application_support_path)) {
    LOG(ERROR) << "Could not get NSApplicationSupportDirectory path";
    return std::nullopt;
  }
  return application_support_path.AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

std::optional<base::FilePath> FindExistingInstall() {
  std::optional<base::FilePath> path = GetInstallDirectory();
  if (!path) {
    return std::nullopt;
  }
  path = path->AppendASCII(kExecutableName);
  return base::PathExists(*path) ? path : std::nullopt;
}

base::FilePath GetKSAdminPath() {
  return base::FilePath("/Library")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(KEYSTONE_NAME)
      .AppendASCII(base::StrCat({KEYSTONE_NAME, ".bundle"}))
      .Append(FILE_PATH_LITERAL("Contents/Helpers/ksadmin"));
}

}  // namespace enterprise_companion
