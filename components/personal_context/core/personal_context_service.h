// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_H_

#include <optional>

#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/context_memory_service.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace personal_context {

// The PersonalContextService manages the personal context of a profile and acts
// as a bridge to handle requests to the Personal Context server.
class PersonalContextService : public KeyedService {
 public:
  ~PersonalContextService() override = default;

  // Fetches context for `feature` with `request_metadata` and invokes the
  // `callback` with results.
  virtual void FetchContext(
      proto::ContextMemoryFeature feature,
      const google::protobuf::MessageLite& request_metadata,
      const ContextMemoryRequestOptions& options,
      FetchContextCallback callback) = 0;

  // Fetches unmasked PII entities for the given `request` and invokes the
  // `callback` with results.
  virtual void FetchPiiEntities(const proto::FetchPiiEntitiesRequest& request,
                                const ContextMemoryRequestOptions& options,
                                FetchPiiContextCallback callback) = 0;

  // Decrypts the `encrypted_entity` field in `entity` using this device's
  // private key and converts it to an Entity. Returns the deserialized and
  // converted `Entity` on success, or `std::nullopt` if decryption, parsing,
  // or conversion fails.
  virtual std::optional<proto::Entity> DecryptEntity(
      const proto::Entity& entity) = 0;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_H_
