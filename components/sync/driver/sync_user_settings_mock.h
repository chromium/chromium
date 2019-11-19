// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_MOCK_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_MOCK_H_

#include <string>
#include <vector>

#include "components/sync/driver/sync_user_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class SyncUserSettingsMock : public SyncUserSettings {
 public:
  SyncUserSettingsMock();
  ~SyncUserSettingsMock() override;

  MOCK_CONST_METHOD0(IsSyncRequested, bool());
  MOCK_METHOD1(SetSyncRequested, void(bool));

  MOCK_CONST_METHOD0(IsSyncAllowedByPlatform, bool());
  MOCK_METHOD1(SetSyncAllowedByPlatform, void(bool));

  MOCK_CONST_METHOD0(IsFirstSetupComplete, bool());
  MOCK_METHOD1(SetFirstSetupComplete, void(SyncFirstSetupCompleteSource));

  MOCK_CONST_METHOD0(IsSyncEverythingEnabled, bool());
  MOCK_CONST_METHOD0(GetSelectedTypes, UserSelectableTypeSet());
  MOCK_METHOD2(SetSelectedTypes, void(bool, UserSelectableTypeSet));
  MOCK_CONST_METHOD0(GetRegisteredSelectableTypes, UserSelectableTypeSet());
  MOCK_CONST_METHOD0(GetForcedTypes, UserSelectableTypeSet());

#if defined(OS_CHROMEOS)
  MOCK_CONST_METHOD0(IsSyncAllOsTypesEnabled, bool());
  MOCK_CONST_METHOD0(GetSelectedOsTypes, UserSelectableOsTypeSet());
  MOCK_METHOD2(SetSelectedOsTypes, void(bool, UserSelectableOsTypeSet));
  MOCK_CONST_METHOD0(GetRegisteredSelectableOsTypes, UserSelectableOsTypeSet());

  MOCK_CONST_METHOD0(GetOsSyncFeatureEnabled, bool());
  MOCK_METHOD1(SetOsSyncFeatureEnabled, void(bool));
#endif

  MOCK_CONST_METHOD0(IsEncryptEverythingAllowed, bool());
  MOCK_CONST_METHOD0(IsEncryptEverythingEnabled, bool());
  MOCK_METHOD0(EnableEncryptEverything, void());

  MOCK_CONST_METHOD0(GetEncryptedDataTypes, ModelTypeSet());
  MOCK_CONST_METHOD0(IsPassphraseRequired, bool());
  MOCK_CONST_METHOD0(IsPassphraseRequiredForPreferredDataTypes, bool());
  MOCK_CONST_METHOD0(IsTrustedVaultKeyRequiredForPreferredDataTypes, bool());
  MOCK_CONST_METHOD0(IsUsingSecondaryPassphrase, bool());
  MOCK_CONST_METHOD0(GetExplicitPassphraseTime, base::Time());
  MOCK_CONST_METHOD0(GetPassphraseType, PassphraseType());

  MOCK_METHOD1(SetEncryptionPassphrase, void(const std::string&));
  MOCK_METHOD1(SetDecryptionPassphrase, bool(const std::string&));
  MOCK_METHOD2(AddTrustedVaultDecryptionKeys,
               void(const std::string&, const std::vector<std::string>&));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_MOCK_H_
