// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_H_

#include "components/autofill/core/common/signatures.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill {

// `AutofillAiModelCache` is an interface for storing and retrieving AutofillAI
// model responses. The cache is per profile.
class AutofillAiModelCache : public KeyedService {
 public:
  using CacheEntry = optimization_guide::proto::AutofillAiTypeResponse;

  // Updates the `entry` with key `form_signature`. If the `form_signature` is
  // not yet known to the cache, it is added to it.
  virtual void Update(FormSignature form_signature, CacheEntry entry) = 0;

  // Returns whether the cache contains an entry with `form_signature`.
  virtual bool Contains(FormSignature form_signature) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_CACHE_H_
