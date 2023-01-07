// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_util.h"
#include "base/files/file_path.h"
#include "crypto/sha2.h"

namespace enterprise_reporting {

std::string ObfuscateFilePath(const std::string& file_path) {
  return crypto::SHA256HashString(file_path);
}

}  // namespace enterprise_reporting
