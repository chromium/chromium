// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PPAPI_UTILS_H_
#define CHROME_COMMON_PPAPI_UTILS_H_

// Returns true if the interface name passed in is supported by the
// browser.
bool IsSupportedPepperInterface(const char* name);

// Returns whether any NaCl usage is allowed in Chrome. This checks command-line
// flags since this can be called from non-browser processes.
bool IsNaclAllowed();

#endif  // CHROME_COMMON_PPAPI_UTILS_H_
