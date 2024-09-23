// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/translate/core/language_detection/language_detection_util.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace {

void DeterminePageLanguageDoesNotCrash(std::string code,
                                       std::string html_lang,
                                       std::u16string content) {
  // Output parameters.
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score;

  // Fuzzed function call:
  translate::DeterminePageLanguage(code, html_lang, content,
                                   &model_detected_language, &is_model_reliable,
                                   model_reliability_score);
}

// Note: Once chromium support adding fuzztests in unittests, this file could
// be merged with the corresponding unittest file.
FUZZ_TEST(LanguageDetectionUtilFuzzTest, DeterminePageLanguageDoesNotCrash);

}  // namespace
