// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/service_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace on_device_translation {

// Returns the Accept Languages for a given Profile.
const std::vector<std::string_view> GetAcceptLanguages(
    content::BrowserContext* browser_context);

// Determines if a given language is in the Accept languages.
bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang);

// Determines if the Translator API is enabled.
bool IsTranslatorAllowed(content::BrowserContext* browser_context);

// Implementation of LookupMatchingLocaleByBestFit
// (https://tc39.es/ecma402/#sec-lookupmatchinglocalebybestfit) as
// LookupMatchingLocaleByPrefix
// (https://tc39.es/ecma402/#sec-lookupmatchinglocalebyprefix) assuming
// `available_languages` contains no extension.
template <typename SetType>
std::optional<std::string> LookupMatchingLocaleByBestFit(
    const SetType& available_languages,
    std::string_view requested_language) {
  std::string_view prefix = requested_language;
  while (prefix != "") {
    if (available_languages.contains(prefix)) {
      return std::string(prefix);
    }
    int pos = prefix.rfind('-');
    if (pos == -1) {
      break;
    }
    prefix = prefix.substr(0, pos);
  }
  return std::nullopt;
}

// Returns the supported language code that best fits language_code, or nullopt
// if none fits. Maps he to iw, and maps Chinese region codes to the script of
// that region, such as zh-TW to zh-Hant.
std::optional<std::string> GetBestFitLanguageCode(
    std::string_view language_code);

// Computes the required, missing, and unregistered language packs for
// translating between `source_lang` and `target_lang`.
LanguagePackRequirements GetLanguagePackRequirements(
    const OnDeviceTranslationInstaller& installer,
    const std::string& source_lang,
    const std::string& target_lang);

// Checks whether `installer` can translate from `source_lang` to `target_lang`
// without requiring an `OnDeviceTranslationServiceController`.
void CanTranslateWithInstaller(
    OnDeviceTranslationInstaller& installer,
    const std::string& source_lang,
    const std::string& target_lang,
    OnDeviceTranslationController::CanTranslateCallback callback);

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_
