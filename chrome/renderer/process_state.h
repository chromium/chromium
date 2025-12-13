// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PROCESS_STATE_H_
#define CHROME_RENDERER_PROCESS_STATE_H_

#include "build/build_config.h"

namespace process_state {

// Returns true if this renderer process is incognito.
bool IsIncognitoProcess();

// Sets whether this renderer process is an incognito process.
void SetIsIncognitoProcess(bool is_incognito_process);

// Returns true if this renderer process is an instant process.
// When kInstantUsesSpareRenderer is enabled, instant processes
// are identified by the is_instant_process rather than the cmdline
// switch. This allows spare renderers to be dynamically designated
// as an instant process.
bool IsInstantProcess();

#if !BUILDFLAG(IS_ANDROID)
// Sets whether this renderer process is an instant process. This
// value is only used when kInstantUsesSpareRenderer feature is
// enabled; otherwise instant process status is determined by
// cmdline switches.
void SetIsInstantProcess(bool is_instant_process);
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace process_state

#endif  // CHROME_RENDERER_PROCESS_STATE_H_
