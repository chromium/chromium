// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_IOS_BROWSER_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_H_
#define COMPONENTS_LANGUAGE_DETECTION_IOS_BROWSER_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_H_

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace translate {
class LanguageDetectionModel;
}  // namespace translate

namespace language_detection {
class LanguageDetectionModelService;

// A service that contains the LanguageDetectionModel and handles its loading.
// This is a workaround for crbug/1324530 on iOS where it is mandatory to have
// LanguageDetectionModel scoped by BrowserState.
// TODO(crbug.com/40225076): remove this class once
// LanguageDetectionModelService does this.
class LanguageDetectionModelLoaderServiceIOS : public KeyedService {
 public:
  LanguageDetectionModelLoaderServiceIOS(
      language_detection::LanguageDetectionModelService*
          language_detection_model_service);
  ~LanguageDetectionModelLoaderServiceIOS() override;

  // Get for the actual TFLite language detection model.
  translate::LanguageDetectionModel* GetLanguageDetectionModel();

  // Utility function to check if the model is already loaded.
  // |GetLanguageDetectionModel| can be used even if this return false.
  bool IsModelAvailable();

 private:
  // Notifies |this| that the translate model service is available for model
  // requests or is invalidating existing requests specified by |is_available|.
  void OnLanguageDetectionModelFileReceived(base::File model_file);

  SEQUENCE_CHECKER(sequence_checker_);

  // The TranslageModelService that will handle the downloading and provide
  // the file containing the model.
  raw_ptr<language_detection::LanguageDetectionModelService>
      language_detection_model_service_;

  // The managed language detection model.
  std::unique_ptr<translate::LanguageDetectionModel> language_detection_model_;

  base::WeakPtrFactory<LanguageDetectionModelLoaderServiceIOS>
      weak_ptr_factory_{this};
};

}  // namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_IOS_BROWSER_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_H_
