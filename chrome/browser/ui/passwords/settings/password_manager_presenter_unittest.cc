// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/test_sync_service.h"
#if !defined(OS_ANDROID)
#include "base/test/metrics/histogram_tester.h"
#endif
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/plaintext_reason.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Each;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Pair;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

constexpr char kExampleCom[] = "https://example.com/";
constexpr char kExampleOrg[] = "https://example.org/";
constexpr char kPassword[] = "pass";
constexpr char kPassword2[] = "pass2";
constexpr char kUsername[] = "user";
constexpr char kUsername2[] = "user2";
#if !defined(OS_ANDROID)
constexpr char kHistogramName[] = "PasswordManager.AccessPasswordInSettings";
#endif
MATCHER(IsNotBlocked, "") {
  return !arg->blocked_by_user;
}

MATCHER_P(HasUrl, url, "") {
  return arg->url == url;
}

// Ensures that all previously-started operations in the store have completed.
class PasswordStoreWaiter : public password_manager::PasswordStoreConsumer {
 public:
  explicit PasswordStoreWaiter(password_manager::PasswordStore* store);
  ~PasswordStoreWaiter() override = default;

  PasswordStoreWaiter(const PasswordStoreWaiter&) = delete;
  PasswordStoreWaiter& operator=(const PasswordStoreWaiter&) = delete;

 private:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>) override;

  base::RunLoop run_loop_;
};

PasswordStoreWaiter::PasswordStoreWaiter(
    password_manager::PasswordStore* store) {
  store->GetAllLoginsWithAffiliationAndBrandingInformation(this);
  run_loop_.Run();
}

void PasswordStoreWaiter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>) {
  run_loop_.Quit();
}

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_METHOD(password_manager::PasswordStore*,
              GetProfilePasswordStore,
              (),
              (const override));
  MOCK_METHOD(password_manager::PasswordStore*,
              GetAccountPasswordStore,
              (),
              (const override));
};

std::vector<std::pair<std::string, std::string>> GetUsernamesAndPasswords(
    const std::vector<password_manager::PasswordForm>& forms) {
  std::vector<std::pair<std::string, std::string>> result;
  result.reserve(forms.size());
  for (const auto& form : forms) {
    result.emplace_back(base::UTF16ToUTF8(form.username_value),
                        base::UTF16ToUTF8(form.password_value));
  }

  return result;
}

password_manager::PasswordForm AddPasswordToStore(
    password_manager::PasswordStore* store,
    const GURL& url,
    base::StringPiece username,
    base::StringPiece password) {
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = url.GetOrigin().spec();
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  store->AddLogin(form);
  return form;
}

std::vector<password_manager::PasswordForm> GetPasswordsInStoreForRealm(
    const password_manager::TestPasswordStore& store,
    base::StringPiece signon_realm) {
  const auto& stored_passwords = store.stored_passwords();
  auto for_realm_it = stored_passwords.find(signon_realm);
  return for_realm_it != stored_passwords.end()
             ? for_realm_it->second
             : std::vector<password_manager::PasswordForm>();
}

void SetUpSyncInTransportMode(Profile* profile) {
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile,
          base::BindRepeating(
              [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                return std::make_unique<syncer::TestSyncService>();
              })));
  CoreAccountInfo account;
  account.email = "foo@gmail.com";
  account.gaia = "foo";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service->SetAuthenticatedAccountInfo(account);
  sync_service->SetDisableReasons({});
  sync_service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service->SetIsAuthenticatedAccountPrimary(false);
  ASSERT_FALSE(sync_service->IsSyncFeatureEnabled());
}

}  // namespace

