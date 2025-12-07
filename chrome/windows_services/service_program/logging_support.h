// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_LOGGING_SUPPORT_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_LOGGING_SUPPORT_H_

namespace base {
class CommandLine;
}

// Initializes logging in the service process. Tests may redirect logging by use
// of a `ScopedLogGrabber` instance.
void InitializeLogging(const base::CommandLine& command_line);

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_LOGGING_SUPPORT_H_
