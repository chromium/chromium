// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/browser/content_language_detection_driver.h"

#include <memory>

#include "components/language_detection/core/browser/language_detection_model_service.h"

namespace language_detection {

ContentLanguageDetectionDriver::ContentLanguageDetectionDriver(
    LanguageDetectionModelService* language_detection_model_service)
    : language_detection_model_service_(language_detection_model_service) {}

ContentLanguageDetectionDriver::~ContentLanguageDetectionDriver() = default;

void ContentLanguageDetectionDriver::AddReceiver(
    mojo::PendingReceiver<mojom::ContentLanguageDetectionDriver> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ContentLanguageDetectionDriver::GetLanguageDetectionModel(
    GetLanguageDetectionModelCallback callback) {
  if (!language_detection_model_service_) {
    std::move(callback).Run(base::File());
    return;
  }

  language_detection_model_service_->GetLanguageDetectionModelFile(
      std::move(callback));
}

}  // namespace language_detection
