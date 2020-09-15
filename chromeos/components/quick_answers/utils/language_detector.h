// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_LANGUAGE_DETECTOR_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_LANGUAGE_DETECTOR_H_

#include <memory>
#include <string>

namespace chrome_lang_id {
class NNetLanguageIdentifier;
}  // namespace chrome_lang_id

namespace chromeos {
namespace quick_answers {

// Utility class for language detection.
// TODO(b/168541952): Cleanup this class after the new language detection API
// becomes stable.
class LanguageDetector {
 public:
  LanguageDetector();
  LanguageDetector(const LanguageDetector&) = delete;
  LanguageDetector& operator=(const LanguageDetector&) = delete;
  virtual ~LanguageDetector();

  // Returns the ISO 639 language code of the specified |text|, or empty string
  // if it failed. Virtual for testing.
  virtual std::string DetectLanguage(const std::string& text);

 private:
  std::unique_ptr<chrome_lang_id::NNetLanguageIdentifier> lang_id_;
};

}  // namespace quick_answers
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_LANGUAGE_DETECTOR_H_
