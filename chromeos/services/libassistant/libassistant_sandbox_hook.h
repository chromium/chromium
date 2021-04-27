// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SANDBOX_HOOK_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SANDBOX_HOOK_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace chromeos {
namespace libassistant {

bool LibassistantPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_LIBASSISTANT_SANDBOX_HOOK_H_
