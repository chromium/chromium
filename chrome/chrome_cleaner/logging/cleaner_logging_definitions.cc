// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides the logging service instance to be used by the cleaner executable.

#include "chrome/chrome_cleaner/logging/cleaner_logging_service.h"
#include "chrome/chrome_cleaner/logging/logging_definitions.h"

namespace chrome_cleaner {

LoggingServiceAPI* GetLoggingServiceForCurrentBuild() {
  return CleanerLoggingService::GetInstance();
}

}  // namespace chrome_cleaner
