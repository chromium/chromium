// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_
#define COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/first_run/personal_context_first_run_client.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"
#include "components/personal_context/first_run/personal_context_first_run_types.h"

class PrefService;

namespace content {
class WebContents;
}

namespace personal_context {
class PersonalContextEnablementService;

class PersonalContextFirstRunServiceImpl
    : public PersonalContextFirstRunService {
 public:
  PersonalContextFirstRunServiceImpl(
      std::unique_ptr<PersonalContextFirstRunClient> client,
      PersonalContextEnablementService* enablement_service,
      PrefService* pref_service);
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
  void MarkPersonalContextInAutofillNoticeAsShown() override;
  void MarkPersonalContextInAutofillNoticeAsAcknowledged() override;
  bool ShouldShowPersonalContextAutofillNotice() const override;

 private:
  void OnNoticeDialogCompleted(
      base::OnceCallback<void(FirstRunTriggerResult)> callback,
      NoticeResult result);

  std::unique_ptr<PersonalContextFirstRunClient> client_;
  raw_ptr<PersonalContextEnablementService> enablement_service_;
  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<PersonalContextFirstRunServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_FIRST_RUN_PERSONAL_CONTEXT_FIRST_RUN_SERVICE_IMPL_H_
