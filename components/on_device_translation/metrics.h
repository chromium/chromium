// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_METRICS_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_METRICS_H_

#include <string_view>

namespace on_device_translation {

// Record language code for a certain UMA. If the language code is not
// recognized it will be recorded as unknown.
void RecordLanguageUma(std::string_view uma_name,
                       std::string_view language_code);

// Record language code for a certain UMA.
// The source_lang and target_lang will be converted to the integer value
// of the LanguageCode enumeration and then concatenated to a single integer
// value.
void RecordLanguagePairUma(std::string_view uma_name,
                           std::string_view source_lang,
                           std::string_view target_lang);

// The following UMAs will be recorded:
// Translate.OnDeviceTranslation.${api_name}.SourceLanguage
// Translate.OnDeviceTranslation.${api_name}.TargetLanguage
// Translate.OnDeviceTranslation.${api_name}.LanguagePair
void RecordTranslationAPICallForLanguagePair(std::string_view api_name,
                                             std::string_view source_lang,
                                             std::string_view target_lang);

// Record the character count UMA for:
// Translate.OnDeviceTranslation.CharacterCount
// Translate.OnDeviceTranslation.Source.${source_lang}.CharacterCount
// Translate.OnDeviceTranslation.Target.${target_lang}.CharacterCount
void RecordTranslationCharacterCount(std::string_view source_lang,
                                     std::string_view target_lang,
                                     int character_count);

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_METRICS_H_
