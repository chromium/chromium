// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/passwords_private/mock_passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

class MockPage : public mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  MOCK_METHOD(void,
              OnPasswordAutomaticChangeStateUpdated,
              (int32_t credential_id,
               mojom::PasswordAutomaticChangeState state),
              (override));

  MOCK_METHOD(void,
              OnPasswordsExportProgress,
              (password_manager::mojom::ExportProgressStatus,
               const std::optional<std::string>&),
              (override));

  mojo::PendingRemote<mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::Page> receiver_{this};
};

// A RAII helper that waits for the SavedPasswordsPresenter to notify that
// passwords have changed.
class SavedPasswordsChangedWaiter : public SavedPasswordsPresenter::Observer {
 public:
  explicit SavedPasswordsChangedWaiter(SavedPasswordsPresenter* presenter)
      : presenter_(presenter) {
    presenter_->AddObserver(this);
  }

  ~SavedPasswordsChangedWaiter() override { presenter_->RemoveObserver(this); }

  // Blocks until OnSavedPasswordsChanged is called.
  void Wait() { ASSERT_TRUE(future_.Wait()); }

 private:
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(
      const PasswordStoreChangeList& changes) override {
    future_.SetValue();
  }

  const raw_ptr<SavedPasswordsPresenter> presenter_;
  base::test::TestFuture<void> future_;
};

class MockPasswordChangeService : public ChromePasswordChangeService {
 public:
  MockPasswordChangeService()
      : ChromePasswordChangeService(/*pref_service*/ nullptr,
                                    /*affiliation_service=*/nullptr,
                                    /*optimization_keyed_service=*/nullptr,
                                    /*settings_service=*/nullptr,
                                    /*feature_manager=*/nullptr,
                                    /*log_router*/ nullptr) {}

  MOCK_METHOD(void,
              StartPasswordChangeFromCheckup,
              (const password_manager::CredentialUIEntry&,
               content::WebContents*,
               PasswordChangeFromCheckupDelegate::StateChangeCallback),
              (override));
};

}  // namespace

class PasswordManagerUIHandlerUnitTest : public testing::Test {
 public:
  PasswordManagerUIHandlerUnitTest()
      : profile_(std::make_unique<TestingProfile>()),
        web_contents_(factory_.CreateWebContents(profile_.get())),
        password_store_(CreateAndUseTestPasswordStore(profile_.get())),
        affiliation_service_(
            std::make_unique<affiliations::FakeAffiliationService>()) {}

  ~PasswordManagerUIHandlerUnitTest() override = default;

  void SetUp() override {
    // Set up the delegate dependency.
    auto delegate =
        base::MakeRefCounted<NiceMock<MockPasswordsPrivateDelegate>>();
    mock_delegate_ = delegate.get();

    // Create the handler under test.
    handler_ = std::make_unique<PasswordManagerUIHandler>(
        mojo::PendingReceiver<mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), std::move(delegate), web_contents_);

    // Ensure the Mojo connection is established.
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  void InitPresenter() {
    auto presenter = std::make_unique<SavedPasswordsPresenter>(
        affiliation_service_.get(), password_store_,
        /*account_store=*/nullptr);
    presenter_ = presenter.get();
    owned_presenter_ = std::move(presenter);

    // Initialize the presenter and wait for it to complete.
    base::test::TestFuture<void> init_future;
    presenter_->Init(init_future.GetCallback());
    ASSERT_TRUE(init_future.Wait());

    // Transfer presenter ownership to the delegate.
    ON_CALL(*mock_delegate_, GetSavedPasswordsPresenter())
        .WillByDefault(Return(presenter_));
  }

  void TearDown() override {
    mock_delegate_ = nullptr;
    presenter_ = nullptr;
    handler_.reset();
    owned_presenter_.reset();
    testing::Test::TearDown();
  }

  // Helper to inject a password form into the store and wait for the update.
  void CreateAndSeedPasswordForm(const GURL& url,
                                 const std::u16string& username,
                                 bool actor_login_approved) {
    PasswordForm form;
    form.url = url;
    form.signon_realm = url.spec();
    form.username_value = username;
    form.actor_login_approved = actor_login_approved;
    form.in_store = PasswordForm::Store::kProfileStore;

    SavedPasswordsChangedWaiter waiter(presenter_);
    password_store_->AddLogin(password_manager::FromPasswordForm(form));
    waiter.Wait();
  }

  PasswordManagerUIHandler& handler() { return *handler_; }
  NiceMock<MockPasswordsPrivateDelegate>& mock_delegate() {
    return *mock_delegate_;
  }

