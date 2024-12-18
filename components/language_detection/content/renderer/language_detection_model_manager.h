// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_MODEL_MANAGER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_MODEL_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/language_detection/content/common/language_detection.mojom.h"
#include "components/language_detection/core/language_detection_model.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace language_detection {

// Provides a language detection model. It tries to ensure that it has a valid
// model file loaded by requesting one from the browser.
class LanguageDetectionModelManager {
 public:
  // `model` is not owned by and must outlive `this`.
  explicit LanguageDetectionModelManager(LanguageDetectionModel& model);

  LanguageDetectionModelManager(const LanguageDetectionModelManager&) = delete;
  LanguageDetectionModelManager& operator=(
      const LanguageDetectionModelManager&) = delete;

  ~LanguageDetectionModelManager();

  // `nullptr` indicates that no valid model could be loaded.
  using GetLanuageDetectionModelCallback =
      base::OnceCallback<void(LanguageDetectionModel* model)>;
  using GetLanguageDetectionModelStatusCallback = language_detection::mojom::
      ContentLanguageDetectionDriver::GetLanguageDetectionModelStatusCallback;

  // Passes a model to `callback`. If no valid model can possibly be loaded, an
  // invalid model will be passed. If the current model is valid, this will
  // succeed immediately. If it's not valid, it will request a model via the
  // `interface_broker` and call `callback` when that response arrives. This may
  // still result in no valid model.
  void GetLanguageDetectionModel(
      const blink::BrowserInterfaceBrokerProxy& interface_broker,
      GetLanuageDetectionModelCallback callback);

  // Checks if the model has been downloaded in the browser process.
  void GetLanguageDetectionModelStatus(
      const blink::BrowserInterfaceBrokerProxy& interface_broker,
      GetLanguageDetectionModelStatusCallback callback);

 private:
  // Ensures that the driver is connected before returning it.
  mojo::Remote<mojom::ContentLanguageDetectionDriver>&
  GetLanguageDetectionDriver(
      const blink::BrowserInterfaceBrokerProxy& interface_broker);

  // The callback to receive the language detection model file.
  void UpdateLanguageDetectionModel(base::File model_file,
                                    base::OnceClosure callback);

  // Callback for when the browser returns a model file.
  static void OnModelFileAvailable(
      base::WeakPtr<LanguageDetectionModelManager> manager,
      LanguageDetectionModelManager::GetLanuageDetectionModelCallback callback,
      base::File model_file);

  // Callback for when the returned model file has been loaded.
  void OnModelFileLoaded(
      LanguageDetectionModelManager::GetLanuageDetectionModelCallback callback);

  mojo::Remote<mojom::ContentLanguageDetectionDriver>
      language_detection_driver_;

  const raw_ref<language_detection::LanguageDetectionModel>
      language_detection_model_;

  base::WeakPtrFactory<LanguageDetectionModelManager> weak_pointer_factory_{
      this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_RENDERER_LANGUAGE_DETECTION_MODEL_MANAGER_H_
