// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_CONTENT_LANGUAGE_DETECTION_DRIVER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_CONTENT_LANGUAGE_DETECTION_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/language_detection/content/common/language_detection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace language_detection {

class LanguageDetectionModelService;

// Content implementation of LanguageDetectionDriver.
class ContentLanguageDetectionDriver
    : public mojom::ContentLanguageDetectionDriver {
 public:
  explicit ContentLanguageDetectionDriver(
      LanguageDetectionModelService* language_detection_model_service);

  ContentLanguageDetectionDriver(const ContentLanguageDetectionDriver&) =
      delete;
  ContentLanguageDetectionDriver& operator=(
      const ContentLanguageDetectionDriver&) = delete;

  ~ContentLanguageDetectionDriver() override;

  // Adds a receiver in `receivers_` for the passed `receiver`.
  void AddReceiver(
      mojo::PendingReceiver<mojom::ContentLanguageDetectionDriver> receiver);

  // translate::mojom::ContentTranslateDriver implementation:
  void GetLanguageDetectionModel(
      GetLanguageDetectionModelCallback callback) override;

 protected:
  // Notifies `this` that the translate model service is available for model
  // requests or is invalidating existing requests specified by `is_available`.
  // `callback` will be either forwarded to a request to get the actual model
  // file or will be run with an empty file if the translate model service is
  // rejecting requests.
  void OnLanguageModelFileAvailabilityChanged(
      GetLanguageDetectionModelCallback callback,
      bool is_available);

  // ContentTranslateDriver is a singleton per web contents but multiple render
  // frames may be contained in a single web contents. TranslateAgents get the
  // other end of this receiver in the form of a ContentTranslateDriver.
  mojo::ReceiverSet<language_detection::mojom::ContentLanguageDetectionDriver>
      receivers_;

  // The service that provides the model files needed for translate. Not owned
  // but guaranteed to outlive `this`.
  const raw_ptr<LanguageDetectionModelService>
      language_detection_model_service_;

  base::WeakPtrFactory<ContentLanguageDetectionDriver> weak_pointer_factory_{
      this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_CONTENT_LANGUAGE_DETECTION_DRIVER_H_
