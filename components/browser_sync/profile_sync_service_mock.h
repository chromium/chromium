// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/device_info/device_info.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/protocol/sync_protocol_error.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

class ProfileSyncServiceMock : public ProfileSyncService {
 public:
  explicit ProfileSyncServiceMock(InitParams init_params);
  // The second constructor defers to the first one. Use it when you need to
  // create a StrictMock or NiceMock of ProfileSyncServiceMock, because those
  // template classes cannot handle the input class having constructors with
  // arguments passed by value. Otherwise use the constructor above for cleaner
  // code.
  explicit ProfileSyncServiceMock(InitParams* init_params);

  ~ProfileSyncServiceMock() override;

  MOCK_METHOD6(
      OnEngineInitialized,
      void(syncer::ModelTypeSet initial_types,
           const syncer::WeakHandle<syncer::JsBackend>&,
           const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&,
           const std::string&,
           const std::string&,
           bool));
  MOCK_METHOD1(OnSyncCycleCompleted, void(const syncer::SyncCycleSnapshot&));
  MOCK_METHOD2(OnUserChoseDatatypes,
               void(bool sync_everything, syncer::ModelTypeSet chosen_types));
  MOCK_METHOD0(SetFirstSetupComplete, void());
  MOCK_METHOD0(GetSetupInProgressHandle,
               std::unique_ptr<syncer::SyncSetupInProgressHandle>());

  MOCK_METHOD2(OnUnrecoverableError,
               void(const base::Location& location,
                    const std::string& message));
  MOCK_CONST_METHOD0(GetUserShare, syncer::UserShare*());
  MOCK_METHOD0(RequestStart, void());
  MOCK_METHOD1(RequestStop, void(ProfileSyncService::SyncStopDataFate));

  MOCK_METHOD0(NotifyForeignSessionUpdated, void());
  MOCK_METHOD1(AddObserver, void(syncer::SyncServiceObserver*));
  MOCK_METHOD1(RemoveObserver, void(syncer::SyncServiceObserver*));
  MOCK_METHOD0(GetJsController, base::WeakPtr<syncer::JsController>());
  MOCK_CONST_METHOD0(IsFirstSetupComplete, bool());

  MOCK_CONST_METHOD0(IsEncryptEverythingAllowed, bool());
  MOCK_CONST_METHOD0(IsEncryptEverythingEnabled, bool());
  MOCK_METHOD0(EnableEncryptEverything, void());

  MOCK_CONST_METHOD0(GetActiveDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetPreferredDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetRegisteredDataTypes, syncer::ModelTypeSet());
  MOCK_CONST_METHOD0(GetLastCycleSnapshot, syncer::SyncCycleSnapshot());

  MOCK_CONST_METHOD0(GetDisableReasons, int());
  MOCK_CONST_METHOD0(GetTransportState, TransportState());
  MOCK_CONST_METHOD1(QueryDetailedSyncStatus,
                     bool(syncer::SyncEngine::Status* result));
  MOCK_CONST_METHOD0(GetAuthError, const GoogleServiceAuthError&());
  MOCK_CONST_METHOD0(IsSetupInProgress, bool());
  MOCK_CONST_METHOD0(GetLastSyncedTime, base::Time());
  MOCK_CONST_METHOD0(IsSyncConfirmationNeeded, bool());
  MOCK_METHOD1(OnActionableError, void(const syncer::SyncProtocolError&));
  MOCK_CONST_METHOD1(IsDataTypeControllerRunning, bool(syncer::ModelType));

  MOCK_METHOD0(GetOpenTabsUIDelegateMock, sync_sessions::OpenTabsUIDelegate*());
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;

  MOCK_METHOD0(StartUpSlowEngineComponents, void());

  // DataTypeManagerObserver mocks.
  MOCK_METHOD1(OnConfigureDone,
               void(const syncer::DataTypeManager::ConfigureResult&));
  MOCK_METHOD0(OnConfigureStart, void());

  MOCK_CONST_METHOD0(IsPassphraseRequired, bool());
  MOCK_CONST_METHOD0(IsPassphraseRequiredForDecryption, bool());
  MOCK_CONST_METHOD0(IsUsingSecondaryPassphrase, bool());
  MOCK_CONST_METHOD0(GetPassphraseType, syncer::PassphraseType());
  MOCK_CONST_METHOD0(GetExplicitPassphraseTime, base::Time());

  MOCK_METHOD1(SetDecryptionPassphrase, bool(const std::string& passphrase));
  MOCK_METHOD1(SetEncryptionPassphrase, void(const std::string& passphrase));

  MOCK_METHOD0(OnSetupInProgressHandleDestroyed, void());

  // TODO(crbug.com/871221): Remove this override. This is overridden here to
  // return true by default, as a workaround for tests not setting up an
  // authenticated account and IsSyncFeatureEnabled() therefore returning false.
  bool IsAuthenticatedAccountPrimary() const override;

  // Gives access to the real implementation of ProfileSyncService methods:
  std::unique_ptr<syncer::SyncSetupInProgressHandle>
  GetSetupInProgressHandleConcrete();
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_SERVICE_MOCK_H_
