// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SWITCHES_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SWITCHES_H_

#include <string_view>

namespace switches {

// A switch conveying a handle value for the file to which the service should
// emit its logs.
inline constexpr std::string_view kLogFile("log-file");

// A switch conveying the PID of the process in which the log file handle value
// is valid.
inline constexpr std::string_view kLogFileSource("log-file-source");

}  // namespace switches

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SWITCHES_H_
