// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/os_crypt_async_migrator.h"

#include <string_view>

#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

PasswordForm CreateForm(std::string_view signon_realm) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.password_value = u"password";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class MockOSCryptAsyncMigratorObserver : public OSCryptAsyncMigrator::Observer {
 public:
  MockOSCryptAsyncMigratorObserver() = default;
  ~MockOSCryptAsyncMigratorObserver() override = default;

  MOCK_METHOD(void, CleaningCompleted, (), (override));
};

}  // namespace

class OSCryptAsyncMigratorTest
    : public testing::Test,
      public testing::WithParamInterface<password_manager::IsAccountStore> {
 public:
  OSCryptAsyncMigratorTest() = default;
  ~OSCryptAsyncMigratorTest() override = default;

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kAccountStoreMigratedToOSCryptAsync, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kProfileStoreMigratedToOSCryptAsync, false);
    store_ =
        base::MakeRefCounted<testing::NiceMock<MockPasswordStoreInterface>>();
    migrator_ =
        std::make_unique<OSCryptAsyncMigrator>(store_, GetParam(), &prefs_);
    feature_list_.InitWithFeatures(
        {features::kUseAsyncOsCryptInLoginDatabase,
         features::kUseNewEncryptionMethod,
         features::kEncryptAllPasswordsWithOSCryptAsync},
        {});
  }

  MockPasswordStoreInterface* store() { return store_.get(); }
  TestingPrefServiceSimple& prefs() { return prefs_; }
  OSCryptAsyncMigrator* migrator() { return migrator_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  scoped_refptr<MockPasswordStoreInterface> store_;
  std::unique_ptr<OSCryptAsyncMigrator> migrator_;
};

TEST_P(OSCryptAsyncMigratorTest, DoesNotNeedCleaningWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kEncryptAllPasswordsWithOSCryptAsync);

  ASSERT_FALSE(prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
  ASSERT_FALSE(prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));

  EXPECT_FALSE(migrator()->NeedsCleaning());
}

TEST_P(OSCryptAsyncMigratorTest, DoesNotNeedCleaningWhenCleanedBefore) {
  prefs().SetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync, true);
  prefs().SetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync, true);

  EXPECT_FALSE(migrator()->NeedsCleaning());
}

TEST_P(OSCryptAsyncMigratorTest, NeedsCleaning) {
  ASSERT_FALSE(prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
  ASSERT_FALSE(prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));

  EXPECT_TRUE(migrator()->NeedsCleaning());
}

TEST_P(OSCryptAsyncMigratorTest, StartCleaningEmptyStore) {
  MockOSCryptAsyncMigratorObserver observer;
  EXPECT_CALL(*store(), GetAutofillableLogins);
  migrator()->StartCleaning(&observer);

  EXPECT_CALL(observer, CleaningCompleted);
  static_cast<PasswordStoreConsumer*>(migrator())
      ->OnGetPasswordStoreResultsOrErrorFrom(store(), LoginsResult());

  if (GetParam() == password_manager::kAccountStore) {
    ASSERT_TRUE(prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
    ASSERT_FALSE(
        prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));
  } else {
    ASSERT_FALSE(
        prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
    ASSERT_TRUE(prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));
  }
}

TEST_P(OSCryptAsyncMigratorTest, StartCleaningErrorInStore) {
  MockOSCryptAsyncMigratorObserver observer;
  EXPECT_CALL(*store(), GetAutofillableLogins);
  migrator()->StartCleaning(&observer);

  EXPECT_CALL(observer, CleaningCompleted);
  PasswordStoreBackendError error{
      PasswordStoreBackendErrorType::kUncategorized};
  static_cast<PasswordStoreConsumer*>(migrator())
      ->OnGetPasswordStoreResultsOrErrorFrom(store(), error);

  ASSERT_FALSE(prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
  ASSERT_FALSE(prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));
}

TEST_P(OSCryptAsyncMigratorTest, StartCleaningHasPasswords) {
  MockOSCryptAsyncMigratorObserver observer;
  EXPECT_CALL(*store(), GetAutofillableLogins);
  migrator()->StartCleaning(&observer);

  std::vector<PasswordForm> forms = {
      CreateForm("https://test.com/"),
      CreateForm("https://example.com/"),
  };

  base::OnceClosure completion_callback;
  EXPECT_CALL(*store(),
              UpdateLogins(testing::ElementsAreArray(forms), testing::_))
      .WillOnce(MoveArg<1>(&completion_callback));
  static_cast<PasswordStoreConsumer*>(migrator())
      ->OnGetPasswordStoreResultsOrErrorFrom(store(), forms);

  EXPECT_CALL(observer, CleaningCompleted);
  std::move(completion_callback).Run();

  if (GetParam() == password_manager::kAccountStore) {
    ASSERT_TRUE(prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
    ASSERT_FALSE(
        prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));
  } else {
    ASSERT_FALSE(
        prefs().GetBoolean(prefs::kAccountStoreMigratedToOSCryptAsync));
    ASSERT_TRUE(prefs().GetBoolean(prefs::kProfileStoreMigratedToOSCryptAsync));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         OSCryptAsyncMigratorTest,
                         testing::Values(password_manager::kAccountStore,
                                         password_manager::kProfileStore));

}  // namespace password_manager
