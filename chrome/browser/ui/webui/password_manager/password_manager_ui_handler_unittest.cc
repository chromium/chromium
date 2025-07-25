// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui_handler.h"

#include "base/test/test_future.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

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

}  // namespace

class PasswordManagerUIHandlerUnitTest : public testing::Test {
 public:
  PasswordManagerUIHandlerUnitTest()
      : profile_(std::make_unique<TestingProfile>()),
        web_contents_(factory_.CreateWebContents(profile_.get())) {}
  ~PasswordManagerUIHandlerUnitTest() override = default;

  void SetUp() override {
    auto delegate =
        base::MakeRefCounted<extensions::TestPasswordsPrivateDelegate>();
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

  PasswordManagerUIHandler& handler() { return *handler_; }
  extensions::TestPasswordsPrivateDelegate& test_delegate() {
    return *test_delegate_;
  }

 protected:
  raw_ptr<extensions::TestPasswordsPrivateDelegate> test_delegate_;

  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  testing::NiceMock<MockPage> mock_page_;
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

}  // namespace password_manager
