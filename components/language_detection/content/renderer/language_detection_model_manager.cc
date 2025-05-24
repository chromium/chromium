// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/content/renderer/language_detection_model_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/language_detection/core/features.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace language_detection {

LanguageDetectionModelManager::LanguageDetectionModelManager(
    LanguageDetectionModel& model)
    : language_detection_model_(model) {}

LanguageDetectionModelManager::~LanguageDetectionModelManager() = default;

// static
void LanguageDetectionModelManager::OnModelFileAvailable(
    base::WeakPtr<LanguageDetectionModelManager> manager,
    LanguageDetectionModelManager::GetLanuageDetectionModelCallback callback,
    base::File model_file) {
  if (!manager) {
    // Destroy the file we have received on a background thread.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce([](base::File) {}, std::move(model_file)));
    std::move(callback).Run(nullptr);
  }
  manager->UpdateLanguageDetectionModel(
      std::move(model_file),
      base::BindOnce(&LanguageDetectionModelManager::OnModelFileLoaded, manager,
                     std::move(callback)));
}

void LanguageDetectionModelManager::OnModelFileLoaded(
    LanguageDetectionModelManager::GetLanuageDetectionModelCallback callback) {
  std::move(callback).Run(&language_detection_model_.get());
}

void LanguageDetectionModelManager::GetLanguageDetectionModel(
    const blink::BrowserInterfaceBrokerProxy& interface_broker,
    GetLanuageDetectionModelCallback callback) {
  if (language_detection_model_->IsAvailable()) {
    std::move(callback).Run(&language_detection_model_.get());
    return;
  }

  // There is no valid model, so request a model file from the browser.
  // This is complicated by the fact that the IPC returns a file and we must be
  // careful about that file's destruction if `this` has been destroyed by the
  // time the IPC reply arrives.
  GetLanguageDetectionDriver(interface_broker)
      ->GetLanguageDetectionModel(base::BindOnce(
          LanguageDetectionModelManager::OnModelFileAvailable,
          weak_pointer_factory_.GetWeakPtr(), std::move(callback)));
}

void LanguageDetectionModelManager::GetLanguageDetectionModelStatus(
    const blink::BrowserInterfaceBrokerProxy& interface_broker,
    GetLanguageDetectionModelStatusCallback callback) {
  GetLanguageDetectionDriver(interface_broker)
      ->GetLanguageDetectionModelStatus(std::move(callback));
}

mojo::Remote<mojom::ContentLanguageDetectionDriver>&
LanguageDetectionModelManager::GetLanguageDetectionDriver(
    const blink::BrowserInterfaceBrokerProxy& interface_broker) {
  if (language_detection_driver_) {
    return language_detection_driver_;
  }

  interface_broker.GetInterface(
      language_detection_driver_.BindNewPipeAndPassReceiver());
  return language_detection_driver_;
}

void LanguageDetectionModelManager::UpdateLanguageDetectionModel(
    base::File model_file,
    base::OnceClosure callback) {
  TRACE_EVENT("browser",
              "LanguageDetectionModelManager::UpdateLanguageDetectionModel");
  base::ScopedUmaHistogramTimer timer(
      "LanguageDetection.TFLiteModel.UpdateLanaguageDetectionModelTime");

  // When enabled, we postpone updating the language detection model to
  // avoid congesting the render main thread during navigation critical
  // timing (crbug.com/361215212).
  if (base::FeatureList::IsEnabled(
          language_detection::features::kLazyUpdateTranslateModel)) {
    language_detection_model_->UpdateWithFileAsync(std::move(model_file),
                                                   std::move(callback));
  } else {
    language_detection_model_->UpdateWithFile(std::move(model_file));
    std::move(callback).Run();
  }
}
}  // namespace language_detection
