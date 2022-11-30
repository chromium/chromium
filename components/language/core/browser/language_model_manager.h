// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_MANAGER_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_MANAGER_H_

#include <map>
#include <memory>
#include <string>

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
    ULP,
  };

  LanguageModelManager() = delete;

  LanguageModelManager(PrefService* prefs, const std::string& ui_lang);

  LanguageModelManager(const LanguageModelManager&) = delete;
  LanguageModelManager& operator=(const LanguageModelManager&) = delete;

  ~LanguageModelManager() override;

  void AddModel(const ModelType type, std::unique_ptr<LanguageModel> model);

  // Sets which model type should be used as the Primary model to make target
  // language decisions. A model for |type| must have been previously added
  // through a call to AddModel.
  void SetPrimaryModel(ModelType type);
  LanguageModel* GetPrimaryModel() const;
  ModelType GetPrimaryModelType() const;
  LanguageModel* GetLanguageModel(ModelType type) const;

 private:
  std::unique_ptr<LanguageModel> default_model_;
  std::map<ModelType, std::unique_ptr<LanguageModel>> models_;

  ModelType primary_model_type_;
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_MODEL_MANAGER_H_
