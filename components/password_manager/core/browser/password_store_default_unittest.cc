// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_default.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_origin_unittest.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::_;

namespace password_manager {

namespace {

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
 public:
  MOCK_METHOD1(OnGetPasswordStoreResultsConstRef,
               void(const std::vector<std::unique_ptr<PasswordForm>>&));

  // GMock cannot mock methods with move-only args.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    OnGetPasswordStoreResultsConstRef(results);
  }
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public LoginDatabase {
 public:
  BadLoginDatabase() : LoginDatabase(base::FilePath(), IsAccountStore(false)) {}
  ~BadLoginDatabase() override {}

  // LoginDatabase:
  bool Init() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BadLoginDatabase);
};

PasswordFormData CreateTestPasswordFormData() {
  PasswordFormData data = {PasswordForm::Scheme::kHtml,
                           "http://bar.example.com",
                           "http://bar.example.com/origin",
                           "http://bar.example.com/action",
                           L"submit_element",
                           L"username_element",
                           L"password_element",
                           L"username_value",
                           L"password_value",
                           true,
                           1};
  return data;
}

class PasswordStoreDefaultTestDelegate {
 public:
  PasswordStoreDefaultTestDelegate();
  explicit PasswordStoreDefaultTestDelegate(
      std::unique_ptr<LoginDatabase> database);
  ~PasswordStoreDefaultTestDelegate();

  PasswordStoreDefault* store() { return store_.get(); }

  void FinishAsyncProcessing();

 private:
  void SetupTempDir();

  void ClosePasswordStore();

  scoped_refptr<PasswordStoreDefault> CreateInitializedStore(
      std::unique_ptr<LoginDatabase> database);

  base::FilePath test_login_db_file_path() const;

  base::test::TaskEnvironment task_environment_{base::test::TaskEnvironment::MainThreadType::UI};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<PasswordStoreDefault> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefaultTestDelegate);
};

PasswordStoreDefaultTestDelegate::PasswordStoreDefaultTestDelegate() {
  OSCryptMocker::SetUp();
  SetupTempDir();
  store_ = CreateInitializedStore(std::make_unique<LoginDatabase>(
      test_login_db_file_path(), IsAccountStore(false)));
}

PasswordStoreDefaultTestDelegate::PasswordStoreDefaultTestDelegate(
    std::unique_ptr<LoginDatabase> database) {
  OSCryptMocker::SetUp();
  SetupTempDir();
  store_ = CreateInitializedStore(std::move(database));
}

PasswordStoreDefaultTestDelegate::~PasswordStoreDefaultTestDelegate() {
  ClosePasswordStore();
  OSCryptMocker::TearDown();
}

void PasswordStoreDefaultTestDelegate::FinishAsyncProcessing() {
  task_environment_.RunUntilIdle();
}

void PasswordStoreDefaultTestDelegate::SetupTempDir() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void PasswordStoreDefaultTestDelegate::ClosePasswordStore() {
  store_->ShutdownOnUIThread();
  FinishAsyncProcessing();
  ASSERT_TRUE(temp_dir_.Delete());
}

scoped_refptr<PasswordStoreDefault>
PasswordStoreDefaultTestDelegate::CreateInitializedStore(
    std::unique_ptr<LoginDatabase> database) {
  scoped_refptr<PasswordStoreDefault> store(
      new PasswordStoreDefault(std::move(database)));
  store->Init(syncer::SyncableService::StartSyncFlare(), nullptr);

  return store;
}

base::FilePath PasswordStoreDefaultTestDelegate::test_login_db_file_path()
    const {
  return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
}

}  // anonymous namespace

INSTANTIATE_TYPED_TEST_SUITE_P(Default,
                               PasswordStoreOriginTest,
                               PasswordStoreDefaultTestDelegate);

