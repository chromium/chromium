// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_
#define COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/first_run/personal_context_first_run_client.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"
#include "components/personal_context/first_run/personal_context_first_run_types.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefService;

namespace content {
class WebContents;
}

namespace personal_context {
class PersonalContextEligibilityService;

class PersonalContextFirstRunServiceImpl
    : public PersonalContextFirstRunService,
      public signin::IdentityManager::Observer {
 public:
  PersonalContextFirstRunServiceImpl(
      std::unique_ptr<PersonalContextFirstRunClient> client,
      PersonalContextEligibilityService* eligibility_service,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager);
  PersonalContextFirstRunServiceImpl(
      const PersonalContextFirstRunServiceImpl&) = delete;
  PersonalContextFirstRunServiceImpl& operator=(
      const PersonalContextFirstRunServiceImpl&) = delete;
  ~PersonalContextFirstRunServiceImpl() override;

  // PersonalContextFirstRunService:
  void MaybeTriggerFirstRun(
      content::WebContents* web_contents,
      FirstRunInvocationSource invocation_source,
      base::OnceCallback<void(FirstRunTriggerResult)> callback) override;
  void MarkPersonalContextAmbientAutofillNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAmbientAutofillNotice() const override;
  void MarkPersonalContextInAtMemoryNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAtMemoryNotice() const override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

 private:
  void OnNoticeDialogCompleted(
      base::OnceCallback<void(FirstRunTriggerResult)> callback,
      NoticeResult result);

  std::unique_ptr<PersonalContextFirstRunClient> client_;
  raw_ptr<PersonalContextEligibilityService> eligibility_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<PersonalContextFirstRunServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_
