// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "https://example.org";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";

constexpr char16_t kPassword1[] = u"f00b4r";
constexpr char16_t kPassword2[] = u"s3cr3t";

using ::testing::_;
using ::testing::ByMove;
using ::testing::NiceMock;
using ::testing::Return;

MATCHER_P(CredentialsAre, credentials, "") {
  return base::ranges::equal(arg, credentials.get(),
                             [](const auto& lhs, const auto& rhs) {
                               return lhs.username() == rhs.username() &&
                                      lhs.password() == rhs.password();
                             });
}

MATCHER_P(SavedPasswordsAre, passwords, "") {
  return base::ranges::equal(
      arg, passwords, [](const auto& lhs, const auto& rhs) {
        return lhs.signon_realm == rhs.signon_realm &&
               lhs.username_value == rhs.username_value &&
               lhs.password_value == rhs.password_value;
      });
}

PasswordForm MakeSavedPassword(std::string_view signon_realm,
                               std::u16string_view username,
                               std::u16string_view password) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  return form;
}

LeakCheckCredential MakeLeakCheckCredential(std::u16string_view username,
                                            std::u16string_view password) {
  return LeakCheckCredential(std::u16string(username),
                             std::u16string(password));
}

struct MockBulkLeakCheck : BulkLeakCheck {
  MOCK_METHOD(void,
              CheckCredentials,
              (LeakDetectionInitiator, std::vector<LeakCheckCredential>),
              (override));
  MOCK_METHOD(size_t, GetPendingChecksCount, (), (const override));
};

using NiceMockBulkLeakCheck = ::testing::NiceMock<MockBulkLeakCheck>;

class BulkLeakCheckServiceAdapterTest : public testing::Test {
 public:
  BulkLeakCheckServiceAdapterTest() {
    auto factory = std::make_unique<MockLeakDetectionCheckFactory>();
    factory_ = factory.get();
    service_.set_leak_factory(std::move(factory));
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    prefs_.registry()->RegisterBooleanPref(prefs::kPasswordLeakDetectionEnabled,
                                           true);
    prefs_.registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnabled, true);
    prefs_.registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnhanced,
                                           false);
    presenter_.Init();
    RunUntilIdle();
  }

  ~BulkLeakCheckServiceAdapterTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }
  MockLeakDetectionCheckFactory& factory() { return *factory_; }
  PrefService& prefs() { return prefs_; }
  BulkLeakCheckServiceAdapter& adapter() { return adapter_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  affiliations::FakeAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, store_,
                                     /*account_store=*/nullptr};
  BulkLeakCheckService service_{
      identity_test_env_.identity_manager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>()};
  raw_ptr<MockLeakDetectionCheckFactory> factory_ = nullptr;
  TestingPrefServiceSimple prefs_;
  BulkLeakCheckServiceAdapter adapter_{&presenter_, &service_, &prefs_};
};

}  // namespace

TEST_F(BulkLeakCheckServiceAdapterTest, OnCreation) {
  EXPECT_EQ(0u, adapter().GetPendingChecksCount());
  EXPECT_EQ(BulkLeakCheckService::State::kIdle,
            adapter().GetBulkLeakCheckState());
}

// Checks that starting a leak check correctly transforms the list of saved
// passwords into LeakCheckCredentials and attaches the underlying password
// forms as user data.
TEST_F(BulkLeakCheckServiceAdapterTest, StartBulkLeakCheck) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1),
      MakeSavedPassword(kExampleOrg, kUsername2, kPassword2)};
  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  std::vector<LeakCheckCredential> credentials;
  EXPECT_CALL(
      *leak_check,
      CheckCredentials(LeakDetectionInitiator::kBulkSyncedPasswordsCheck, _))
      .WillOnce(MoveArg<1>(&credentials));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck,
      /*key=*/nullptr, /*data=*/nullptr);

  std::vector<LeakCheckCredential> expected;
  expected.push_back(MakeLeakCheckCredential(kUsername1, kPassword1));
  expected.push_back(MakeLeakCheckCredential(kUsername2, kPassword2));

  EXPECT_THAT(credentials, CredentialsAre(std::cref(expected)));
}

TEST_F(BulkLeakCheckServiceAdapterTest, StartBulkLeakCheckAttachesData) {
  constexpr char kKey[] = "key";
  struct UserData : LeakCheckCredential::Data {
    std::unique_ptr<Data> Clone() override { return std::make_unique<Data>(); }
  } data;

  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1)};
  store().AddLogin(passwords[0]);
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  std::vector<LeakCheckCredential> credentials;
  EXPECT_CALL(
      *leak_check,
      CheckCredentials(LeakDetectionInitiator::kBulkSyncedPasswordsCheck, _))
      .WillOnce(MoveArg<1>(&credentials));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck(
      LeakDetectionInitiator::kBulkSyncedPasswordsCheck, kKey, &data);

  EXPECT_NE(nullptr, credentials.at(0).GetUserData(kKey));
}