class PasswordManagerPresenterTest : public testing::Test {
 protected:
  explicit PasswordManagerPresenterTest(bool with_account_store = false) {
    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            PasswordStoreFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    &profile_,
                    base::BindRepeating(&password_manager::BuildPasswordStore<
                                        content::BrowserContext,
                                        password_manager::TestPasswordStore>))
                .get()));

    // The account store setup is done here and not in
    // PasswordManagerPresenterTestWithAccountStore to initialize the feature
    // list and testing factories as soon as possible.
    if (with_account_store) {
      feature_list_.InitAndEnableFeature(
          password_manager::features::kEnablePasswordsAccountStorage);
      account_store_ = base::WrapRefCounted(
          static_cast<password_manager::TestPasswordStore*>(
              AccountPasswordStoreFactory::GetInstance()
                  ->SetTestingFactoryAndUse(
                      &profile_,
                      base::BindRepeating(
                          &password_manager::BuildPasswordStoreWithArgs<
                              content::BrowserContext,
                              password_manager::TestPasswordStore,
                              password_manager::IsAccountStore>,
                          password_manager::IsAccountStore(true)))
                  .get()));

      SetUpSyncInTransportMode(&profile_);
    }
  }

  ~PasswordManagerPresenterTest() override {
    store_->ShutdownOnUIThread();
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
    }
    task_environment_.RunUntilIdle();
  }

  // TODO(victorvianna): Inline calls to this.
  password_manager::PasswordForm AddPasswordEntry(const GURL& url,
                                                  base::StringPiece username,
                                                  base::StringPiece password) {
    return AddPasswordToStore(store_.get(), url, username, password);
  }

  // TODO(victorvianna): Move to anonymous namespace taking store as argument.
  password_manager::PasswordForm AddPasswordException(const GURL& url) {
    password_manager::PasswordForm form;
    form.url = url;
    form.blocked_by_user = true;
    store_->AddLogin(form);
    return form;
  }

  void UpdatePasswordLists() {
    mock_controller_.GetPasswordManagerPresenter()->UpdatePasswordLists();
    task_environment_.RunUntilIdle();
  }

  MockPasswordUIView& GetUIController() { return mock_controller_; }

  // TODO(victorvianna): Inline this.
  std::vector<password_manager::PasswordForm> GetStoredPasswordsForRealm(
      base::StringPiece signon_realm) {
    return GetPasswordsInStoreForRealm(*store_, signon_realm);
  }

  password_manager::TestPasswordStore* profile_store() { return store_.get(); }
  password_manager::TestPasswordStore* account_store() {
    return account_store_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockPasswordUIView mock_controller_{&profile_};
  // TODO(victorvianna): Rename to profile_store_.
  scoped_refptr<password_manager::TestPasswordStore> store_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<password_manager::TestPasswordStore> account_store_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenterTest);
};

namespace {

TEST_F(PasswordManagerPresenterTest, UIControllerIsCalled) {
  EXPECT_CALL(GetUIController(), SetPasswordList(IsEmpty()));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();

  GURL pass_origin("http://abc1.com");
  AddPasswordEntry(pass_origin, "test@gmail.com", "test");
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();

  GURL except_origin("http://abc2.com");
  AddPasswordException(except_origin);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();

  GURL pass_origin2("http://example.com");
  AddPasswordEntry(pass_origin2, "test@gmail.com", "test");
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(2u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();
}

TEST_F(PasswordManagerPresenterTest, SavedPasswordsReturnedCorrectly) {
  password_manager::PasswordForm password1 =
      AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleOrg), kUsername, kPassword);
  UpdatePasswordLists();

  base::span<const std::unique_ptr<password_manager::PasswordForm>> passwords =
      GetUIController().GetPasswordManagerPresenter()->GetPasswordsForKey(
          password_manager::CreateSortKey(password1));

  ASSERT_EQ(1u, passwords.size());
  EXPECT_THAT(GetUsernamesAndPasswords({*passwords[0]}),
              ElementsAre(Pair(kUsername, kPassword)));
}

// Check that only stored passwords, not blocklisted entries, are provided for
// exporting.
TEST_F(PasswordManagerPresenterTest, BlocklistedPasswordsNotExported) {
  AddPasswordEntry(GURL("http://abc1.com"), "test@gmail.com", "test");
  AddPasswordException(GURL("http://abc2.com"));
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();

  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      passwords_for_export =
          GetUIController().GetPasswordManagerPresenter()->GetAllPasswords();
  EXPECT_EQ(1u, passwords_for_export.size());
  EXPECT_THAT(passwords_for_export, Each(IsNotBlocked()));
}

