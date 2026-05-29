// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager.h"

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

namespace autofill {

class PersonalContextAccessManagerImpl : public PersonalContextAccessManager {
 public:
  explicit PersonalContextAccessManagerImpl(
      personal_context::PersonalContextService* personal_context_service);

  PersonalContextAccessManagerImpl(const PersonalContextAccessManagerImpl&) =
      delete;
  PersonalContextAccessManagerImpl& operator=(
      const PersonalContextAccessManagerImpl&) = delete;

  ~PersonalContextAccessManagerImpl() override;

 private:
  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_IMPL_H_
