// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_MANAGER_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "components/language/core/browser/language_model.h"
#include "components/prefs/pref_service.h"

namespace language {

// Manages a set of LanguageModel objects.
class LanguageModelManager : public KeyedService {
 public:
  enum class ModelType {
    BASELINE,
    FLUENT,
    GEO,
    HEURISTIC,
  };

  LanguageModelManager(PrefService* prefs, const std::string& ui_lang);

  ~LanguageModelManager() override;

  void AddModel(const ModelType type, std::unique_ptr<LanguageModel> model);

  // Sets which model type should be used as the Primary model to make target
  // language decisions. A model for |type| must have been previously added
  // through a call to AddModel.
  void SetPrimaryModel(ModelType type);
  LanguageModel* GetPrimaryModel() const;

 private:
  std::unique_ptr<LanguageModel> default_model_;
  std::map<ModelType, std::unique_ptr<LanguageModel>> models_;

  ModelType primary_model_type_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LanguageModelManager);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_MANAGER_H_
