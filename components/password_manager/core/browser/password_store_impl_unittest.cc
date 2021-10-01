// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;

namespace password_manager {

namespace {

constexpr const char kTestWebRealm1[] = "https://one.example.com/";
constexpr const char kTestWebOrigin1[] = "https://one.example.com/origin";
constexpr const char kTestWebRealm2[] = "https://two.example.com/";
constexpr const char kTestWebOrigin2[] = "https://two.example.com/origin";
constexpr const char kTestWebRealm3[] = "https://three.example.com/";
constexpr const char kTestWebOrigin3[] = "https://three.example.com/origin";
constexpr const char kTestAndroidRealm1[] =
    "android://hash@com.example.android/";
constexpr const char kTestAndroidRealm2[] =
    "android://hash@com.example.two.android/";
constexpr const char kTestAndroidRealm3[] =
    "android://hash@com.example.three.android/";
constexpr const time_t kTestLastUsageTime = 1546300800;  // 00:00 Jan 1 2019 UTC

class MockPasswordStoreConsumer : public PasswordStoreConsumer {
  MOCK_METHOD(void,
              OnGetPasswordStoreResults,
              (std::vector<std::unique_ptr<PasswordForm>> results),
              (override));
};

class MockPasswordStoreBackendTester {
 public:
  MOCK_METHOD(void, HandleChanges, (const PasswordStoreChangeList&));
  MOCK_METHOD(void,
              LoginsReceivedConstRef,
              (const std::vector<std::unique_ptr<PasswordForm>>&));

  void HandleLogins(std::vector<std::unique_ptr<PasswordForm>> results) {
    LoginsReceivedConstRef(results);
  }
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public LoginDatabase {
 public:
  BadLoginDatabase() : LoginDatabase(base::FilePath(), IsAccountStore(false)) {}

  BadLoginDatabase(const BadLoginDatabase&) = delete;
  BadLoginDatabase& operator=(const BadLoginDatabase&) = delete;

  // LoginDatabase:
  bool Init() override { return false; }
};

PasswordFormData CreateTestPasswordFormData() {
  PasswordFormData data = {PasswordForm::Scheme::kHtml,
                           "http://bar.example.com",
                           "http://bar.example.com/origin",
                           "http://bar.example.com/action",
                           u"submit_element",
                           u"username_element",
                           u"password_element",
                           u"username_value",
                           u"password_value",
                           true,
                           1};
  return data;
}

}  // anonymous namespace

class PasswordStoreImplTest : public testing::Test {
 public:
  PasswordStoreImplTest() = default;

  PasswordStoreBackend* Initialize() {
    store_ =
        std::make_unique<PasswordStoreImpl>(std::make_unique<LoginDatabase>(
            test_login_db_file_path(), IsAccountStore(false)));
    PasswordStoreBackend* backend = store_.get();
    backend->InitBackend(/*remote_form_changes_received=*/base::DoNothing(),
                         /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
                         /*completion=*/base::DoNothing());
    RunUntilIdle();
    return backend;
  }

  PasswordStoreBackend* InitializeWithDatabase(
      std::unique_ptr<LoginDatabase> database) {
    store_ = std::make_unique<PasswordStoreImpl>(std::move(database));
    PasswordStoreBackend* backend = store_.get();
    backend->InitBackend(/*remote_form_changes_received=*/base::DoNothing(),
                         /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
                         /*completion=*/base::DoNothing());
    RunUntilIdle();
    return backend;
  }

