// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_IMPL_TEST_API_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_IMPL_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/personal_context/core/personal_context_eligibility_service_impl.h"

namespace personal_context {

class PersonalContextEligibilityServiceImplTestApi {
 public:
  explicit PersonalContextEligibilityServiceImplTestApi(
      PersonalContextEligibilityServiceImpl* service)
      : service_(CHECK_DEREF(service)) {}

  PersonalContextEligibilityState ComputeEligibilityState() {
    return service_->ComputeEligibilityState().first;
  }

 private:
  const raw_ref<PersonalContextEligibilityServiceImpl> service_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_IMPL_TEST_API_H_
