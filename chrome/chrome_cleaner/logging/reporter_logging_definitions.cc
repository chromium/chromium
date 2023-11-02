// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides the logging service instance to be used by the reporter executable.

#include "chrome/chrome_cleaner/logging/logging_definitions.h"
#include "chrome/chrome_cleaner/logging/noop_logging_service.h"
#include "chrome/chrome_cleaner/logging/reporter_logging_service.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

// Returns an instance of ReporterLoggingService if logs collection is enabled,
// or NoOpLoggingService otherwise.
LoggingServiceAPI* GetLoggingServiceForCurrentBuild() {
  if (Settings::GetInstance()->logs_collection_enabled())
    return ReporterLoggingService::GetInstance();
  return NoOpLoggingService::GetInstance();
}

}  // namespace chrome_cleaner
