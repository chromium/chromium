// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  mojo::Receiver<mojom::Page> receiver_{this};
};

class QuitRunLoopObserver : public SavedPasswordsPresenter::Observer {
 public:
  explicit QuitRunLoopObserver(base::RunLoop* run_loop) : run_loop_(run_loop) {}
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override {
    run_loop_->Quit();
  }

 private:
  raw_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class PasswordManagerUIHandlerUnitTest : public testing::Test {
 public:
  PasswordManagerUIHandlerUnitTest()
      : profile_(std::make_unique<TestingProfile>()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {}
  ~PasswordManagerUIHandlerUnitTest() override = default;

  void SetUp() override {
    password_store_ = CreateAndUseTestPasswordStore(profile_.get());
    affiliation_service_ =
        std::make_unique<affiliations::FakeAffiliationService>();
    auto delegate =
        base::MakeRefCounted<extensions::TestPasswordsPrivateDelegate>();
    auto presenter = std::make_unique<SavedPasswordsPresenter>(
        affiliation_service_.get(), password_store_,
        /*account_store=*/nullptr);
    base::RunLoop run_loop;
    presenter->Init(run_loop.QuitClosure());
    run_loop.Run();
    presenter_ = presenter.get();
    delegate->SetSavedPasswordsPresenter(std::move(presenter));
    test_delegate_ = delegate.get();

    handler_ = std::make_unique<PasswordManagerUIHandler>(
        mojo::PendingReceiver<mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), std::move(delegate), web_contents_);
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  void TearDown() override {
    test_delegate_ = nullptr;
    testing::Test::TearDown();
  }

  void CreateAndSeedPasswordForm(const GURL& url,
                                 const std::u16string& username,
                                 bool actor_login_approved) {
    PasswordForm form;
    form.url = url;
    form.username_value = username;
    form.actor_login_approved = actor_login_approved;
    form.in_store = PasswordForm::Store::kProfileStore;
    base::RunLoop run_loop;
    QuitRunLoopObserver observer(&run_loop);
    presenter().AddObserver(&observer);
    password_store().AddLogin(form);
    run_loop.Run();
    presenter().RemoveObserver(&observer);
  }

  PasswordManagerUIHandler& handler() { return *handler_; }
  extensions::TestPasswordsPrivateDelegate& test_delegate() {
    return *test_delegate_;
  }
  SavedPasswordsPresenter& presenter() { return *presenter_; }
  TestPasswordStore& password_store() { return *password_store_; }

 protected:
  raw_ptr<extensions::TestPasswordsPrivateDelegate> test_delegate_;

  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<PasswordManagerUIHandler> handler_;
  scoped_refptr<TestPasswordStore> password_store_;
  std::unique_ptr<affiliations::FakeAffiliationService> affiliation_service_;
  raw_ptr<SavedPasswordsPresenter> presenter_;
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
  base::test::TestFuture<std::vector<mojom::ActorLoginPermissionPtr>> future;
  CreateAndSeedPasswordForm(GURL("https://test.com"), u"testuser",
                            /*actor_login_approved=*/true);

  handler().GetActorLoginPermissions(future.GetCallback());

  EXPECT_EQ(future.Get().size(), 1u);
}

TEST_F(PasswordManagerUIHandlerUnitTest,
       RevokeActorLoginPermission_CallsPresenter) {
  auto site = mojom::ActorLoginPermission::New();
  site->url = mojom::FormattedUrl::New("test.com", "https://test.com");
  site->username = "testuser";
  CreateAndSeedPasswordForm(GURL(site->url->link), u"testuser",
                            /*actor_login_approved=*/true);

  base::RunLoop run_loop;
  QuitRunLoopObserver observer(&run_loop);
  presenter().AddObserver(&observer);
  handler().RevokeActorLoginPermission(std::move(site));
  run_loop.Run();
  presenter().RemoveObserver(&observer);

  ASSERT_EQ(password_store().stored_passwords().size(), 1u);
  const auto& passwords = password_store().stored_passwords().begin()->second;
  ASSERT_EQ(passwords.size(), 1u);
  EXPECT_FALSE(passwords[0].actor_login_approved);
}

}  // namespace password_manager
