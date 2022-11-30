// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_LOGGING_DEFINITIONS_H_
#define CHROME_CHROME_CLEANER_LOGGING_LOGGING_DEFINITIONS_H_

// All executable targets build in this project should contain one
// *_logging_definitions.cc file to ensure that this function is defined.

namespace chrome_cleaner {

class LoggingServiceAPI;

LoggingServiceAPI* GetLoggingServiceForCurrentBuild();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_LOGGING_DEFINITIONS_H_
