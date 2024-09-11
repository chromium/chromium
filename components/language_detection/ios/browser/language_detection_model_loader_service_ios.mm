// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/ios/browser/language_detection_model_loader_service_ios.h"

#include <memory>

#import "base/task/sequenced_task_runner.h"
#include "components/language_detection/core/browser/language_detection_model_service.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "components/translate/core/language_detection/language_detection_model.h"

namespace language_detection {

class LanguageDetectionModelContainer
    // TODO(https://crbug.com/356380874): Clarify the thread safety of this
    // class. The TFLite model should not be accessed from multiple threads,
    // so it should not be destroyed from another thread.
    : public base::RefCountedThreadSafe<LanguageDetectionModelContainer>,
      public translate::LanguageDetectionModel {
 public:
  LanguageDetectionModelContainer()
      : translate::LanguageDetectionModel(
            &language_detection::GetLanguageDetectionModel()) {}

 private:
  // Allow destruction by RefCounted<>.
  friend class RefCountedThreadSafe<LanguageDetectionModelContainer>;
  // Destructor must be private/protected.
  ~LanguageDetectionModelContainer() = default;
};

namespace {
void SetLanguageDetectionModelModelFile(
    scoped_refptr<LanguageDetectionModelContainer> language_detection_model,
    base::File model_file) {
  language_detection_model->UpdateWithFile(std::move(model_file));
}
}  // namespace

LanguageDetectionModelLoaderServiceIOS::LanguageDetectionModelLoaderServiceIOS(
    language_detection::LanguageDetectionModelService*
        language_detection_model_service,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : language_detection_model_service_(language_detection_model_service),
      background_task_runner_(background_task_runner),
      language_detection_model_(
          base::MakeRefCounted<LanguageDetectionModelContainer>()) {
  if (language_detection_model_service_) {
    if (!language_detection_model_->IsAvailable()) {
      language_detection_model_service_->NotifyOnModelFileAvailable(
          base::BindOnce(&LanguageDetectionModelLoaderServiceIOS::
                             OnLanguageModelFileAvailabilityChanged,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      OnLanguageModelFileAvailabilityChanged(true);
    }
  }
}

LanguageDetectionModelLoaderServiceIOS::
    ~LanguageDetectionModelLoaderServiceIOS() {}

translate::LanguageDetectionModel*
LanguageDetectionModelLoaderServiceIOS::GetLanguageDetectionModel() {
  return language_detection_model_.get();
}

bool LanguageDetectionModelLoaderServiceIOS::IsModelAvailable() {
  return language_detection_model_->IsAvailable();
}

void LanguageDetectionModelLoaderServiceIOS::
    OnLanguageModelFileAvailabilityChanged(bool available) {
  if (available) {
    DCHECK(language_detection_model_service_);
    base::File model_file =
        language_detection_model_service_->GetLanguageDetectionModelFile();
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SetLanguageDetectionModelModelFile,
                       language_detection_model_, std::move(model_file)));
  }
}

}  // namespace language_detection
