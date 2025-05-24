// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_MOCK_AUTOFILL_AI_MODEL_CACHE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_MOCK_AUTOFILL_AI_MODEL_CACHE_H_

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillAiModelCache : public AutofillAiModelCache {
 public:
  MockAutofillAiModelCache();
  MockAutofillAiModelCache(const MockAutofillAiModelCache&) = delete;
  MockAutofillAiModelCache& operator=(const MockAutofillAiModelCache&) = delete;
  ~MockAutofillAiModelCache() override;

  MOCK_METHOD(void,
              Update,
              (FormSignature, ModelResponse, base::span<const FieldIdentifier>),
              (override));
  MOCK_METHOD(bool, Contains, (FormSignature), (const override));
  MOCK_METHOD(void, Erase, (FormSignature), (override));
  MOCK_METHOD((std::map<FormSignature, CacheEntryWithMetadata>),
              GetAllEntries,
              (),
              (const override));
  MOCK_METHOD((base::flat_map<FieldIdentifier, FieldPrediction>),
              GetFieldPredictions,
              (FormSignature),
              (const override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_MOCK_AUTOFILL_AI_MODEL_CACHE_H_
