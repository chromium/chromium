// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_TTS_TTS_SANDBOX_HOOK_H_
#define CHROMEOS_SERVICES_TTS_TTS_SANDBOX_HOOK_H_

#include "sandbox/policy/linux/sandbox_linux.h"

namespace chromeos {
namespace tts {

bool TtsPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace tts
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_TTS_TTS_SANDBOX_HOOK_H_
