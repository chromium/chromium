// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/installer_paths.h"

#include <optional>

#include "base/files/file_path.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"

namespace enterprise_companion {

const char kExecutableName[] = "enterprise_companion";

std::optional<base::FilePath> GetInstallDirectory() {
  return base::FilePath("/opt/")
      .AppendASCII(COMPANY_SHORTNAME_STRING)
      .AppendASCII(PRODUCT_FULLNAME_STRING);
}

}  // namespace enterprise_companion
