// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_SYNC_CLIENT_MOCK_H_
#define COMPONENTS_SYNC_TEST_SYNC_CLIENT_MOCK_H_

#include <map>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class SyncClientMock : public SyncClient {
 public:
  SyncClientMock();

  SyncClientMock(const SyncClientMock&) = delete;
  SyncClientMock& operator=(const SyncClientMock&) = delete;

  ~SyncClientMock() override;

  MOCK_METHOD(PrefService*, GetPrefService, (), (override));
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(base::FilePath, GetLocalSyncBackendFolder, (), (override));
  MOCK_METHOD(syncer::SyncInvalidationsService*,
              GetSyncInvalidationsService,
              (),
              (override));
  MOCK_METHOD(trusted_vault::TrustedVaultClient*,
              GetTrustedVaultClient,
              (),
              (override));
  MOCK_METHOD(scoped_refptr<ExtensionsActivity>,
              GetExtensionsActivity,
              (),
              (override));
  MOCK_METHOD(SyncEngineFactory*, GetSyncEngineFactory, (), (override));
  MOCK_METHOD(bool, IsCustomPassphraseAllowed, (), (override));
  MOCK_METHOD(bool, IsPasswordSyncAllowed, (), (override));
  MOCK_METHOD(void,
              SetPasswordSyncAllowedChangeCb,
              (const base::RepeatingClosure&),
              (override));
  MOCK_METHOD(
      void,
      GetLocalDataDescriptions,
      (DataTypeSet types,
       base::OnceCallback<void(std::map<DataType, LocalDataDescription>)>
           callback),
      (override));
  MOCK_METHOD(void, TriggerLocalDataMigration, (DataTypeSet types), (override));
  MOCK_METHOD(void,
              RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial,
              (const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup&),
              (override));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_SYNC_CLIENT_MOCK_H_