 protected:
  // NOTE: The initialization order of these members matters for construction
  // and destruction.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory factory_;
  // Weak ptr owned by factory_.
  raw_ptr<content::WebContents> web_contents_;
  NiceMock<MockPage> mock_page_;
  scoped_refptr<TestPasswordStore> password_store_;
  std::unique_ptr<affiliations::FakeAffiliationService> affiliation_service_;

  std::unique_ptr<SavedPasswordsPresenter> owned_presenter_;
  std::unique_ptr<PasswordManagerUIHandler> handler_;

  // These are raw pointers to objects owned by the handler_'s delegate and
  // owned_presenter_. They are valid between SetUp() and TearDown().
  raw_ptr<NiceMock<MockPasswordsPrivateDelegate>> mock_delegate_;
  raw_ptr<SavedPasswordsPresenter> presenter_;
};

TEST_F(PasswordManagerUIHandlerUnitTest,
       DeleteAllPasswordManagerData_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), DeleteAllPasswordManagerData(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));

  handler().DeleteAllPasswordManagerData(future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest, ExtendAuthValidity_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), RestartAuthTimer());

  handler().ExtendAuthValidity();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       CopyPlaintextBackupPassword_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), CopyPlaintextBackupPassword(0, _))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  handler().CopyPlaintextBackupPassword(0, future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest, RemoveBackupPassword_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), RemoveBackupPassword(0));

  handler().RemoveBackupPassword(0);
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       GetActorLoginPermissionSites_CallsPresenter) {
  InitPresenter();
  const GURL kTestUrl("https://test.com");
  const std::u16string kTestUsername = u"testuser";
  CreateAndSeedPasswordForm(kTestUrl, kTestUsername,
                            /*actor_login_approved=*/true);

  base::test::TestFuture<std::vector<mojom::ActorLoginPermissionPtr>> future;
  handler().GetActorLoginPermissions(future.GetCallback());

  const auto& permissions = future.Get();
  ASSERT_EQ(permissions.size(), 1u);
  EXPECT_EQ(permissions[0]->domain_info->url, kTestUrl);
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       RevokeActorLoginPermission_CallsPresenter) {
  InitPresenter();
  const GURL kTestUrl("https://test.com");
  const std::u16string kTestUsername = u"testuser";
  CreateAndSeedPasswordForm(kTestUrl, kTestUsername,
                            /*actor_login_approved=*/true);

  auto site = mojom::ActorLoginPermission::New();
  site->domain_info = mojom::DomainInfo::New(
      /*human_redable_name*/ std::string(kTestUrl.host()),
      /*url=*/kTestUrl,
      /*signon_realm=*/kTestUrl.spec());
  site->username = base::UTF16ToUTF8(kTestUsername);

  SavedPasswordsChangedWaiter waiter(presenter_);
  handler().RevokeActorLoginPermission(std::move(site));
  waiter.Wait();

  ASSERT_EQ(GetAllLoginsSync(password_store_.get()).size(), 1u);
  std::vector<password_manager::PasswordForm> passwords =
      GetAllLoginsSync(password_store_.get()).begin()->second;
  ASSERT_EQ(passwords.size(), 1u);
  EXPECT_FALSE(passwords[0].actor_login_approved);
}

