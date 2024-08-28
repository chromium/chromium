// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/browser/content_language_detection_driver.h"

#include <memory>

#include "components/translate/core/browser/translate_model_service.h"

namespace language_detection {

ContentLanguageDetectionDriver::ContentLanguageDetectionDriver(
    translate::TranslateModelService* translate_model_service)
    : translate_model_service_(translate_model_service) {}

ContentLanguageDetectionDriver::~ContentLanguageDetectionDriver() = default;

void ContentLanguageDetectionDriver::AddReceiver(
    mojo::PendingReceiver<mojom::ContentLanguageDetectionDriver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ContentLanguageDetectionDriver::GetLanguageDetectionModel(
    GetLanguageDetectionModelCallback callback) {
  if (!translate_model_service_) {
    std::move(callback).Run(base::File());
    return;
  }
  // If the model file is not available, request the translate model service
  // notify `this` when it is. The two-step process is to ensure that
  // the model file and callback lifetimes are carefully managed so they
  // are not freed without be handled on the appropriate thread, particularly
  // for the model file.
  if (!translate_model_service_->IsModelAvailable()) {
    translate_model_service_->NotifyOnModelFileAvailable(base::BindOnce(
        &ContentLanguageDetectionDriver::OnLanguageModelFileAvailabilityChanged,
        weak_pointer_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  OnLanguageModelFileAvailabilityChanged(std::move(callback), true);
}

void ContentLanguageDetectionDriver::OnLanguageModelFileAvailabilityChanged(
    GetLanguageDetectionModelCallback callback,
    bool is_available) {
  if (!is_available) {
    std::move(callback).Run(base::File());
    return;
  }
  std::move(callback).Run(
      translate_model_service_->GetLanguageDetectionModelFile());
}

}  // namespace language_detection
