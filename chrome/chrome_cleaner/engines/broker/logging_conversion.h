// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_LOGGING_CONVERSION_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_LOGGING_CONVERSION_H_

#include <stdint.h>

#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Whether or not an UwS should be logged.
enum class LoggingDecision {
  kUnsupported,  // UwS is not logged because it is not supported.
  kNotLogged,
  kLogged,
};

// Checks if |pup_id| can be reported and returns status information if logging
// is supported or needed for the UwS. Files associated with the UwS that no
// longer exist will be removed from the global PUP data.
LoggingDecision UwSLoggingDecision(UwSId pup_id);

// Returns the appropriate result code for the given engine result code.
ResultCode ScanningResultCode(uint32_t result_code);

// Returns the appropriate result code for the given |engine_result| and
// |needs_reboot| value set during cleaning.
ResultCode CleaningResultCode(uint32_t engine_result, bool needs_reboot);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_LOGGING_CONVERSION_H_
