// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/sign_in_celebration_handler.h"

#include <memory>

#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/webui/intro/sign_in_celebration.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

using ::testing::_;
using ::testing::Field;
using ::testing::Pointee;

class MockPage : public intro::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<intro::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              OnSignInCelebrationUserInfoUpdated,
              (intro::mojom::SignInCelebrationUserInfoPtr),
              (override));

 private:
  mojo::Receiver<intro::mojom::Page> receiver_{this};
};

class SignInCelebrationHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    handler_ = std::make_unique<SignInCelebrationHandler>(
        identity_env().identity_manager(), mock_page_.BindAndGetRemote(),
        handler_remote_.BindNewPipeAndPassReceiver());
  }

  signin::IdentityTestEnvironment& identity_env() {
    return CHECK_DEREF(identity_test_env_);
  }
  MockPage& mock_page() { return mock_page_; }
  SignInCelebrationHandler& handler() { return CHECK_DEREF(handler_); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  testing::StrictMock<MockPage> mock_page_;
  mojo::Remote<intro::mojom::PageHandler> handler_remote_;
  std::unique_ptr<SignInCelebrationHandler> handler_;
};

TEST_F(SignInCelebrationHandlerTest, ReturnsUserInfo) {
  const std::string kEmail = "test@gmail.com";
  const std::string kName = "Test";
  AccountInfo account_info = identity_env().MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);
  account_info = AccountInfo::Builder(account_info).SetGivenName(kName).Build();

  // UpdateAccountInfoForAccount will trigger a notification to the page.
  EXPECT_CALL(mock_page(), OnSignInCelebrationUserInfoUpdated(_));
  identity_env().UpdateAccountInfoForAccount(account_info);

  mock_page().FlushForTesting();

  base::test::TestFuture<intro::mojom::SignInCelebrationUserInfoPtr> future;
  handler().GetSignInCelebrationUserInfo(future.GetCallback());

  const auto& user_info = future.Get();
  EXPECT_FALSE(user_info->avatar_url.is_empty());
  EXPECT_EQ(user_info->title,
            l10n_util::GetStringFUTF8(IDS_FRE_SIGN_IN_CELEBRATION_WELCOME_TITLE,
                                      base::UTF8ToUTF16(kName)));
}

TEST_F(SignInCelebrationHandlerTest, ReturnsUserInfoWithFallbackToEmail) {
  const std::string kEmail = "test@gmail.com";
  identity_env().MakePrimaryAccountAvailable(kEmail,
                                             signin::ConsentLevel::kSignin);

  base::test::TestFuture<intro::mojom::SignInCelebrationUserInfoPtr> future;
  handler().GetSignInCelebrationUserInfo(future.GetCallback());
  const auto& user_info = future.Get();
  // Should fallback to email if name is missing.
  EXPECT_EQ(user_info->title,
            l10n_util::GetStringFUTF8(IDS_FRE_SIGN_IN_CELEBRATION_WELCOME_TITLE,
                                      base::UTF8ToUTF16(kEmail)));
}

TEST_F(SignInCelebrationHandlerTest, DoesNotNotifyIfAccountInfoIncomplete) {
  const std::string kEmail = "test@gmail.com";
  const AccountInfo account_info = identity_env().MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  // Initial update when info is not yet fully available (no name/avatar) -
  // should not trigger update.
  EXPECT_CALL(mock_page(), OnSignInCelebrationUserInfoUpdated(_)).Times(0);
  identity_env().UpdateAccountInfoForAccount(account_info);

  mock_page().FlushForTesting();
}

TEST_F(SignInCelebrationHandlerTest,
       NotifiesWhenAccountInfoUpdatedWithGivenName) {
  const std::string kEmail = "test@gmail.com";
  const std::string kName = "Test";
  AccountInfo account_info = identity_env().MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  const std::string expected_title = l10n_util::GetStringFUTF8(
      IDS_FRE_SIGN_IN_CELEBRATION_WELCOME_TITLE, base::UTF8ToUTF16(kName));

  base::RunLoop run_loop;
  EXPECT_CALL(
      mock_page(),
      OnSignInCelebrationUserInfoUpdated(Pointee(Field(
          &intro::mojom::SignInCelebrationUserInfo::title, expected_title))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  account_info = AccountInfo::Builder(account_info).SetGivenName(kName).Build();
  identity_env().UpdateAccountInfoForAccount(account_info);
  run_loop.Run();
}

TEST_F(SignInCelebrationHandlerTest, DoesNotNotifyIfNonPrimaryAccountUpdated) {
  identity_env().MakePrimaryAccountAvailable("test@gmail.com",
                                             signin::ConsentLevel::kSignin);

  // Update for non-primary account - should not trigger update.
  AccountInfo other_account =
      identity_env().MakeAccountAvailable("other@gmail.com");
  other_account =
      AccountInfo::Builder(other_account).SetGivenName("Other").Build();
  EXPECT_CALL(mock_page(), OnSignInCelebrationUserInfoUpdated(_)).Times(0);
  identity_env().UpdateAccountInfoForAccount(other_account);

  mock_page().FlushForTesting();
}

TEST_F(SignInCelebrationHandlerTest, ReturnsAvatarUrl) {
  const std::string kEmail = "test@gmail.com";
  AccountInfo account_info = identity_env().MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  // No avatar set, should return placeholder avatar url.
  {
    EXPECT_CALL(mock_page(), OnSignInCelebrationUserInfoUpdated(_)).Times(0);
    base::test::TestFuture<intro::mojom::SignInCelebrationUserInfoPtr> future;
    handler().GetSignInCelebrationUserInfo(future.GetCallback());
    EXPECT_EQ(future.Get()->avatar_url,
              GURL(profiles::GetPlaceholderAvatarIconUrl()));
    mock_page().FlushForTesting();
  }

  // Avatar set, should return the correct avatar url.
  {
    EXPECT_CALL(mock_page(), OnSignInCelebrationUserInfoUpdated(_));
    signin::SimulateAccountImageFetch(
        identity_env().identity_manager(), account_info.account_id,
        "https://example.com/image.png", gfx::test::CreateImage(100, 100));
    mock_page().FlushForTesting();

    base::test::TestFuture<intro::mojom::SignInCelebrationUserInfoPtr> future;
    handler().GetSignInCelebrationUserInfo(future.GetCallback());
    const auto& user_info = future.Get();
    EXPECT_NE(user_info->avatar_url,
              GURL(profiles::GetPlaceholderAvatarIconUrl()));
    EXPECT_TRUE(user_info->avatar_url.is_valid());
    EXPECT_TRUE(user_info->avatar_url.SchemeIs(url::kDataScheme));
  }
}

}  // namespace
