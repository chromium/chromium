// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/language_detector.h"

#include "third_party/cld_3/src/src/nnet_language_identifier.h"

namespace chromeos {
namespace quick_answers {
namespace {

using chrome_lang_id::NNetLanguageIdentifier;

const int kMinNumBytes = 0;
const int kMaxNumBytes = 1000;

}  // namespace

LanguageDetector::LanguageDetector() {
  lang_id_ =
      std::make_unique<NNetLanguageIdentifier>(kMinNumBytes, kMaxNumBytes);
}

LanguageDetector::~LanguageDetector() = default;

std::string LanguageDetector::DetectLanguage(const std::string& text) {
  const NNetLanguageIdentifier::Result result = lang_id_->FindLanguage(text);

  std::string language;
  if (result.is_reliable)
    language = result.language;

  return language;
}

}  // namespace quick_answers
}  // namespace chromeos
