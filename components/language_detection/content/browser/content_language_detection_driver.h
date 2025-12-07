// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_CONTENT_LANGUAGE_DETECTION_DRIVER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_CONTENT_LANGUAGE_DETECTION_DRIVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/language_detection/content/common/language_detection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace language_detection {

class LanguageDetectionModelProvider;

// Content implementation of LanguageDetectionDriver.
class ContentLanguageDetectionDriver
    : public mojom::ContentLanguageDetectionDriver,
      public base::SupportsUserData::Data {
 public:
  // `language_detection_model_provider` is not owned by and must outlive
  // `this`.
  explicit ContentLanguageDetectionDriver(
      LanguageDetectionModelProvider* language_detection_model_provider);

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

  void GetLanguageDetectionModelStatus(
      GetLanguageDetectionModelStatusCallback callback) override;

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

  // Provides access to the model file needed for language detection.
  const raw_ptr<LanguageDetectionModelProvider>
      language_detection_model_provider_;

  base::WeakPtrFactory<ContentLanguageDetectionDriver> weak_pointer_factory_{
      this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CONTENT_BROWSER_CONTENT_LANGUAGE_DETECTION_DRIVER_H_
