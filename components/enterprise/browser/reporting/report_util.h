// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UTIL_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UTIL_H_

#include <string>

namespace enterprise_reporting {

// Returns the obfusted `file_path` string with SHA256 algorithm.
std::string ObfuscateFilePath(const std::string& file_path);
}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UTIL_H_
