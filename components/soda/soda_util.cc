// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/soda_util.h"

#include <string>

#include "base/feature_list.h"
#include "base/i18n/legacy_language_tag_helpers.h"
#include "build/build_config.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(ARCH_CPU_X86_FAMILY)
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
  // On-Device API (SODA) library. SODA requires AVX support on x86.
  static bool has_avx = base::CPU().has_avx();
  return has_avx;
#else
  // Other architectures are not supported.
  return false;
#endif  // defined(ARCH_CPU_X86_FAMILY)
}
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
bool IsSupportedMac() {
#if defined(ARCH_CPU_X86_FAMILY)
  // Check if the CPU has the required instruction set to run the Speech
  // On-Device API (SODA) library. SODA requires AVX support on x86.
  // Pre-2011 Intel Macs (e.g. Penryn, Nehalem, Westmere) running modern
  // macOS via OpenCore Legacy Patcher lack AVX. Without AVX, SODA falls
  // back to a gemmlowp kernel that crashes.
  static bool has_avx = base::CPU().has_avx();
  return has_avx;
#elif defined(ARCH_CPU_ARM_FAMILY)
  return true;
#else
  return false;
#endif  // defined(ARCH_CPU_X86_FAMILY)
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
bool IsSupportedWin() {
#if defined(ARCH_CPU_X86_FAMILY)
  // SODA requires AVX support on x86.
  static bool has_avx = base::CPU().has_avx();
  return has_avx;
#else
  return true;
#endif  // defined(ARCH_CPU_X86_FAMILY)
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

bool IsOnDeviceSpeechRecognitionSupported() {
  // TODO(crbug.com/446260680): Disable on-device speech recognition if the
  // OnDeviceWebSpeechGeminiNano feature flag is enabled and the device doesn't
  // support Gemini Nano.
#if BUILDFLAG(IS_CHROMEOS)
  return IsSupportedChromeOS();
#elif BUILDFLAG(IS_LINUX)
  return IsSupportedLinux();
#elif BUILDFLAG(IS_MAC)
  return IsSupportedMac();
#elif BUILDFLAG(IS_WIN)
  return IsSupportedWin();
#else
  return false;
#endif
}

media::mojom::AvailabilityStatus GetSodaAvailabilityStatus(
    std::string_view language) {
  if (!base::FeatureList::IsEnabled(media::kOnDeviceWebSpeech) ||
      !IsOnDeviceSpeechRecognitionSupported()) {
    return media::mojom::AvailabilityStatus::kUnavailable;
  }

  // The SODA installer might not be available in tests.
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (!soda_installer) {
    return media::mojom::AvailabilityStatus::kUnavailable;
  }

  // Check whether the language supported.
  bool is_language_supported = false;
  speech::LanguageCode lang_code = speech::LanguageCode::kNone;
  for (auto const& available_lang : soda_installer->GetAvailableLanguages()) {
    if (base::i18n::GetLanguageSubtagUsingLanguageTag(available_lang) ==
        base::i18n::GetLanguageSubtagUsingLanguageTag(language)) {
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
