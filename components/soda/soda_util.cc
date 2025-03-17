// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_util.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
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
  return base::FeatureList::IsEnabled(
      ash::features::kOnDeviceSpeechRecognition);
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

media::mojom::AvailabilityStatus IsOnDeviceSpeechRecognitionAvailable(
    const std::string& language) {
  if (!base::FeatureList::IsEnabled(media::kOnDeviceWebSpeech) ||
      !IsOnDeviceSpeechRecognitionSupported()) {
    return media::mojom::AvailabilityStatus::kUnavailable;
  }

  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  DCHECK(soda_installer);

  // Check whether the language supported.
  bool is_language_supported = false;
  speech::LanguageCode lang_code = speech::LanguageCode::kNone;
  for (auto const& available_lang : soda_installer->GetAvailableLanguages()) {
    if (l10n_util::GetLanguage(available_lang) ==
        l10n_util::GetLanguage(language)) {
      is_language_supported = true;
      lang_code = speech::GetLanguageCode(available_lang);
      break;
    }
  }

  if (!is_language_supported) {
    return media::mojom::AvailabilityStatus::kUnavailable;
  }

  if (soda_installer->IsSodaInstalled(lang_code)) {
    return media::mojom::AvailabilityStatus::kAvailable;
  }

  if (soda_installer->IsLanguageEnabled(language)) {
    // By this point the language must be either be available but not yet
    // installed or currently downloading.
    if (soda_installer->IsSodaLanguageDownloading(
            speech::GetLanguageCode(language))) {
      return media::mojom::AvailabilityStatus::kDownloading;
    }

    return media::mojom::AvailabilityStatus::kDownloadable;
  }

  return media::mojom::AvailabilityStatus::kUnavailable;
}

}  // namespace speech
