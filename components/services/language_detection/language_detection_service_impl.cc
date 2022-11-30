// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/language_detection/language_detection_service_impl.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/translate/core/language_detection/language_detection_util.h"

namespace language_detection {

LanguageDetectionServiceImpl::LanguageDetectionServiceImpl(
    mojo::PendingReceiver<mojom::LanguageDetectionService> receiver)
    : receiver_(this, std::move(receiver)) {}

LanguageDetectionServiceImpl::~LanguageDetectionServiceImpl() = default;

void LanguageDetectionServiceImpl::DetermineLanguage(
    const ::std::u16string& text,
    DetermineLanguageCallback callback) {
  bool is_model_reliable = false;
  float model_reliability_score = 0.0;
  std::string model_detected_language = translate::DetermineTextLanguage(
      base::UTF16ToUTF8(text), &is_model_reliable, model_reliability_score);
  std::move(callback).Run(model_detected_language, is_model_reliable);
}

}  // namespace language_detection
