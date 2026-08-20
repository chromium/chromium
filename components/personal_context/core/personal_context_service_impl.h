// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_IMPL_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/personal_context/core/personal_context_service.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace personal_context {

class PersonalContextKeyManager;
class PersonalContextManager;

class PersonalContextServiceImpl : public PersonalContextService {
 public:
  PersonalContextServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::DeviceInfoSyncService* device_info_sync_service);

  PersonalContextServiceImpl(const PersonalContextServiceImpl&) = delete;
  PersonalContextServiceImpl& operator=(const PersonalContextServiceImpl&) = delete;
  ~PersonalContextServiceImpl() override;

  // KeyedService implementation:
  void Shutdown() override;

  // PersonalContextService implementation:
  void FetchContext(proto::ContextMemoryFeature feature,
                    const google::protobuf::MessageLite& request_metadata,
                    const ContextMemoryRequestOptions& options,
                    FetchContextCallback callback) override;
  void FetchPiiEntities(const proto::FetchPiiEntitiesRequest& request,
                        const ContextMemoryRequestOptions& options,
                        FetchPiiContextCallback callback) override;
  std::optional<proto::Entity> DecryptEntity(
      const proto::Entity& entity) override;

 private:
  std::unique_ptr<PersonalContextManager> personal_context_manager_;
  std::unique_ptr<PersonalContextKeyManager> key_manager_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_SERVICE_IMPL_H_
