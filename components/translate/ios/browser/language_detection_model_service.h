// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/translate_model_service.h"

namespace translate {

class LanguageDetectionModelContainer;
class LanguageDetectionModel;

// A service that contains the LanguageDetectionModel and handles its loading.
// This is a workaround for crbug/1324530 on iOS where it is mandatory to have
// LanguageDetectionModel scoped by BrowserState.
// TODO(crbug.com/1324530): remove this class once TranslateModelService does
// this.
class LanguageDetectionModelService : public KeyedService {
 public:
  LanguageDetectionModelService(
      TranslateModelService* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~LanguageDetectionModelService() override;

  // Get for the actual TFLite language detection model.
  LanguageDetectionModel* GetLanguageDetectionModel();

  // Utility function to check if the model is already loaded.
  // |GetLanguageDetectionModel| can be used even if this return false.
  bool IsModelAvailable();

 private:
  // Notifies |this| that the translate model service is available for model
  // requests or is invalidating existing requests specified by |is_available|.
  void OnLanguageModelFileAvailabilityChanged(bool available);
  // The TranslageModelService that will handle the downloading and provide
  // the file containing the model.
  raw_ptr<TranslateModelService> translate_model_service_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<LanguageDetectionModelContainer> language_detection_model_;
  base::WeakPtrFactory<LanguageDetectionModelService> weak_ptr_factory_{this};
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_LANGUAGE_DETECTION_MODEL_SERVICE_H_
