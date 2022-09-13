// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SANDBOX_SCREEN_AI_SANDBOX_HOOK_LINUX_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SANDBOX_SCREEN_AI_SANDBOX_HOOK_LINUX_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace screen_ai {

// Opens the chrome_screen_ai.lib binary and grants broker file permissions to
// the necessary files required by the binary.
bool ScreenAIPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SANDBOX_SCREEN_AI_SANDBOX_HOOK_LINUX_H_
