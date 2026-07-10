// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_TEST_API_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager_impl.h"

namespace autofill {

class AutofillAiPersonalContextAccessManagerImplTestApi {
 public:
  explicit AutofillAiPersonalContextAccessManagerImplTestApi(
      AutofillAiPersonalContextAccessManagerImpl* manager)
      : manager_(*manager) {}

  void ResetStateForType(EntityType type) { manager_->ResetStateForType(type); }

  void CacheUnmaskedSpiiEntity(EntityInstance entity) {
    manager_->CacheUnmaskedSpiiEntity(std::move(entity));
  }

  void CachePresenceSignal(EntityType type) {
    manager_->CachePresenceSignal(type);
  }

  bool IsPresenceSignalCached(EntityType type) const {
    return manager_->spii_presence_signal_cache_.contains(type);
  }

 private:
  raw_ref<AutofillAiPersonalContextAccessManagerImpl> manager_;
};

inline AutofillAiPersonalContextAccessManagerImplTestApi test_api(
    AutofillAiPersonalContextAccessManagerImpl& manager) {
  return AutofillAiPersonalContextAccessManagerImplTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_TEST_API_H_
