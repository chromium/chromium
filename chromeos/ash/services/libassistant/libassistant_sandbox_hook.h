// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_SANDBOX_HOOK_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_SANDBOX_HOOK_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace ash::libassistant {

bool LibassistantPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_LIBASSISTANT_SANDBOX_HOOK_H_
