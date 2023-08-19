// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PPAPI_UTILS_H_
#define CHROME_COMMON_PPAPI_UTILS_H_

namespace base {
class CommandLine;
}  // namespace base

// Returns true if the interface name passed in is supported by the
// browser.
bool IsSupportedPepperInterface(const char* name);

// Must be called from the browser process. If not called then NaCl is allowed.
// Once it is called NaCl is disallowed in all processes.
void DisallowNacl();

// Returns whether any NaCl usage is allowed in Chrome. This has a different
// implementation for browser and non-browser processes but the return value
// should be identical.
bool IsNaclAllowed();

// Adds the kDisableNacl command line flag if disable is disallowed.
void AppendDisableNaclSwitchIfNecessary(base::CommandLine* command_line);

#endif  // CHROME_COMMON_PPAPI_UTILS_H_
