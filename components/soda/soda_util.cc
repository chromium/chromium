// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_util.h"

#include "base/cpu.h"
#include "base/feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace speech {

bool IsOnDeviceSpeechRecognitionSupported() {
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
#endif

#if BUILDFLAG(IS_LINUX)
  // Check if the CPU has the required instruction set to run the Speech
  // On-Device API (SODA) library.
  static bool has_sse41 = base::CPU().has_sse41();
  if (!has_sse41) {
    return false;
  }
#endif

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
  // The Speech On-Device API (SODA) component does not support Windows on
  // arm64.
  return false;
#else
  return true;
#endif
}

}  // namespace speech
