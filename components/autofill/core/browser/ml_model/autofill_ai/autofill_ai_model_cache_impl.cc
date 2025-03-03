// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_cache_impl.h"

#include <utility>

namespace autofill {

AutofillAiModelCacheImpl::AutofillAiModelCacheImpl(
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& profile_path) {}

AutofillAiModelCacheImpl::~AutofillAiModelCacheImpl() = default;

void AutofillAiModelCacheImpl::Update(FormSignature form_signature,
                                      CacheEntry entry) {
  in_memory_cache_[form_signature] = std::move(entry);
}

bool AutofillAiModelCacheImpl::Contains(FormSignature form_signature) const {
  return in_memory_cache_.contains(form_signature);
}

}  // namespace autofill
