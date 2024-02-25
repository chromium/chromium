// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/language_detector.h"

#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "ui/base/l10n/l10n_util.h"

namespace quick_answers {
namespace {

constexpr double kSelectedTextConfidenceThreshold = 0.9;

constexpr double kSurroundingTextConfidenceThreshold = 0.9;

std::optional<std::string> GetLanguageWithConfidence(
    const std::vector<chromeos::machine_learning::mojom::TextLanguagePtr>&
        languages,
    double confidence_threshold) {
  // The languages are sorted according to the confidence score, from the
  // highest to the lowest (according to the mojom method documentation).
  if (!languages.empty() &&
      languages.front()->confidence > confidence_threshold) {
    return l10n_util::GetLanguage(languages.front()->locale);
  }
  return std::nullopt;
}

}  // namespace

LanguageDetector::LanguageDetector(
    chromeos::machine_learning::mojom::TextClassifier* text_classifier)
    : text_classifier_(text_classifier) {}

LanguageDetector::~LanguageDetector() = default;

void LanguageDetector::DetectLanguage(const std::string& surrounding_text,
                                      const std::string& selected_text,
                                      DetectLanguageCallback callback) {
  text_classifier_->FindLanguages(
      selected_text,
      base::BindOnce(&LanguageDetector::FindLanguagesForSelectedTextCallback,
                     weak_factory_.GetWeakPtr(), surrounding_text,
                     std::move(callback)));
}

void LanguageDetector::FindLanguagesForSelectedTextCallback(
    const std::string& surrounding_text,
    DetectLanguageCallback callback,
    std::vector<chromeos::machine_learning::mojom::TextLanguagePtr> languages) {
  auto locale = GetLanguageWithConfidence(std::move(languages),
                                          kSelectedTextConfidenceThreshold);
  if (locale.has_value()) {
    std::move(callback).Run(std::move(locale));
    return;
  }

  // If find language failed or the confidence level is too low, fall back to
  // find language for the surrounding text.
  text_classifier_->FindLanguages(
      surrounding_text,
      base::BindOnce(&LanguageDetector::FindLanguagesForSurroundingTextCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LanguageDetector::FindLanguagesForSurroundingTextCallback(
    DetectLanguageCallback callback,
    std::vector<chromeos::machine_learning::mojom::TextLanguagePtr> languages) {
  auto locale =
      GetLanguageWithConfidence(languages, kSurroundingTextConfidenceThreshold);

  std::move(callback).Run(std::move(locale));
}

}  // namespace quick_answers
