// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager_test_base.h"

#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

PaymentsDataManagerTestBase::PaymentsDataManagerTestBase()
    : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting()) {}
PaymentsDataManagerTestBase::~PaymentsDataManagerTestBase() = default;

void PaymentsDataManagerTestBase::SetUpTest() {
  prefs_ = test::PrefServiceForTesting();
  base::FilePath path(WebDatabase::kInMemoryPath);
  profile_web_database_ = new WebDatabaseService(
      path, base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  profile_web_database_->AddTable(std::make_unique<AddressAutofillTable>());
  // Hacky: hold onto a pointer but pass ownership.
  profile_autofill_table_ = new PaymentsAutofillTable;
  profile_web_database_->AddTable(
      std::unique_ptr<WebDatabaseTable>(profile_autofill_table_));
  profile_web_database_->LoadDatabase(os_crypt_.get());
  profile_database_service_ = new AutofillWebDataService(
      profile_web_database_, base::SingleThreadTaskRunner::GetCurrentDefault());
  profile_database_service_->Init(base::NullCallback());

  account_web_database_ =
      new WebDatabaseService(base::FilePath(WebDatabase::kInMemoryPath),
                             base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::SingleThreadTaskRunner::GetCurrentDefault());
  account_autofill_table_ = new PaymentsAutofillTable;
  account_web_database_->AddTable(
      std::unique_ptr<WebDatabaseTable>(account_autofill_table_));
  account_web_database_->LoadDatabase(os_crypt_.get());
  account_database_service_ = new AutofillWebDataService(
      account_web_database_, base::SingleThreadTaskRunner::GetCurrentDefault());
  account_database_service_->Init(base::NullCallback());
}

void PaymentsDataManagerTestBase::TearDownTest() {
  account_database_service_->ShutdownDatabase();
  profile_web_database_->ShutdownDatabase();
}

}  // namespace autofill
