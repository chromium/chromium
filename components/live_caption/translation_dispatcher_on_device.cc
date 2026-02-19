// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/translation_dispatcher_on_device.h"

#include "base/types/expected.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/on_device_translation/service_controller.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "url/gurl.h"

namespace captions {

using blink::mojom::CanCreateTranslatorResult;

TranslationDispatcherOnDevice::TranslationDispatcherOnDevice() = default;

TranslationDispatcherOnDevice::TranslationDispatcherOnDevice(
    on_device_translation::ServiceControllerManager* manager)
    : origin_(url::Origin()) {
  CHECK(manager);
  service_controller_ = manager->GetServiceControllerForOrigin(origin_);
}

TranslationDispatcherOnDevice::~TranslationDispatcherOnDevice() = default;

void TranslationDispatcherOnDevice::GetTranslation(
    absl::string_view result,
    absl::string_view source_language,
    absl::string_view target_language,
    TranslateEventCallback callback) {
  std::string base_source_language =
      GetBaseLanguage(std::string(source_language));
  std::string base_target_language =
      GetBaseLanguage(std::string(target_language));

  if (translator_.is_bound() && source_language_ == base_source_language &&
      target_language_ == base_target_language) {
    translator_->Translate(
        std::string(result),
        base::BindOnce(&TranslationDispatcherOnDevice::OnTranslated,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  if (creation_in_progress_) {
    pending_callbacks_.emplace_back(std::string(result), std::move(callback));
    return;
  }

  translator_.reset();
  creation_in_progress_ = true;

  service_controller_->CanTranslate(
      base_source_language, base_target_language,
      base::BindOnce(&TranslationDispatcherOnDevice::OnCanTranslate,
                     weak_factory_.GetWeakPtr(), base_source_language,
                     base_target_language, std::string(result),
                     std::move(callback)));
}

std::string TranslationDispatcherOnDevice::GetBaseLanguage(
    const std::string& locale_code) {
  size_t hyphen_pos = locale_code.find('-');
  if (hyphen_pos != std::string::npos) {
    return locale_code.substr(0, hyphen_pos);
  }
  return locale_code;
}

void TranslationDispatcherOnDevice::OnCanTranslate(
    const std::string& source_language,
    const std::string& target_language,
    const std::string& result,
    TranslateEventCallback callback,
    CanCreateTranslatorResult can_create_translator_result) {
  switch (can_create_translator_result) {
    case CanCreateTranslatorResult::kReadily:
    case CanCreateTranslatorResult::kAfterDownloadLibraryNotReady:
    case CanCreateTranslatorResult::kAfterDownloadLanguagePackNotReady:
    case CanCreateTranslatorResult::
        kAfterDownloadLibraryAndLanguagePackNotReady:
    case CanCreateTranslatorResult::kAfterDownloadTranslatorCreationRequired:
      service_controller_->CreateTranslator(
          source_language, target_language,
          base::BindOnce(&TranslationDispatcherOnDevice::OnTranslationCreated,
                         weak_factory_.GetWeakPtr(), source_language,
                         target_language, result, std::move(callback)));
      return;
    case CanCreateTranslatorResult::kNoNotSupportedLanguage:
    case CanCreateTranslatorResult::kNoServiceCrashed:
    case CanCreateTranslatorResult::kNoDisallowedByPolicy:
    case CanCreateTranslatorResult::kNoExceedsServiceCountLimitation:
    case CanCreateTranslatorResult::kNoInvalidStoragePartition:
      creation_in_progress_ = false;
      std::move(callback).Run(base::unexpected("Failed to create translator"));
      for (auto& pending_callback : pending_callbacks_) {
        std::move(pending_callback.second)
            .Run(base::unexpected("Failed to create translator"));
      }
      pending_callbacks_.clear();
      return;
  }
}

void TranslationDispatcherOnDevice::OnTranslationCreated(
    const std::string& source_language,
    const std::string& target_language,
    const std::string& result,
    TranslateEventCallback callback,
    base::expected<
        mojo::PendingRemote<on_device_translation::mojom::Translator>,
        blink::mojom::CreateTranslatorError> translator) {
  creation_in_progress_ = false;
  if (!translator.has_value()) {
    std::move(callback).Run(base::unexpected("Failed to create translator"));
    for (auto& pending_callback : pending_callbacks_) {
      std::move(pending_callback.second)
          .Run(base::unexpected("Failed to create translator"));
    }
    pending_callbacks_.clear();
    return;
  }
  source_language_ = source_language;
  target_language_ = target_language;
  translator_.Bind(std::move(translator.value()));
  translator_->Translate(
      result, base::BindOnce(&TranslationDispatcherOnDevice::OnTranslated,
                             weak_factory_.GetWeakPtr(), std::move(callback)));

  for (auto& pending_callback : pending_callbacks_) {
    translator_->Translate(
        pending_callback.first,
        base::BindOnce(&TranslationDispatcherOnDevice::OnTranslated,
                       weak_factory_.GetWeakPtr(),
                       std::move(pending_callback.second)));
  }
  pending_callbacks_.clear();
}

void TranslationDispatcherOnDevice::OnTranslated(
    TranslateEventCallback callback,
    const std::optional<std::string>& translation) {
  if (!translation) {
    std::move(callback).Run(base::unexpected("Failed to get translation"));
    return;
  }
  std::move(callback).Run(base::ok(translation.value()));
}

}  // namespace captions
