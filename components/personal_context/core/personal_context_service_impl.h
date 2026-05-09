// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_IMPL_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_IMPL_H_

#include "components/personal_context/core/personal_context_service.h"

namespace personal_context {

class PersonalContextServiceImpl : public PersonalContextService {
 public:
  PersonalContextServiceImpl();

  PersonalContextServiceImpl(const PersonalContextServiceImpl&) = delete;
  PersonalContextServiceImpl& operator=(const PersonalContextServiceImpl&) = delete;
  ~PersonalContextServiceImpl() override;

  // PersonalContextService implementation:
  void FetchContext(proto::ContextMemoryFeature feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const ContextMemoryRequestOptions& options,
                    FetchContextCallback callback) override;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_IMPL_H_
