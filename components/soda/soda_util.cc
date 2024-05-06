// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace speech {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
bool IsSupportedChromeOS() {
// Some Chrome OS devices do not support on-device speech.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    return false;
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::BrowserParamsProxy::Get()->IsOndeviceSpeechSupported()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX)
bool IsSupportedLinux() {
#if defined(ARCH_CPU_X86_FAMILY)
  // Check if the CPU has the required instruction set to run the Speech
  // On-Device API (SODA) library.
  static bool has_sse41 = base::CPU().has_sse41();
  return has_sse41;
#else
  // Other architectures are not supported.
  return false;
#endif  // defined(ARCH_CPU_X86_FAMILY)
}
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
bool IsSupportedWin() {
#if defined(ARCH_CPU_ARM64)
  // The Speech On-Device API (SODA) component does not support Windows on
  // arm64.
  return false;
#else
  return true;
#endif  // defined(ARCH_CPU_ARM64)
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

bool IsOnDeviceSpeechRecognitionSupported() {
#if BUILDFLAG(IS_CHROMEOS)
  return IsSupportedChromeOS();
#elif BUILDFLAG(IS_LINUX)
  return IsSupportedLinux();
#elif BUILDFLAG(IS_WIN)
  return IsSupportedWin();
#else
  return true;
#endif
}

}  // namespace speech
