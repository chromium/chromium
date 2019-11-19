// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_PROFILE_SYNC_SERVICE_BUNDLE_H_
#define COMPONENTS_SYNC_DRIVER_PROFILE_SYNC_SERVICE_BUNDLE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/test/test_url_loader_factory.h"

namespace syncer {

// Aggregate this class to get all necessary support for creating a
// ProfileSyncService in tests. The test still needs to have its own
// MessageLoop, though.
class ProfileSyncServiceBundle {
 public:
  ProfileSyncServiceBundle();

  ~ProfileSyncServiceBundle();

  // Creates a mock sync client that leverages the dependencies in this bundle.
  std::unique_ptr<SyncClientMock> CreateSyncClientMock();

  // Creates an InitParams instance with the specified |start_behavior| and
  // |sync_client|, and fills the rest with dummy values and objects owned by
  // the bundle.
  ProfileSyncService::InitParams CreateBasicInitParams(
      ProfileSyncService::StartBehavior start_behavior,
      std::unique_ptr<SyncClient> sync_client);

  // Accessors

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  SyncApiComponentFactoryMock* component_factory() {
    return &component_factory_;
  }

  invalidation::ProfileIdentityProvider* identity_provider() {
    return identity_provider_.get();
  }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<SyncApiComponentFactoryMock> component_factory_;
  std::unique_ptr<invalidation::ProfileIdentityProvider> identity_provider_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceBundle);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_PROFILE_SYNC_SERVICE_BUNDLE_H_