// Check that stored passwords are provided for exporting even if there is a
// blocklist entry for the same origin. This is needed to keep the user in
// control of all of their stored passwords.
TEST_F(PasswordManagerPresenterTest, BlocklistDoesNotPreventExporting) {
  const GURL kSameOrigin("https://abc.com");
  AddPasswordEntry(kSameOrigin, "test@gmail.com", "test");
  AddPasswordException(kSameOrigin);
  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1u)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(SizeIs(1u)));
  UpdatePasswordLists();

  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      passwords_for_export =
          GetUIController().GetPasswordManagerPresenter()->GetAllPasswords();
  ASSERT_EQ(1u, passwords_for_export.size());
  EXPECT_EQ(kSameOrigin, passwords_for_export[0]->url);
}

#if !defined(OS_ANDROID)
TEST_F(PasswordManagerPresenterTest, TestRequestPlaintextPassword) {
  base::HistogramTester histogram_tester;
  password_manager::PasswordForm form =
      AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);

  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  base::MockOnceCallback<void(base::Optional<std::u16string>)>
      password_callback;
  EXPECT_CALL(password_callback,
              Run(testing::Eq(base::ASCIIToUTF16(kPassword))));
  std::string sort_key = password_manager::CreateSortKey(form);
  GetUIController().GetPasswordManagerPresenter()->RequestPlaintextPassword(
      sort_key, password_manager::PlaintextReason::kView,
      password_callback.Get());

  histogram_tester.ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      1);
}

TEST_F(PasswordManagerPresenterTest, TestRequestPlaintextPasswordEdit) {
  base::HistogramTester histogram_tester;
  password_manager::PasswordForm form =
      AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);

  EXPECT_CALL(GetUIController(), SetPasswordList(SizeIs(1)));
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();
  base::MockOnceCallback<void(base::Optional<std::u16string>)>
      password_callback;
  EXPECT_CALL(password_callback,
              Run(testing::Eq(base::ASCIIToUTF16(kPassword))));
  std::string sort_key = password_manager::CreateSortKey(form);
  GetUIController().GetPasswordManagerPresenter()->RequestPlaintextPassword(
      sort_key, password_manager::PlaintextReason::kEdit,
      password_callback.Get());

  histogram_tester.ExpectUniqueSample(
      kHistogramName, password_manager::metrics_util::ACCESS_PASSWORD_EDITED,
      1);
}
#endif

TEST_F(PasswordManagerPresenterTest, TestPasswordRemovalAndUndo) {
  password_manager::PasswordForm password1 =
      AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  password_manager::PasswordForm password2 =
      AddPasswordEntry(GURL(kExampleCom), kUsername2, kPassword2);
  UpdatePasswordLists();
  ASSERT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword2)));

  GetUIController().GetPasswordManagerPresenter()->RemoveSavedPasswords(
      {password_manager::CreateSortKey(password1)});
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername2, kPassword2)));

  GetUIController()
      .GetPasswordManagerPresenter()
      ->UndoRemoveSavedPasswordOrException();
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordManagerPresenterTest, TestExceptionRemovalAndUndo) {
  password_manager::PasswordForm exception1 =
      AddPasswordException(GURL(kExampleCom));
  password_manager::PasswordForm exception2 =
      AddPasswordException(GURL(kExampleOrg));
  UpdatePasswordLists();

  GetUIController().GetPasswordManagerPresenter()->RemovePasswordExceptions(
      {password_manager::CreateSortKey(exception1)});
  EXPECT_CALL(
      GetUIController(),
      SetPasswordExceptionList(UnorderedElementsAre(HasUrl(exception2.url))));
  UpdatePasswordLists();

  GetUIController()
      .GetPasswordManagerPresenter()
      ->UndoRemoveSavedPasswordOrException();
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(UnorderedElementsAre(
                  HasUrl(exception1.url), HasUrl(exception2.url))));
  UpdatePasswordLists();
}

TEST_F(PasswordManagerPresenterTest, TestPasswordBatchRemovalAndUndo) {
  password_manager::PasswordForm password1 =
      AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  password_manager::PasswordForm password2 =
      AddPasswordEntry(GURL(kExampleCom), kUsername2, kPassword2);
  UpdatePasswordLists();
  ASSERT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword2)));

  GetUIController().GetPasswordManagerPresenter()->RemoveSavedPasswords(
      {password_manager::CreateSortKey(password1),
       password_manager::CreateSortKey(password2)});
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              IsEmpty());

  GetUIController()
      .GetPasswordManagerPresenter()
      ->UndoRemoveSavedPasswordOrException();
  UpdatePasswordLists();
  EXPECT_THAT(GetUsernamesAndPasswords(GetStoredPasswordsForRealm(kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword2)));
}

