// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_ON_DEVICE_H_
#define COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_ON_DEVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "components/on_device_translation/service_controller.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"
#include "url/origin.h"

namespace on_device_translation {
class ServiceControllerManager;
}  // namespace on_device_translation

namespace captions {

class TranslationDispatcherOnDevice : public TranslationDispatcher {
 public:
  TranslationDispatcherOnDevice();
  ~TranslationDispatcherOnDevice() override;
  TranslationDispatcherOnDevice(const TranslationDispatcherOnDevice&) = delete;
  TranslationDispatcherOnDevice& operator=(
      const TranslationDispatcherOnDevice&) = delete;
  explicit TranslationDispatcherOnDevice(
      on_device_translation::ServiceControllerManager* manager);

  void GetTranslation(absl::string_view result,
                      absl::string_view source_language,
                      absl::string_view target_language,
                      TranslateEventCallback callback) override;

  void SetURLLoaderFactoryForTest(
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory);

 private:
  std::string GetBaseLanguage(const std::string& locale_code);

  void OnTranslationCreated(
      const std::string& source_language,
      const std::string& target_language,
      const std::string& result,
      TranslateEventCallback callback,
      base::expected<
          mojo::PendingRemote<on_device_translation::mojom::Translator>,
          blink::mojom::CreateTranslatorError> translator);

  void OnTranslated(TranslateEventCallback callback,
                    const std::optional<std::string>& translation);

  void OnCanTranslate(
      const std::string& source_language,
      const std::string& target_language,
      const std::string& result,
      TranslateEventCallback callback,
      blink::mojom::CanCreateTranslatorResult can_create_translator_result);

  std::string source_language_;
  std::string target_language_;

  const url::Origin origin_;

  scoped_refptr<on_device_translation::OnDeviceTranslationServiceController>
      service_controller_;
  mojo::Remote<on_device_translation::mojom::Translator> translator_;
  bool creation_in_progress_ = false;
  std::vector<std::pair<std::string, TranslateEventCallback>>
      pending_callbacks_;

  base::WeakPtrFactory<TranslationDispatcherOnDevice> weak_factory_{this};
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_ON_DEVICE_H_
