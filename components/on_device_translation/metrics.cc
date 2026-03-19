// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/metrics.h"

#include <sstream>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/on_device_translation/public/language_pack.h"

namespace on_device_translation {
namespace {

constexpr int kLanguagePairOffset = 1000;
constexpr char kTranslatorApiName[] = "TranslatorApi";
constexpr char kOnDeviceTranslationName[] = "OnDeviceTranslation";

static_assert(static_cast<int>(SupportedLanguage::kMaxValue) <
              kLanguagePairOffset);

std::string GetSourceUMANameForAPI(std::string_view product_name,
                                   std::string_view action_name) {
  return base::StrCat(
      {"Translate.", product_name, ".", action_name, ".SourceLanguage"});
}

std::string GetTargetUMANameForAPI(std::string_view product_name,
                                   std::string_view action_name) {
  return base::StrCat(
      {"Translate.", product_name, ".", action_name, ".TargetLanguage"});
}

std::string GetPairUMANameForAPI(std::string_view product_name,
                                 std::string_view action_name) {
  return base::StrCat(
      {"Translate.", product_name, ".", action_name, ".LanguagePair"});
}

std::string GetTranslationLengthUMAForSourceLanguage(
    std::string_view product_name,
    std::string_view source_lang) {
  return base::StrCat({"Translate.", product_name, ".SourceLanguage.",
                       source_lang, ".TranslationLength"});
}

std::string GetTranslationLengthUMAForTargetLanguage(
    std::string_view product_name,
    std::string_view target_lang) {
  return base::StrCat({"Translate.", product_name, ".TargetLanguage.",
                       target_lang, ".TranslationLength"});
}

void RecordCallForLanguagePair(std::string_view product_name,
                               std::string_view action_name,
                               std::string_view source_lang,
                               std::string_view target_lang) {
  if (!ToSupportedLanguage(source_lang).has_value() ||
      !ToSupportedLanguage(target_lang).has_value()) {
    return;
  }
  RecordLanguageUma(GetSourceUMANameForAPI(product_name, action_name),
                    source_lang);
  RecordLanguageUma(GetTargetUMANameForAPI(product_name, action_name),
                    target_lang);
  RecordLanguagePairUma(GetPairUMANameForAPI(product_name, action_name),
                        source_lang, target_lang);
}

void RecordTranslationLength(std::string_view product_name,
                             std::string_view source_lang,
                             std::string_view target_lang,
                             size_t length) {
  // The translate() API call requires the source and target language to be
  // supported.
  CHECK(ToSupportedLanguage(source_lang).has_value());
  CHECK(ToSupportedLanguage(target_lang).has_value());
  base::UmaHistogramCounts1M(
      base::StrCat({"Translate.", product_name, ".TranslationLength"}), length);
  base::UmaHistogramCounts1M(
      GetTranslationLengthUMAForSourceLanguage(product_name, source_lang),
      length);
  base::UmaHistogramCounts1M(
      GetTranslationLengthUMAForTargetLanguage(product_name, target_lang),
      length);
}

}  // namespace

void RecordOnDeviceTranslationSupportedSourceLanguage(
    std::string_view action_name,
    bool is_supported) {
  base::UmaHistogramBoolean(
      base::StrCat({"Translate.OnDeviceTranslation.", action_name,
                    ".IsSourceLanguageSupported"}),
      is_supported);
}

void RecordOnDeviceTranslationSupportedTargetLanguage(
    std::string_view action_name,
    bool is_supported) {
  base::UmaHistogramBoolean(
      base::StrCat({"Translate.OnDeviceTranslation.", action_name,
                    ".IsTargetLanguageSupported"}),
      is_supported);
}

void RecordLanguageUma(std::string_view uma_name,
                       std::string_view language_code) {
  std::optional<SupportedLanguage> code = ToSupportedLanguage(language_code);
  if (code.has_value()) {
    base::UmaHistogramEnumeration(uma_name, code.value());
  }
}

void RecordLanguagePairUma(std::string_view uma_name,
                           std::string_view source_lang,
                           std::string_view target_lang) {
  std::optional<SupportedLanguage> source_code =
      ToSupportedLanguage(source_lang);
  std::optional<SupportedLanguage> target_code =
      ToSupportedLanguage(target_lang);
  if (source_code.has_value() && target_code.has_value()) {
    base::UmaHistogramSparse(
        uma_name, static_cast<int>(source_code.value()) * kLanguagePairOffset +
                      static_cast<int>(target_code.value()));
  }
}
void RecordOnDeviceTranslationCallForLanguagePair(
    std::string_view action_name,
    std::string_view source_lang,
    std::string_view target_lang) {
  RecordCallForLanguagePair(kOnDeviceTranslationName, action_name, source_lang,
                            target_lang);
}

void RecordTranslatorApiCallForLanguagePair(std::string_view action_name,
                                            std::string_view source_lang,
                                            std::string_view target_lang) {
  RecordCallForLanguagePair(kTranslatorApiName, action_name, source_lang,
                            target_lang);
}

void RecordTranslatorApiTranslationLength(std::string_view source_lang,
                                          std::string_view target_lang,
                                          size_t length) {
  RecordTranslationLength(kTranslatorApiName, source_lang, target_lang, length);
}

void RecordOnDeviceTranslationLength(std::string_view source_lang,
                                     std::string_view target_lang,
                                     size_t length) {
  RecordTranslationLength(kOnDeviceTranslationName, source_lang, target_lang,
                          length);
}

}  // namespace on_device_translation