TEST_F(PasswordManagerPresenterTest, TestExceptionBatchRemovalAndUndo) {
  password_manager::PasswordForm exception1 =
      AddPasswordException(GURL(kExampleCom));
  password_manager::PasswordForm exception2 =
      AddPasswordException(GURL(kExampleOrg));
  UpdatePasswordLists();

  GetUIController().GetPasswordManagerPresenter()->RemovePasswordExceptions(
      {password_manager::CreateSortKey(exception1),
       password_manager::CreateSortKey(exception2)});
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList(IsEmpty()));
  UpdatePasswordLists();

  GetUIController()
      .GetPasswordManagerPresenter()
      ->UndoRemoveSavedPasswordOrException();
  EXPECT_CALL(GetUIController(),
              SetPasswordExceptionList(UnorderedElementsAre(
                  HasUrl(exception1.url), HasUrl(exception2.url))));
  UpdatePasswordLists();
}

class PasswordManagerPresenterTestWithAccountStore
    : public PasswordManagerPresenterTest {
 public:
  PasswordManagerPresenterTestWithAccountStore()
      : PasswordManagerPresenterTest(/*with_account_store=*/true) {
    ON_CALL(*(client_.GetPasswordFeatureManager()), IsOptedInForAccountStorage)
        .WillByDefault(Return(true));
    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(Return(profile_store()));
    ON_CALL(client_, GetAccountPasswordStore)
        .WillByDefault(Return(account_store()));
  }
  password_manager::PasswordManagerClient* client() { return &client_; }

 private:
  NiceMock<MockPasswordManagerClient> client_;
};

TEST_F(PasswordManagerPresenterTestWithAccountStore,
       TestMovePasswordsToAccountStore) {
  base::HistogramTester histogram_tester;

  // Fill the profile store with 3 entries of which 2 are in the same
  // equivalence class.
  password_manager::PasswordForm password =
      AddPasswordEntry(GURL(kExampleCom), kUsername, kPassword);
  AddPasswordEntry(GURL(kExampleCom).Resolve("someOtherPath"), kUsername,
                   kPassword);
  password_manager::PasswordForm password2 =
      AddPasswordEntry(GURL(kExampleCom), kUsername2, kPassword);
  // Since there are 2 stores, SetPasswordList() and SetPasswordExceptionList()
  // are called twice.
  EXPECT_CALL(GetUIController(), SetPasswordList).Times(2);
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList).Times(2);
  UpdatePasswordLists();
  ASSERT_THAT(GetUsernamesAndPasswords(
                  GetPasswordsInStoreForRealm(*profile_store(), kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));
  ASSERT_THAT(GetUsernamesAndPasswords(
                  GetPasswordsInStoreForRealm(*account_store(), kExampleCom)),
              IsEmpty());
  testing::Mock::VerifyAndClearExpectations(&GetUIController());

  // Move |password| to account and wait for stores to be updated.
  GetUIController().GetPasswordManagerPresenter()->MovePasswordsToAccountStore(
      {password_manager::CreateSortKey(password),
       password_manager::CreateSortKey(password2)},
      client());
  PasswordStoreWaiter profile_store_waiter(profile_store());
  PasswordStoreWaiter account_store_waiter(account_store());

  // All passwords should have moved.
  EXPECT_CALL(GetUIController(), SetPasswordList).Times(2);
  EXPECT_CALL(GetUIController(), SetPasswordExceptionList).Times(2);
  UpdatePasswordLists();
  EXPECT_THAT(GetPasswordsInStoreForRealm(*profile_store(), kExampleCom),
              IsEmpty());
  EXPECT_THAT(GetUsernamesAndPasswords(
                  GetPasswordsInStoreForRealm(*account_store(), kExampleCom)),
              UnorderedElementsAre(Pair(kUsername, kPassword),
                                   Pair(kUsername, kPassword),
                                   Pair(kUsername2, kPassword)));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted",
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredInSettings,
      2);
}

}  // namespace
