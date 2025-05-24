// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SWITCHES_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SWITCHES_H_

namespace switches {

// A switch conveying a handle value for the file to which the service should
// emit its logs.
inline constexpr char kLogFileHandle[] = "log-file-handle";

// A switch conveying the PID of the process in which the log file handle value
// is valid.
inline constexpr char kLogFileSource[] = "log-file-source";

// A switch that indicates that the service is running on behalf of an
// unattended test (i.e., one for which CHROME_HEADLESS is set in its
// environment block).
inline constexpr char kUnattendedTest[] = "unattended-test";

}  // namespace switches

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SWITCHES_H_
