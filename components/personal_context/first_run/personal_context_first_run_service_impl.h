// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_
#define COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace personal_context {
class PersonalContextEligibilityService;

class PersonalContextFirstRunServiceImpl
    : public PersonalContextFirstRunService,
      public signin::IdentityManager::Observer {
 public:
  PersonalContextFirstRunServiceImpl(
      PersonalContextEligibilityService* eligibility_service,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager);
  PersonalContextFirstRunServiceImpl(
      const PersonalContextFirstRunServiceImpl&) = delete;
  PersonalContextFirstRunServiceImpl& operator=(
      const PersonalContextFirstRunServiceImpl&) = delete;
  ~PersonalContextFirstRunServiceImpl() override;

  // PersonalContextFirstRunService:
  void MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAmbientAutofillNotice() const override;
  void RecordAmbientAutofillNoticeImpression(uint32_t session_id) override;
  void MarkPersonalContextInAtMemoryNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAtMemoryNotice() const override;
  void RecordAtMemoryNoticeImpression(uint32_t session_id) override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

 private:
  raw_ptr<PersonalContextEligibilityService> eligibility_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  std::optional<uint32_t> last_logged_ambient_autofill_session_id_;
  std::optional<uint32_t> last_logged_at_memory_session_id_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_
