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

const char kExecutableName[] = PRODUCT_FULLNAME_STRING;

std::optional<base::FilePath> GetInstallDirectory() {
  base::FilePath application_support_path;
  if (!base::apple::GetLocalDirectory(NSApplicationSupportDirectory,
                                      &application_support_path)) {
    VLOG(1) << "Could not get NSApplicationSupportDirectory path";
    return std::nullopt;
  }
  return application_support_path.Append(COMPANY_SHORTNAME_STRING)
      .Append(PRODUCT_FULLNAME_STRING);
}

std::optional<base::FilePath> FindExistingInstall() {
  std::optional<base::FilePath> path = GetInstallDirectory();
  if (!path) {
    return std::nullopt;
  }

  path = path->Append(base::StrCat({PRODUCT_FULLNAME_STRING, ".app"}))
             .Append("Contents/MacOS")
             .Append(kExecutableName);
  return base::PathExists(*path) ? std::make_optional(*path) : std::nullopt;
}

base::FilePath GetKSAdminPath() {
  return base::FilePath("/Library")
      .Append(COMPANY_SHORTNAME_STRING)
      .Append(KEYSTONE_NAME)
      .Append(base::StrCat({KEYSTONE_NAME, ".bundle"}))
      .Append("Contents/Helpers/ksadmin");
}

}  // namespace enterprise_companion
