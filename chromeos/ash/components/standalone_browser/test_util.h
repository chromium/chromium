// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_TEST_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace base {
class CommandLine;
}  // namespace base

namespace ash::standalone_browser {

// Adds command line arguments that will be passed to Lacros. `new_args` are the
// arguments to add. If there are existing Lacros arguments, the `new_args` will
// be appended. Arguments should include the "--" prefix, e.g. "--foo".
// `command_line` is the current ash-chrome command line. It will be modified by
// this function.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
void AddLacrosArguments(base::span<std::string> new_args,
                        base::CommandLine* command_line);

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_TEST_UTIL_H_