TEST_F(PasswordManagerUIHandlerUnitTest, ShowAddShortcutDialog_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), ShowAddShortcutDialog(_));

  handler().ShowAddShortcutDialog();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       ChangePasswordManagerPin_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), ChangePasswordManagerPin(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  handler().ChangePasswordManagerPin(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       IsPasswordManagerPinAvailable_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), IsPasswordManagerPinAvailable(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(false));

  handler().IsPasswordManagerPinAvailable(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       SwitchBiometricAuthBeforeFillingState_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), SwitchBiometricAuthBeforeFillingState(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));

  handler().SwitchBiometricAuthBeforeFillingState(future.GetCallback());

  EXPECT_TRUE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       IsAccountStorageActive_ReturnsCorrectValue) {
  for (bool is_active : {true, false}) {
    EXPECT_CALL(mock_delegate(), IsAccountStorageActive())
        .WillOnce(Return(is_active));

    base::test::TestFuture<bool> future;
    handler().IsAccountStorageActive(future.GetCallback());
    EXPECT_EQ(is_active, future.Get());
  }
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       SetAccountStorageEnabled_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), SetAccountStorageEnabled(true));

  handler().SetAccountStorageEnabled(true);
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       ShouldShowAccountStorageSettingToggle_CallsDelegate) {
  for (bool should_show : {true, false}) {
    EXPECT_CALL(mock_delegate(), ShouldShowAccountStorageSettingToggle())
        .WillOnce(Return(should_show));

    base::test::TestFuture<bool> future;
    handler().ShouldShowAccountStorageSettingToggle(future.GetCallback());
    EXPECT_EQ(should_show, future.Get());
  }
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       GetPasswordManagerActionableError_ReturnsCorrectValue) {
  EXPECT_CALL(mock_delegate(), GetActionableError())
      .WillOnce(
          Return(password_manager::ActionableError::kTrustedVaultKeyNeeded));

  base::test::TestFuture<mojom::PasswordManagerActionableError> future;
  handler().GetPasswordManagerActionableError(future.GetCallback());

  EXPECT_EQ(future.Get(),
            mojom::PasswordManagerActionableError::kTrustedVaultKeyNeeded);
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       ShowLastExportedFileInShell_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), ShowLastExportedFileInShell(_));

  handler().ShowLastExportedFileInShell();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       DisconnectCloudAuthenticator_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), DisconnectCloudAuthenticator(_))
      .WillOnce(base::test::RunOnceCallback<0>(false));

  handler().DisconnectCloudAuthenticator(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       IsConnectedToCloudAuthenticator_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_CALL(mock_delegate(), IsConnectedToCloudAuthenticator())
      .WillOnce(Return(false));

  handler().IsConnectedToCloudAuthenticator(future.GetCallback());

  EXPECT_FALSE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       UndoRemoveSavedPasswordOrException_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), UndoRemoveSavedPasswordOrException());

  handler().UndoRemoveSavedPasswordOrException();
}

TEST_F(PasswordManagerUIHandlerUnitTest, RequestPasswordsExport_CallsDelegate) {
  base::test::TestFuture<mojom::ExportPasswordsResult> future;
  EXPECT_CALL(mock_delegate(), ExportPasswords(_, _))
      .WillOnce(
          base::test::RunOnceCallback<0>(extensions::PasswordsPrivateDelegate::
                                             ExportPasswordsResult::kSuccess));

  handler().RequestPasswordsExport(future.GetCallback());

  EXPECT_EQ(mojom::ExportPasswordsResult::kSuccess, future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       GetPasswordsExportProgress_CallsDelegate) {
  base::test::TestFuture<mojom::ExportProgressStatus> future;
  EXPECT_CALL(mock_delegate(), GetExportProgressStatus())
      .WillOnce(Return(extensions::api::passwords_private::
                           ExportProgressStatus::kInProgress));

  handler().GetPasswordsExportProgress(future.GetCallback());

  EXPECT_EQ(mojom::ExportProgressStatus::kInProgress, future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       OnPasswordsExportProgress_CallsMojoObserver) {
  EXPECT_CALL(mock_page_, OnPasswordsExportProgress(
                              mojom::ExportProgressStatus::kSucceeded,
                              Eq(std::optional<std::string>("folder"))));

  handler().OnPasswordsExportProgress(
      password_manager::ExportProgressStatus::kSucceeded, "folder");
  mock_page_.FlushForTesting();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       RemovePasswordException_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), RemovePasswordException(42));

  handler().RemovePasswordException(42);
}

TEST_F(PasswordManagerUIHandlerUnitTest, StartBulkPasswordCheck_CallsDelegate) {
  EXPECT_CALL(mock_delegate(), StartPasswordCheck(_));

  handler().StartBulkPasswordCheck();
}

TEST_F(PasswordManagerUIHandlerUnitTest, MovePasswordsToAccount_CallsDelegate) {
  const std::vector<int32_t> kIds = {1, 2, 3};
  EXPECT_CALL(mock_delegate(), MovePasswordsToAccount(kIds));

  handler().MovePasswordsToAccount(kIds);
}

