// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_IMPL_TEST_API_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_IMPL_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/personal_context/core/personal_context_enablement_service_impl.h"

namespace personal_context {

class PersonalContextEnablementServiceImplTestApi {
 public:
  explicit PersonalContextEnablementServiceImplTestApi(
      PersonalContextEnablementServiceImpl* service)
      : service_(CHECK_DEREF(service)) {}

  PersonalContextEnablementState ComputeEnablementState() {
    return service_->ComputeEnablementState();
  }

 private:
  const raw_ref<PersonalContextEnablementServiceImpl> service_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_ENABLEMENT_SERVICE_IMPL_TEST_API_H_
