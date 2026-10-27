// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_mojom_traits.h"

#include <string>

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/password_store/actionable_error.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

TEST(PasswordManagerMojomTraitsTest, ExportProgressStatusRoundTrip) {
  const ExportProgressStatus kCases[] = {
      ExportProgressStatus::kNotStarted,
      ExportProgressStatus::kInProgress,
      ExportProgressStatus::kSucceeded,
      ExportProgressStatus::kFailedCancelled,
      ExportProgressStatus::kFailedWrite,
  };

  for (ExportProgressStatus status : kCases) {
    ExportProgressStatus output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                password_manager::mojom::ExportProgressStatus>(status, output));
    EXPECT_EQ(status, output);
  }
}

TEST(PasswordManagerMojomTraitsTest, PasswordAutomaticChangeStateRoundTrip) {
  using State = PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState;
  const State kCases[] = {
      State::kInactive,
      State::kAttemptingSignIn,
      State::kChangingPassword,
      State::kConfirmingChangedPassword,
      State::kPasswordChangedSuccessfully,
      State::kError,
  };

  for (State state : kCases) {
    State output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                password_manager::mojom::PasswordAutomaticChangeState>(state,
                                                                       output));
    EXPECT_EQ(state, output);
  }
}

TEST(PasswordManagerMojomTraitsTest, ActionableErrorRoundTrip) {
  const ActionableError kCases[] = {
      ActionableError::kNoError,
      ActionableError::kInactionable,
      ActionableError::kInactionableTemporaryError,
      ActionableError::kSignInNeeded,
      ActionableError::kKeychainError,
      ActionableError::kTrustedVaultKeyNeeded,
      ActionableError::kNeedsPassphrase,
  };

  for (ActionableError error : kCases) {
    ActionableError output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                password_manager::mojom::PasswordManagerActionableError>(
        error, output));
    EXPECT_EQ(error, output);
  }
}

TEST(PasswordManagerMojomTraitsTest, DomainInfoRoundTrip) {
  CredentialUIEntry::DomainInfo input{
      .name = "Example Domain",
      .url = GURL("https://example.com/login"),
      .signon_realm = "https://example.com/",
  };

  CredentialUIEntry::DomainInfo output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<password_manager::mojom::DomainInfo>(
          input, output));
  EXPECT_EQ(input.name, output.name);
  EXPECT_EQ(input.url, output.url);
  EXPECT_EQ(input.signon_realm, output.signon_realm);
}

TEST(PasswordManagerMojomTraitsTest, ActorLoginPermissionRoundTrip) {
  ActorLoginPermission input;
  input.domain_info = {
      .name = "Google",
      .url = GURL("https://google.com"),
      .signon_realm = "https://google.com/",
  };
  input.username = u"test_user_\u4f60\u597d";
  input.favicon_url = GURL("https://google.com/favicon.ico");

  ActorLoginPermission output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              password_manager::mojom::ActorLoginPermission>(input, output));
  EXPECT_EQ(input.domain_info.name, output.domain_info.name);
  EXPECT_EQ(input.domain_info.url, output.domain_info.url);
  EXPECT_EQ(input.domain_info.signon_realm, output.domain_info.signon_realm);
  EXPECT_EQ(input.username, output.username);
  EXPECT_EQ(input.favicon_url, output.favicon_url);
}

TEST(PasswordManagerMojomTraitsTest, ActorLoginPermissionEmptyFavicon) {
  ActorLoginPermission input;
  input.domain_info = {
      .name = "Android App",
      .url = GURL("android://hash@com.example.android/"),
      .signon_realm = "android://hash@com.example.android/",
  };
  input.username = u"android_user";
  input.favicon_url = GURL();

  ActorLoginPermission output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              password_manager::mojom::ActorLoginPermission>(input, output));
  EXPECT_EQ(input.username, output.username);
  EXPECT_TRUE(output.favicon_url.is_empty());
}

}  // namespace
}  // namespace password_manager
