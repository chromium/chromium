// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/sync_service_impl_bundle.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_prefs.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

using testing::Return;

SyncServiceImplBundle::SyncServiceImplBundle()
    : identity_test_env_(&test_url_loader_factory_, &pref_service_) {
  SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
}

SyncServiceImplBundle::~SyncServiceImplBundle() = default;

std::unique_ptr<SyncClientMock> SyncServiceImplBundle::CreateSyncClientMock() {
  auto sync_client = std::make_unique<testing::NiceMock<SyncClientMock>>();
  ON_CALL(*sync_client, GetPrefService()).WillByDefault(Return(&pref_service_));
  ON_CALL(*sync_client, GetSyncEngineFactory())
      .WillByDefault(Return(&engine_factory_));
  ON_CALL(*sync_client, GetSyncInvalidationsService())
      .WillByDefault(Return(sync_invalidations_service()));
  ON_CALL(*sync_client, GetTrustedVaultClient())
      .WillByDefault(Return(trusted_vault_client()));
  ON_CALL(*sync_client, GetIdentityManager())
      .WillByDefault(Return(identity_manager()));
  return std::move(sync_client);
}

SyncServiceImpl::InitParams SyncServiceImplBundle::CreateBasicInitParams(
    std::unique_ptr<SyncClient> sync_client) {
  SyncServiceImpl::InitParams init_params;

  init_params.sync_client = std::move(sync_client);
  init_params.url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  init_params.create_http_post_provider_factory = base::BindRepeating(
      [](const std::string& user_agent,
         std::unique_ptr<network::PendingSharedURLLoaderFactory>
             pending_url_loader_factory) {
        return std::unique_ptr<HttpPostProviderFactory>();
      });
  init_params.network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  init_params.debug_identifier = "fakeDebugName";
  if (base::FeatureList::IsEnabled(syncer::kSyncUseOsCryptAsync)) {
    init_params.os_crypt_async = os_crypt_async_.get();
  }

  return init_params;
}

}  // namespace syncer
