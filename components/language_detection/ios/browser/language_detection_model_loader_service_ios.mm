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

// Container class owning both a language_detection::LanguageDetectionModel
// and a translate::LanguageDetectionModel and orchestrating their lifetime.
class LanguageDetectionModelLoaderServiceIOS::ModelContainer {
 public:
  ModelContainer() = default;

  ModelContainer(const ModelContainer&) = delete;
  ModelContainer& operator=(const ModelContainer&) = delete;

  ~ModelContainer() = default;

  translate::LanguageDetectionModel* model() { return &outer_model_; }

  bool IsAvailable() const { return outer_model_.IsAvailable(); }

  void UpdateWithFileAsync(base::File model_file, base::OnceClosure callback) {
    inner_model_.UpdateWithFileAsync(std::move(model_file), base::DoNothing());
  }

 private:
  language_detection::LanguageDetectionModel inner_model_;
  translate::LanguageDetectionModel outer_model_{&inner_model_};
};

LanguageDetectionModelLoaderServiceIOS::LanguageDetectionModelLoaderServiceIOS(
    language_detection::LanguageDetectionModelService*
        language_detection_model_service)
    : language_detection_model_service_(language_detection_model_service),
      model_container_(std::make_unique<ModelContainer>()) {
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
  return model_container_->model();
}

bool LanguageDetectionModelLoaderServiceIOS::IsModelAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_container_->IsAvailable();
}

void LanguageDetectionModelLoaderServiceIOS::
    OnLanguageDetectionModelFileReceived(base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_container_->UpdateWithFileAsync(std::move(model_file),
                                        base::DoNothing());
}

}  // namespace language_detection
