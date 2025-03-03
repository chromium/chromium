// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_IMPL_H_

#include <map>

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache.h"
#include "components/autofill/core/common/signatures.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace autofill {

// `AutofillAiModelCacheImpl` implements `AutofillAiModelCache` using a
// LevelDB as a persistence layer and a map as an in-memory cache to allow
// synchronous access.
class AutofillAiModelCacheImpl : public AutofillAiModelCache {
 public:
  AutofillAiModelCacheImpl(leveldb_proto::ProtoDatabaseProvider* db_provider,
                           const base::FilePath& profile_path);
  AutofillAiModelCacheImpl(const AutofillAiModelCacheImpl&) = delete;
  AutofillAiModelCacheImpl& operator=(const AutofillAiModelCacheImpl&) = delete;
  AutofillAiModelCacheImpl(AutofillAiModelCacheImpl&&) = delete;
  AutofillAiModelCacheImpl& operator=(AutofillAiModelCacheImpl&&) = delete;
  ~AutofillAiModelCacheImpl() override;

  // AutofillAiModelCache:
  void Update(FormSignature form_signature, CacheEntry entry) override;
  bool Contains(FormSignature form_signature) const override;

 private:
  std::map<FormSignature, CacheEntry> in_memory_cache_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_IMPL_H_