// Tests that multiple credentials with effectively the same username are
// correctly deduped before starting the leak check.
TEST_F(BulkLeakCheckServiceAdapterTest, StartBulkLeakCheckDedupes) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kExampleCom, u"alice", kPassword1),
      MakeSavedPassword(kExampleCom, u"ALICE", kPassword1),
      MakeSavedPassword(kExampleCom, u"Alice@example.com", kPassword1)};

  store().AddLogin(passwords[0]);
  store().AddLogin(passwords[1]);
  store().AddLogin(passwords[2]);
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  std::vector<LeakCheckCredential> credentials;
  EXPECT_CALL(*leak_check,
              CheckCredentials(
                  LeakDetectionInitiator::kDesktopProactivePasswordCheckup, _))
      .WillOnce(MoveArg<1>(&credentials));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck(
      LeakDetectionInitiator::kDesktopProactivePasswordCheckup,
      /*key=*/nullptr, /*data=*/nullptr);

  std::vector<LeakCheckCredential> expected;
  expected.push_back(MakeLeakCheckCredential(u"alice", kPassword1));
  EXPECT_THAT(credentials, CredentialsAre(std::cref(expected)));
}

// Checks that trying to start a leak check when another check is already
// running does nothing and returns false to the caller.
TEST_F(BulkLeakCheckServiceAdapterTest, MultipleStarts) {
  store().AddLogin(MakeSavedPassword(kExampleCom, u"alice", kPassword1));
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  auto& leak_check_ref = *leak_check;
  EXPECT_CALL(leak_check_ref, CheckCredentials);
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  EXPECT_TRUE(adapter().StartBulkLeakCheck(
      LeakDetectionInitiator::kDesktopProactivePasswordCheckup));

  EXPECT_CALL(leak_check_ref, CheckCredentials).Times(0);
  EXPECT_FALSE(adapter().StartBulkLeakCheck(
      LeakDetectionInitiator::kDesktopProactivePasswordCheckup));
}

// Checks that stopping the leak check correctly resets the state of the bulk
// leak check.
TEST_F(BulkLeakCheckServiceAdapterTest, StopBulkLeakCheck) {
  store().AddLogin(MakeSavedPassword(kExampleCom, u"alice", kPassword1));
  RunUntilIdle();

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  EXPECT_CALL(*leak_check, CheckCredentials);
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  adapter().StartBulkLeakCheck(
      LeakDetectionInitiator::kDesktopProactivePasswordCheckup);
  EXPECT_EQ(BulkLeakCheckService::State::kRunning,
            adapter().GetBulkLeakCheckState());

  adapter().StopBulkLeakCheck();
  EXPECT_EQ(BulkLeakCheckService::State::kCanceled,
            adapter().GetBulkLeakCheckState());
}

// Tests that editing a password through the presenter does not result in
// another call to CheckCredentials with a corresponding change to the checked
// password if the corresponding prefs are not set.
TEST_F(BulkLeakCheckServiceAdapterTest, OnEditedNoPrefs) {
  prefs().SetBoolean(prefs::kPasswordLeakDetectionEnabled, false);
  prefs().SetBoolean(::prefs::kSafeBrowsingEnabled, false);

  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.in_store = PasswordForm::Store::kProfileStore;
  store().AddLogin(password);
  RunUntilIdle();

  EXPECT_CALL(factory(), TryCreateBulkLeakCheck).Times(0);
  CredentialUIEntry original_credential(password),
      updated_credential = original_credential;
  updated_credential.password = kPassword2;
  presenter().EditSavedCredentials(original_credential, updated_credential);
  RunUntilIdle();
}

// Tests that editing a password through the presenter will result in another
// call to CheckCredentials with a corresponding change to the checked password
// if the corresponding prefs are set.
TEST_F(BulkLeakCheckServiceAdapterTest, OnEditedWithPrefs) {
  PasswordForm password =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  password.in_store = PasswordForm::Store::kProfileStore;
  store().AddLogin(password);
  RunUntilIdle();

  std::vector<LeakCheckCredential> expected;
  expected.push_back(MakeLeakCheckCredential(kUsername1, kPassword2));

  auto leak_check = std::make_unique<NiceMockBulkLeakCheck>();
  EXPECT_CALL(*leak_check,
              CheckCredentials(LeakDetectionInitiator::kEditCheck,
                               CredentialsAre(std::cref(expected))));
  EXPECT_CALL(factory(), TryCreateBulkLeakCheck)
      .WillOnce(Return(ByMove(std::move(leak_check))));
  CredentialUIEntry original_credential(password),
      updated_credential = original_credential;
  updated_credential.password = kPassword2;
  presenter().EditSavedCredentials(original_credential, updated_credential);
  RunUntilIdle();
}

}  // namespace password_manager
