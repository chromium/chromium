// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_SANDBOX_HOOK_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_SANDBOX_HOOK_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace on_device_translation {

// Opens the TranslateKit binary and grants broker file permissions to the
// necessary files required by the binary.
bool OnDeviceTranslationSandboxHook(
    sandbox::policy::SandboxLinux::Options options);

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_SANDBOX_HOOK_H_