TEST(PasswordStoreDefaultTest, NonASCIIData) {
  PasswordStoreDefaultTestDelegate delegate;
  PasswordStoreDefault* store = delegate.store();

  // Some non-ASCII password form data.
  static const PasswordFormData form_data[] = {
      {PasswordForm::Scheme::kHtml, "http://foo.example.com",
       "http://foo.example.com/origin", "http://foo.example.com/action",
       L"มีสีสัน", L"お元気ですか?", L"盆栽", L"أحب كرة", L"£éä국수çà", true, 1},
  };

  // Build the expected forms vector and add the forms to the store.
  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  for (unsigned int i = 0; i < base::size(form_data); ++i) {
    expected_forms.push_back(FillPasswordFormWithData(form_data[i]));
    store->AddLogin(*expected_forms.back());
  }

  MockPasswordStoreConsumer consumer;

  // We expect to get the same data back, even though it's not all ASCII.
  EXPECT_CALL(
      consumer,
      OnGetPasswordStoreResultsConstRef(
          password_manager::UnorderedPasswordFormElementsAre(&expected_forms)));
  store->GetAutofillableLogins(&consumer);

  delegate.FinishAsyncProcessing();
}

TEST(PasswordStoreDefaultTest, Notifications) {
  PasswordStoreDefaultTestDelegate delegate;
  PasswordStoreDefault* store = delegate.store();

  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormData());

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  const PasswordStoreChange expected_add_changes[] = {
      PasswordStoreChange(PasswordStoreChange::ADD, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_add_changes)));

  // Adding a login should trigger a notification.
  store->AddLogin(*form);

  // Change the password.
  form->password_value = base::ASCIIToUTF16("a different password");

  const PasswordStoreChange expected_update_changes[] = {
      PasswordStoreChange(PasswordStoreChange::UPDATE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_update_changes)));

  // Updating the login with the new password should trigger a notification.
  store->UpdateLogin(*form);

  const PasswordStoreChange expected_delete_changes[] = {
      PasswordStoreChange(PasswordStoreChange::REMOVE, *form),
  };

  EXPECT_CALL(observer,
              OnLoginsChanged(ElementsAreArray(expected_delete_changes)));

  // Deleting the login should trigger a notification.
  store->RemoveLogin(*form);
  // Run the tasks to allow all the above expected calls to take place.
  delegate.FinishAsyncProcessing();

  store->RemoveObserver(&observer);
}

// Verify that operations on a PasswordStore with a bad database cause no
// explosions, but fail without side effect, return no data and trigger no
// notifications.
TEST(PasswordStoreDefaultTest, OperationsOnABadDatabaseSilentlyFail) {
  PasswordStoreDefaultTestDelegate delegate(
      std::make_unique<BadLoginDatabase>());
  PasswordStoreDefault* bad_store = delegate.store();
  delegate.FinishAsyncProcessing();
  ASSERT_EQ(nullptr, bad_store->login_db());

  testing::StrictMock<MockPasswordStoreObserver> mock_observer;
  bad_store->AddObserver(&mock_observer);

  // Add a new autofillable login + a blacklisted login.
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormData());
  std::unique_ptr<PasswordForm> blacklisted_form(new PasswordForm(*form));
  blacklisted_form->signon_realm = "http://foo.example.com";
  blacklisted_form->origin = GURL("http://foo.example.com/origin");
  blacklisted_form->action = GURL("http://foo.example.com/action");
  blacklisted_form->blacklisted_by_user = true;
  bad_store->AddLogin(*form);
  bad_store->AddLogin(*blacklisted_form);
  delegate.FinishAsyncProcessing();

  // Get all logins; autofillable logins; blacklisted logins.
  testing::StrictMock<MockPasswordStoreConsumer> mock_consumer;
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetLogins(PasswordStore::FormDigest(*form), &mock_consumer);
  delegate.FinishAsyncProcessing();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetAutofillableLogins(&mock_consumer);
  delegate.FinishAsyncProcessing();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsConstRef(IsEmpty()));
  bad_store->GetAllLogins(&mock_consumer);
  delegate.FinishAsyncProcessing();
  testing::Mock::VerifyAndClearExpectations(&mock_consumer);

  // Report metrics.
  bad_store->ReportMetrics("Test Username", true, false);
  delegate.FinishAsyncProcessing();

  // Change the login.
  form->password_value = base::ASCIIToUTF16("a different password");
  bad_store->UpdateLogin(*form);
  delegate.FinishAsyncProcessing();

  // Delete one login; a range of logins.
  bad_store->RemoveLogin(*form);
  delegate.FinishAsyncProcessing();
  base::RunLoop run_loop;
  bad_store->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(),
                                        run_loop.QuitClosure());
  run_loop.Run();
  delegate.FinishAsyncProcessing();

  // Ensure no notifications and no explosions during shutdown either.
  bad_store->RemoveObserver(&mock_observer);
}

}  // namespace password_manager
