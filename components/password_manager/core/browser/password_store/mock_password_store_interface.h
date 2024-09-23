// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_INTERFACE_H_

#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStoreInterface : public PasswordStoreInterface {
 public:
  MockPasswordStoreInterface();

  MOCK_METHOD(bool, IsAbleToSavePasswords, (), (const, override));
  MOCK_METHOD(void,
              AddLogin,
              (const PasswordForm&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              UpdateLogin,
              (const PasswordForm&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              UpdateLogins,
              (const std::vector<PasswordForm>&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              AddLogins,
              (const std::vector<PasswordForm>&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              UpdateLoginWithPrimaryKey,
              (const PasswordForm&, const PasswordForm&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              RemoveLogin,
              (const base::Location&, const PasswordForm&),
              (override));
  MOCK_METHOD(void,
              RemoveLoginsByURLAndTime,
              (const base::Location&,
               const base::RepeatingCallback<bool(const GURL&)>&,
               base::Time,
               base::Time,
               base::OnceClosure,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              RemoveLoginsCreatedBetween,
              (const base::Location&,
               base::Time,
               base::Time,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              DisableAutoSignInForOrigins,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              Unblocklist,
              (const PasswordFormDigest&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              GetLogins,
              (const PasswordFormDigest&, base::WeakPtr<PasswordStoreConsumer>),
              (override));
  MOCK_METHOD(void,
              GetAutofillableLogins,
              (base::WeakPtr<PasswordStoreConsumer>),
              (override));
  MOCK_METHOD(void,
              GetAllLogins,
              (base::WeakPtr<PasswordStoreConsumer>),
              (override));
  MOCK_METHOD(void,
              GetAllLoginsWithAffiliationAndBrandingInformation,
              (base::WeakPtr<PasswordStoreConsumer>),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(SmartBubbleStatsStore*, GetSmartBubbleStatsStore, (), (override));
  MOCK_METHOD(std::unique_ptr<syncer::DataTypeControllerDelegate>,
              CreateSyncControllerDelegate,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddSyncEnabledOrDisabledCallback,
              (base::RepeatingClosure),
              (override));
  MOCK_METHOD(PasswordStoreBackend*, GetBackendForTesting, (), (override));
  MOCK_METHOD(void,
              OnSyncServiceInitialized,
              (syncer::SyncService*),
              (override));

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

 protected:
  ~MockPasswordStoreInterface() override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_MOCK_PASSWORD_STORE_INTERFACE_H_