  void SetUp() override {
    OSCryptMocker::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    PasswordStoreBackend* backend = store_.get();
    backend->Shutdown(base::BindOnce(
        [](std::unique_ptr<PasswordStoreBackend> backend) { backend.reset(); },
        std::move(store_)));
    RunUntilIdle();
    OSCryptMocker::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  void SetupTempDir();

  void ClosePasswordStore();

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PasswordStoreImpl> store_;
};

TEST_F(PasswordStoreImplTest, NonASCIIData) {
  PasswordStoreBackend* backend = Initialize();

  // Some non-ASCII password form data.
  static const PasswordFormData form_data = {PasswordForm::Scheme::kHtml,
                                             "http://foo.example.com",
                                             "http://foo.example.com/origin",
                                             "http://foo.example.com/action",
                                             u"มีสีสัน",
                                             u"お元気ですか?",
                                             u"盆栽",
                                             u"أحب كرة",
                                             u"£éä국수çà",
                                             true,
                                             1};

  // Build the expected forms vector and add the forms to the store.
  std::vector<std::unique_ptr<PasswordForm>> expected_forms;
  expected_forms.push_back(FillPasswordFormWithData(form_data));
  backend->AddLoginAsync(*expected_forms.back(), base::DoNothing());
  testing::StrictMock<MockPasswordStoreBackendTester> tester;

  // We expect to get the same data back, even though it's not all ASCII.
  EXPECT_CALL(
      tester,
      LoginsReceivedConstRef(
          password_manager::UnorderedPasswordFormElementsAre(&expected_forms)));

  backend->GetAutofillableLoginsAsync(
      base::BindOnce(&MockPasswordStoreBackendTester::HandleLogins,
                     base::Unretained(&tester)));

  RunUntilIdle();
}

TEST_F(PasswordStoreImplTest, TestAddLoginAsync) {
  PasswordStoreBackend* backend = Initialize();
  PasswordForm form = *FillPasswordFormWithData(CreateTestPasswordFormData());

  const PasswordStoreChange add_change =
      PasswordStoreChange(PasswordStoreChange::ADD, form);

  testing::StrictMock<MockPasswordStoreBackendTester> tester;
  EXPECT_CALL(tester, HandleChanges(ElementsAre(add_change)));
  backend->AddLoginAsync(
      form, base::BindOnce(&MockPasswordStoreBackendTester::HandleChanges,
                           base::Unretained(&tester)));
  RunUntilIdle();
}

TEST_F(PasswordStoreImplTest, TestUpdateLoginAsync) {
  PasswordStoreBackend* backend = Initialize();
  PasswordForm form = *FillPasswordFormWithData(CreateTestPasswordFormData());

  backend->AddLoginAsync(form, base::DoNothing());
  RunUntilIdle();

  form.password_value = u"a different password";
  const PasswordStoreChange update_change =
      PasswordStoreChange(PasswordStoreChange::UPDATE, form);

  testing::StrictMock<MockPasswordStoreBackendTester> tester;
  EXPECT_CALL(tester, HandleChanges(ElementsAre(update_change)));
  backend->UpdateLoginAsync(
      form, base::BindOnce(&MockPasswordStoreBackendTester::HandleChanges,
                           base::Unretained(&tester)));
  RunUntilIdle();
}

TEST_F(PasswordStoreImplTest, TestRemoveLoginAsync) {
  PasswordStoreBackend* backend = Initialize();
  PasswordForm form = *FillPasswordFormWithData(CreateTestPasswordFormData());

  backend->AddLoginAsync(form, base::DoNothing());
  RunUntilIdle();

  PasswordStoreChange remove_change =
      PasswordStoreChange(PasswordStoreChange::REMOVE, form);

  testing::StrictMock<MockPasswordStoreBackendTester> tester;
  EXPECT_CALL(tester, HandleChanges(ElementsAre(remove_change)));
  backend->RemoveLoginAsync(
      form, base::BindOnce(&MockPasswordStoreBackendTester::HandleChanges,
                           base::Unretained(&tester)));
  RunUntilIdle();
}

// Verify that operations on a PasswordStore with a bad database cause no
// explosions, but fail without side effect, return no data and trigger no
// notifications.
TEST_F(PasswordStoreImplTest, OperationsOnABadDatabaseSilentlyFail) {
  PasswordStoreBackend* bad_backend =
      InitializeWithDatabase(std::make_unique<BadLoginDatabase>());
  RunUntilIdle();

  testing::StrictMock<MockPasswordStoreBackendTester> tester;

  // Add a new autofillable login + a blocked login.
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormData());
  std::unique_ptr<PasswordForm> blocked_form(new PasswordForm(*form));
  blocked_form->signon_realm = "http://foo.example.com";
  blocked_form->url = GURL("http://foo.example.com/origin");
  blocked_form->action = GURL("http://foo.example.com/action");
  blocked_form->blocked_by_user = true;

  base::RepeatingCallback<void(const PasswordStoreChangeList&)> handle_changes =
      base::BindRepeating(&MockPasswordStoreBackendTester::HandleChanges,
                          base::Unretained(&tester));
  base::RepeatingCallback<void(LoginsResult)> handle_logins =
      base::BindRepeating(&MockPasswordStoreBackendTester::HandleLogins,
                          base::Unretained(&tester));

  EXPECT_CALL(tester, HandleChanges(IsEmpty()));
  bad_backend->AddLoginAsync(*form, handle_changes);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&tester);

  EXPECT_CALL(tester, HandleChanges(IsEmpty()));
  bad_backend->AddLoginAsync(*blocked_form, handle_changes);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&tester);

  // Get PSL matched logins; all logins; autofillable logins.
  EXPECT_CALL(tester, LoginsReceivedConstRef(IsEmpty()));
  bad_backend->FillMatchingLoginsAsync(handle_logins, true,
                                       {PasswordFormDigest(*form)});
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&tester);

  EXPECT_CALL(tester, LoginsReceivedConstRef(IsEmpty()));
  bad_backend->GetAutofillableLoginsAsync(handle_logins);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&tester);

  EXPECT_CALL(tester, LoginsReceivedConstRef(IsEmpty()));
  bad_backend->GetAllLoginsAsync(handle_logins);
  RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&tester);

  EXPECT_CALL(tester, HandleChanges(IsEmpty()));
  bad_backend->RemoveLoginAsync(*form, handle_changes);
  RunUntilIdle();
}

TEST_F(PasswordStoreImplTest, GetAllLoginsAsync) {
  static constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
       u"username_value_2", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
       u"username_value_3", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", u"", kTestLastUsageTime, 1},
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1}};
  PasswordStoreBackend* backend = Initialize();

  // Populate store with test credentials.
  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  base::MockCallback<PasswordStoreChangeListReply> reply;
  EXPECT_CALL(reply, Run).Times(6);
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(test_credential));
    // TODO(crbug.com/1217071): Call AddLoginAsync once it is implemented.
    // store()->AddLogin(*all_credentials.back());
    backend->AddLoginAsync(*all_credentials.back(), reply.Get());
  }
  RunUntilIdle();

  // Verify that the store returns all test credentials.
  MockPasswordStoreConsumer mock_consumer;
  std::vector<std::unique_ptr<PasswordForm>> expected_results;
  for (const auto& credential : all_credentials)
    expected_results.push_back(std::make_unique<PasswordForm>(*credential));
  base::MockCallback<LoginsReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(UnorderedPasswordFormElementsAre(&expected_results)));
  backend->GetAllLoginsAsync(mock_reply.Get());

  RunUntilIdle();
}

}  // namespace password_manager
