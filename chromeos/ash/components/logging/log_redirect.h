// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGGING_LOG_REDIRECT_H_
#define CHROMEOS_ASH_COMPONENTS_LOGGING_LOG_REDIRECT_H_

#include "base/component_export.h"

namespace base {
class CommandLine;
}

namespace ash {

// Redirects chrome logging to the appropriate session log dir.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING_REDIRECT)
void RedirectChromeLogging(const base::CommandLine& command_line);

// Forces log redirection to occur, even if not running on ChromeOS hardware.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGGING_REDIRECT)
void ForceLogRedirectionForTesting();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGGING_LOG_REDIRECT_H_
