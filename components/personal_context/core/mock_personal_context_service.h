// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_SERVICE_H_

#include "base/functional/callback.h"
#include "components/personal_context/core/personal_context_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace personal_context {

class MockPersonalContextService : public PersonalContextService {
 public:
  MockPersonalContextService();
  ~MockPersonalContextService() override;

  MOCK_METHOD(void,
              FetchContext,
              (proto::ContextMemoryFeature feature,
               const google::protobuf::MessageLite& request_metadata,
               const ContextMemoryRequestOptions& options,
               FetchContextCallback callback),
              (override));
  MOCK_METHOD(void,
              FetchPiiEntities,
              (const proto::FetchPiiEntitiesRequest& request,
               const ContextMemoryRequestOptions& options,
               FetchPiiContextCallback callback),
              (override));
  MOCK_METHOD(std::optional<proto::Entity>,
              DecryptEntity,
              (const proto::Entity& entity),
              (override));
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_MOCK_PERSONAL_CONTEXT_SERVICE_H_
