// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_LANGUAGE_DETECTOR_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_LANGUAGE_DETECTOR_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/machine_learning/public/mojom/text_classifier.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace quick_answers {

// Utility class for language detection.
class LanguageDetector {
 public:
  using DetectLanguageCallback =
      base::OnceCallback<void(absl::optional<std::string>)>;

  explicit LanguageDetector(
      chromeos::machine_learning::mojom::TextClassifier* text_classifier);

  LanguageDetector(const LanguageDetector&) = delete;
  LanguageDetector& operator=(const LanguageDetector&) = delete;

  ~LanguageDetector();

  // Returns language code of the specified |selected_text|.
  // Fall back to language code of |surrounding_text| if the confidence level is
  // not high enough.
  // Returns no value if no language can be detected.
  void DetectLanguage(const std::string& surrounding_text,
                      const std::string& selected_text,
                      DetectLanguageCallback callback);

 private:
  void FindLanguagesForSelectedTextCallback(
      const std::string& surrounding_text,
      DetectLanguageCallback callback,
      std::vector<chromeos::machine_learning::mojom::TextLanguagePtr>
          languages);

  void FindLanguagesForSurroundingTextCallback(
      DetectLanguageCallback callback,
      std::vector<chromeos::machine_learning::mojom::TextLanguagePtr>
          languages);

  // Owned by IntentGenerator.
  raw_ptr<chromeos::machine_learning::mojom::TextClassifier> text_classifier_ =
      nullptr;

  base::WeakPtrFactory<LanguageDetector> weak_factory_{this};
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_LANGUAGE_DETECTOR_H_
