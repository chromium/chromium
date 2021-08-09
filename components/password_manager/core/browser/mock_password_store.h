// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  // PasswordStoreInterface

  MOCK_METHOD(void,
              GetAutofillableLogins,
              (PasswordStoreConsumer*),
              (override));

  MOCK_METHOD(void,
              DisableAutoSignInForOrigins,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::OnceClosure),
              (override));

  MOCK_METHOD(void, RemoveLogin, (const PasswordForm&), (override));
  MOCK_METHOD(void,
              RemoveLoginsByURLAndTime,
              (const base::RepeatingCallback<bool(const GURL&)>&,
               base::Time,
               base::Time,
               base::OnceClosure,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              Unblocklist,
              (const PasswordFormDigest&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              GetLogins,
              (const PasswordFormDigest&, PasswordStoreConsumer*),
              (override));
  MOCK_METHOD(void, AddLogin, (const PasswordForm&), (override));
  MOCK_METHOD(void, UpdateLogin, (const PasswordForm&), (override));
  MOCK_METHOD(void,
              UpdateLoginWithPrimaryKey,
              (const PasswordForm&, const PasswordForm&),
              (override));
  MOCK_METHOD(void,
              RemoveLoginsCreatedBetween,
              (base::Time, base::Time, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              ReportMetrics,
              (const std::string&, bool, bool),
              (override));
  MOCK_METHOD(void,
              ReportMetricsImpl,
              (const std::string&, bool, BulkCheckDone),
              (override));
  MOCK_METHOD(void,
              GetAllLoginsWithAffiliationAndBrandingInformation,
              (PasswordStoreConsumer*),
              (override));

  MOCK_METHOD(bool, IsAbleToSavePasswords, (), (override, const));

  MOCK_METHOD(SmartBubbleStatsStore*, GetSmartBubbleStatsStore, (), (override));
  MOCK_METHOD(FieldInfoStore*, GetFieldInfoStore, (), (override));
  MOCK_METHOD(std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>,
              CreateSyncControllerDelegate,
              (),
              (override));

 protected:
  ~MockPasswordStore() override;

 private:
  // PasswordStore:
  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
