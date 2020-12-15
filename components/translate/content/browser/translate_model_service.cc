// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/translate_model_service.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace translate {

TranslateModelService::TranslateModelService(
    optimization_guide::OptimizationGuideDecider* opt_guide)
    : opt_guide_(opt_guide) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION, this);
}

TranslateModelService::~TranslateModelService() = default;

void TranslateModelService::Shutdown() {
  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
}

void TranslateModelService::OnModelFileUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const base::FilePath& file_path) {
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION) {
    return;
  }
  // TODO(crbug.com/1151406): Implement loading the model on a background thread
  // and return it for use by translate.
}

base::Optional<base::File>
TranslateModelService::GetLanguageDetectionModelFile() {
  // TODO(crbug.com/1151406): Implement loading the model on a background thread
  // and return it for use by translate.
  return base::nullopt;
}

}  // namespace translate