TEST_F(PasswordManagerUIHandlerUnitTest, ResetImporter_CallsDelegate) {
  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_delegate(), ResetImporter(true));

  handler().ResetImporter(/*delete_file=*/true, future.GetCallback());

  EXPECT_TRUE(future.Wait());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       StartPasswordChange_CallsServiceAndUpdatesState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordCheckupPrototype);

  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockPasswordChangeService>>();
      }));

  auto* mock_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile_.get()));
  ASSERT_TRUE(mock_service);

  CredentialUIEntry credential;
  credential.username = u"testuser";
  const int kCredentialId = 123;
  EXPECT_CALL(mock_delegate(), GetCredentialFromId(kCredentialId))
      .WillRepeatedly(Return(credential));

  PasswordChangeFromCheckupDelegate::StateChangeCallback captured_callback;
  EXPECT_CALL(*mock_service, StartPasswordChangeFromCheckup(
                                 testing::_, web_contents_.get(), testing::_))
      .WillOnce(testing::SaveArg<2>(&captured_callback));

  handler().StartPasswordChange(kCredentialId);

  ASSERT_TRUE(captured_callback);

  EXPECT_CALL(mock_page_,
              OnPasswordAutomaticChangeStateUpdated(
                  kCredentialId,
                  mojom::PasswordAutomaticChangeState::kChangingPassword));

  captured_callback.Run(PasswordChangeFromCheckupDelegate::
                            PasswordAutomaticChangeState::kChangingPassword);
  mock_page_.FlushForTesting();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       StartPasswordChange_InvalidCredentialId_DoesNotCallService) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordCheckupPrototype);

  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockPasswordChangeService>>();
      }));

  auto* mock_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile_.get()));
  ASSERT_TRUE(mock_service);

  EXPECT_CALL(*mock_service, StartPasswordChangeFromCheckup).Times(0);
  EXPECT_CALL(mock_page_, OnPasswordAutomaticChangeStateUpdated).Times(0);

  const int kInvalidCredentialId = 999;
  handler().StartPasswordChange(kInvalidCredentialId);
  mock_page_.FlushForTesting();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       StartPasswordChange_NullService_DoesNotCrash) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordCheckupPrototype);

  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return nullptr;
      }));

  CredentialUIEntry credential;
  credential.username = u"testuser";
  const int kCredentialId = 123;
  EXPECT_CALL(mock_delegate(), GetCredentialFromId(kCredentialId))
      .WillRepeatedly(Return(credential));

  EXPECT_CALL(mock_page_, OnPasswordAutomaticChangeStateUpdated).Times(0);

  handler().StartPasswordChange(kCredentialId);
  mock_page_.FlushForTesting();
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       StartPasswordChange_MultipleStateTransitions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordCheckupPrototype);

  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating([](content::BrowserContext* context)
                                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockPasswordChangeService>>();
      }));

  auto* mock_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile_.get()));
  ASSERT_TRUE(mock_service);

  CredentialUIEntry credential;
  credential.username = u"testuser";
  const int kCredentialId = 123;
  EXPECT_CALL(mock_delegate(), GetCredentialFromId(kCredentialId))
      .WillRepeatedly(Return(credential));

  PasswordChangeFromCheckupDelegate::StateChangeCallback captured_callback;
  EXPECT_CALL(*mock_service, StartPasswordChangeFromCheckup(
                                 testing::_, web_contents_.get(), testing::_))
      .WillOnce(testing::SaveArg<2>(&captured_callback));

  handler().StartPasswordChange(kCredentialId);
  ASSERT_TRUE(captured_callback);

  testing::InSequence s;
  EXPECT_CALL(mock_page_,
              OnPasswordAutomaticChangeStateUpdated(
                  kCredentialId, mojom::PasswordAutomaticChangeState::kInactive));
  EXPECT_CALL(mock_page_,
              OnPasswordAutomaticChangeStateUpdated(
                  kCredentialId,
                  mojom::PasswordAutomaticChangeState::kAttemptingSignIn));
  EXPECT_CALL(mock_page_,
              OnPasswordAutomaticChangeStateUpdated(
                  kCredentialId,
                  mojom::PasswordAutomaticChangeState::kChangingPassword));
  EXPECT_CALL(
      mock_page_,
      OnPasswordAutomaticChangeStateUpdated(
          kCredentialId,
          mojom::PasswordAutomaticChangeState::kConfirmingChangedPassword));
  EXPECT_CALL(
      mock_page_,
      OnPasswordAutomaticChangeStateUpdated(
          kCredentialId,
          mojom::PasswordAutomaticChangeState::kPasswordChangedSuccessfully));
  EXPECT_CALL(mock_page_,
              OnPasswordAutomaticChangeStateUpdated(
                  kCredentialId, mojom::PasswordAutomaticChangeState::kError));

  captured_callback.Run(
      PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::kInactive);
  captured_callback.Run(PasswordChangeFromCheckupDelegate::
                            PasswordAutomaticChangeState::kAttemptingSignIn);
  captured_callback.Run(PasswordChangeFromCheckupDelegate::
                            PasswordAutomaticChangeState::kChangingPassword);
  captured_callback.Run(
      PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kConfirmingChangedPassword);
  captured_callback.Run(
      PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::
          kPasswordChangedSuccessfully);
  captured_callback.Run(
      PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState::kError);
  mock_page_.FlushForTesting();
}

}  // namespace password_manager
