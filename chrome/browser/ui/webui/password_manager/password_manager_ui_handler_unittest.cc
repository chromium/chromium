// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
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

namespace {

using testing::Return;

class MockPage : public mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

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
    // Set up the delegate and presenter dependencies.
    auto delegate =
        base::MakeRefCounted<extensions::TestPasswordsPrivateDelegate>();
    test_delegate_ = delegate.get();

    auto presenter = std::make_unique<SavedPasswordsPresenter>(
        affiliation_service_.get(), password_store_,
        /*account_store=*/nullptr);
    presenter_ = presenter.get();

    // Initialize the presenter and wait for it to complete.
    base::test::TestFuture<void> init_future;
    presenter_->Init(init_future.GetCallback());
    ASSERT_TRUE(init_future.Wait());

    // Transfer presenter ownership to the delegate.
    delegate->SetSavedPasswordsPresenter(std::move(presenter));

    // Create the handler under test.
    handler_ = std::make_unique<PasswordManagerUIHandler>(
        mojo::PendingReceiver<mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), std::move(delegate), web_contents_);

    // Ensure the Mojo connection is established.
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  void TearDown() override {
    test_delegate_ = nullptr;
    presenter_ = nullptr;
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
    password_store_->AddLogin(form);
    waiter.Wait();
  }

  PasswordManagerUIHandler& handler() { return *handler_; }
  extensions::TestPasswordsPrivateDelegate& test_delegate() {
    return *test_delegate_;
  }

 protected:
  // NOTE: The initialization order of these members matters for construction
  // and destruction.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory factory_;
  // Weak ptr owned by factory_.
  raw_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockPage> mock_page_;
  scoped_refptr<TestPasswordStore> password_store_;
  std::unique_ptr<affiliations::FakeAffiliationService> affiliation_service_;

  // These are raw pointers to objects owned by the handler_'s delegate.
  // They are valid between SetUp() and TearDown().
  raw_ptr<extensions::TestPasswordsPrivateDelegate> test_delegate_;
  raw_ptr<SavedPasswordsPresenter> presenter_;

  std::unique_ptr<PasswordManagerUIHandler> handler_;
};

TEST_F(PasswordManagerUIHandlerUnitTest,
       DeleteAllPasswordManagerData_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_FALSE(test_delegate().get_delete_all_password_manager_data_called());

  handler().DeleteAllPasswordManagerData(future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(test_delegate().get_delete_all_password_manager_data_called());
}

TEST_F(PasswordManagerUIHandlerUnitTest, ExtendAuthValidity_CallsDelegate) {
  EXPECT_FALSE(test_delegate().get_authenticator_interaction_status());

  handler().ExtendAuthValidity();

  EXPECT_TRUE(test_delegate().get_authenticator_interaction_status());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       CopyPlaintextBackupPassword_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_FALSE(test_delegate().copy_plaintext_backup_password());

  handler().CopyPlaintextBackupPassword(0, future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(test_delegate().copy_plaintext_backup_password());
}

TEST_F(PasswordManagerUIHandlerUnitTest, RemoveBackupPassword_CallsDelegate) {
  EXPECT_FALSE(test_delegate().remove_backup_password());

  handler().RemoveBackupPassword(0);

  EXPECT_TRUE(test_delegate().remove_backup_password());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       GetActorLoginPermissionSites_CallsPresenter) {
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

  ASSERT_EQ(password_store_->stored_passwords().size(), 1u);
  const auto& passwords = password_store_->stored_passwords().begin()->second;
  ASSERT_EQ(passwords.size(), 1u);
  EXPECT_FALSE(passwords[0].actor_login_approved);
}

TEST_F(PasswordManagerUIHandlerUnitTest, ShowAddShortcutDialog_CallsDelegate) {
  EXPECT_FALSE(test_delegate().get_add_shortcut_dialog_shown());

  handler().ShowAddShortcutDialog();

  EXPECT_TRUE(test_delegate().get_add_shortcut_dialog_shown());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       ChangePasswordManagerPin_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_FALSE(test_delegate().get_change_password_manager_pin_called());

  handler().ChangePasswordManagerPin(future.GetCallback());

  // The TestPasswordsPrivateDelegate implementation hardcodes false for success
  EXPECT_FALSE(future.Get());
  EXPECT_TRUE(test_delegate().get_change_password_manager_pin_called());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       IsPasswordManagerPinAvailable_CallsDelegate) {
  base::test::TestFuture<bool> future;
  handler().IsPasswordManagerPinAvailable(future.GetCallback());
  // The TestPasswordsPrivateDelegate hardcodes the response to false.
  EXPECT_FALSE(future.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       SwitchBiometricAuthBeforeFillingState_CallsDelegate) {
  base::test::TestFuture<bool> future;
  EXPECT_FALSE(test_delegate().get_authenticator_interaction_status());

  handler().SwitchBiometricAuthBeforeFillingState(future.GetCallback());

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(test_delegate().get_authenticator_interaction_status());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       IsAccountStorageEnabled_ReturnsCorrectValue) {
  // Set the delegate to return true.
  test_delegate().SetAccountStorageEnabled(true);

  base::test::TestFuture<bool> future_true;
  handler().IsAccountStorageEnabled(future_true.GetCallback());
  EXPECT_TRUE(future_true.Get());

  // Set the delegate to return false.
  test_delegate().SetAccountStorageEnabled(false);

  base::test::TestFuture<bool> future_false;
  handler().IsAccountStorageEnabled(future_false.GetCallback());
  EXPECT_FALSE(future_false.Get());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       SetAccountStorageEnabled_CallsDelegate) {
  // Ensure default state is false
  EXPECT_FALSE(test_delegate().IsAccountStorageEnabled());

  // Call the handler
  handler().SetAccountStorageEnabled(true);

  // Verify the state changed in the delegate
  EXPECT_TRUE(test_delegate().IsAccountStorageEnabled());
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       ShouldShowAccountStorageSettingToggle_CallsDelegate) {
  // Set the delegate to return true.
  test_delegate().SetShouldShowAccountStorageSettingToggle(true);

  base::test::TestFuture<bool> future_true;
  handler().ShouldShowAccountStorageSettingToggle(future_true.GetCallback());
  EXPECT_TRUE(future_true.Get());

  // Set the delegate to return false.
  test_delegate().SetShouldShowAccountStorageSettingToggle(false);

  base::test::TestFuture<bool> future_false;
  handler().ShouldShowAccountStorageSettingToggle(future_false.GetCallback());
  EXPECT_FALSE(future_false.Get());
}

}  // namespace password_manager
