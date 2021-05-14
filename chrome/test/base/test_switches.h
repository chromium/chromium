// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_SWITCHES_H_
#define CHROME_TEST_BASE_TEST_SWITCHES_H_

#include "build/build_config.h"
#include "ppapi/buildflags/buildflags.h"

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAlsoEmitSuccessLogs[];

extern const char kDevtoolsCodeCoverage[];

extern const char kPerfTestPrintUmaMeans[];

#if defined(OS_WIN)
extern const char kEnableHighDpiSupport[];
#endif

}  // namespace switches

#endif  // CHROME_TEST_BASE_TEST_SWITCHES_H_
