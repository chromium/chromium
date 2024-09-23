// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/ios/browser/language_detection_model_loader_service_ios.h"

#include <memory>

#include "components/language_detection/core/browser/language_detection_model_service.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/core/language_detection/language_detection_model.h"

namespace language_detection {

LanguageDetectionModelLoaderServiceIOS::LanguageDetectionModelLoaderServiceIOS(
    language_detection::LanguageDetectionModelService*
        language_detection_model_service)
    : language_detection_model_service_(language_detection_model_service),
      language_detection_model_(
          std::make_unique<translate::LanguageDetectionModel>(
              std::make_unique<language_detection::LanguageDetectionModel>())) {
  if (language_detection_model_service_) {
    language_detection_model_service_->GetLanguageDetectionModelFile(
        base::BindOnce(&LanguageDetectionModelLoaderServiceIOS::
                           OnLanguageDetectionModelFileReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

LanguageDetectionModelLoaderServiceIOS::
    ~LanguageDetectionModelLoaderServiceIOS() = default;

translate::LanguageDetectionModel*
LanguageDetectionModelLoaderServiceIOS::GetLanguageDetectionModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return language_detection_model_.get();
}

bool LanguageDetectionModelLoaderServiceIOS::IsModelAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return language_detection_model_->IsAvailable();
}

void LanguageDetectionModelLoaderServiceIOS::
    OnLanguageDetectionModelFileReceived(base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  language_detection_model_->UpdateWithFileAsync(std::move(model_file),
                                                 base::DoNothing());
}

}  // namespace language_detection
