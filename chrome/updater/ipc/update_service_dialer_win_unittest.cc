// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer_win.h"

#include <windows.h>

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

using PipeNameType = ::mojo::NamedPlatformChannel::PipeNameType;
using ::testing::ElementsAre;

class UpdateServiceDialerWinTest : public ::testing::Test {
 protected:
  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> Select() {
    return SelectUpdateServiceEndpoint(
        [&](PipeNameType pipe_name_type) { return Connect(pipe_name_type); },
        [&](PipeNameType pipe_name_type) {
          return !present_.contains(pipe_name_type);
        });
  }

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> Connect(
      PipeNameType pipe_name_type) {
    connect_attempts_.push_back(pipe_name_type);
    if (rejected_.contains(pipe_name_type)) {
      return base::unexpected(ConnectToServerError::kRejected);
    }
    if (!connectable_.contains(pipe_name_type)) {
      return base::unexpected(ConnectToServerError::kRetryable);
    }
    mojo::PlatformChannel channel;
    return channel.TakeLocalEndpoint();
  }

  base::flat_set<PipeNameType> connectable_;
  base::flat_set<PipeNameType> rejected_;
  base::flat_set<PipeNameType> present_;
  std::vector<PipeNameType> connect_attempts_;
};

TEST_F(UpdateServiceDialerWinTest, PrefersProtectedPipeWhenServerServesIt) {
  connectable_ = {PipeNameType::kAdminProtected};
  present_ = {PipeNameType::kAdminProtected, PipeNameType::kDefault};

  EXPECT_TRUE(Select().has_value());
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, FallsBackWhenServerHasNoProtectedPipe) {
  connectable_ = {PipeNameType::kDefault};
  present_ = {PipeNameType::kDefault};

  EXPECT_TRUE(Select().has_value());
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected,
                                             PipeNameType::kDefault));
}

TEST_F(UpdateServiceDialerWinTest, DoesNotFallBackWhenProtectedPipeRefuses) {
  connectable_ = {PipeNameType::kDefault};
  present_ = {PipeNameType::kAdminProtected, PipeNameType::kDefault};

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      Select();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRetryable);
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, DoesNotFallBackWhenProtectedPipeRejects) {
  rejected_ = {PipeNameType::kAdminProtected};
  connectable_ = {PipeNameType::kDefault};
  present_ = {PipeNameType::kDefault};

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      Select();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRejected);
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, PropagatesRejectionFromFallbackPipe) {
  rejected_ = {PipeNameType::kDefault};
  present_ = {PipeNameType::kDefault};

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      Select();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRejected);
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected,
                                             PipeNameType::kDefault));
}

TEST_F(UpdateServiceDialerWinTest, ConnectsWhenOnlyProtectedPipeIsPresent) {
  connectable_ = {PipeNameType::kAdminProtected};
  present_ = {PipeNameType::kAdminProtected};

  EXPECT_TRUE(Select().has_value());
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, DoesNotFallBackWhileServerIsStarting) {
  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      Select();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRetryable);
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, ConnectToUpdateServiceMissingPipeRetries) {
  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      ConnectToUpdateService(
          UpdaterScope::kUser,
          mojo::NamedPlatformChannel::GenerateRandomServerName(),
          PipeNameType::kDefault);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRetryable);
}

TEST_F(UpdateServiceDialerWinTest, ConnectToUpdateServiceAccessDeniedRejects) {
  mojo::NamedPlatformChannel::Options options;
  options.server_name = mojo::NamedPlatformChannel::GenerateRandomServerName();
  // Deny Generic All to Everyone (WD).
  options.security_descriptor = L"D:(D;;GA;;;WD)";
  mojo::NamedPlatformChannel server(options);

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      ConnectToUpdateService(UpdaterScope::kUser, options.server_name,
                             PipeNameType::kDefault);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRejected);
}

// Whether the current process's token owner would pass `CheckServerElevated`.
// Used only to pick which of the two system-scope tests below is meaningful on
// this bot; the expectation in each test is a hard-coded constant.
bool IsCurrentProcessElevatedOwner() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    return false;
  }
  const base::win::Sid owner = token->Owner();
  return owner == base::win::Sid(base::win::WellKnownSid::kLocalSystem) ||
         owner ==
             base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators);
}

TEST_F(UpdateServiceDialerWinTest,
       ConnectToUpdateServiceSystemScopeAcceptsElevatedServer) {
  if (!IsCurrentProcessElevatedOwner()) {
    GTEST_SKIP() << "Requires an elevated test process to host the server.";
  }
  mojo::NamedPlatformChannel::Options options;
  options.server_name = mojo::NamedPlatformChannel::GenerateRandomServerName();
  mojo::NamedPlatformChannel server(options);

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      ConnectToUpdateService(UpdaterScope::kSystem, options.server_name,
                             PipeNameType::kDefault);
  EXPECT_TRUE(result.has_value());
}

TEST_F(UpdateServiceDialerWinTest,
       ConnectToUpdateServiceSystemScopeRejectsUnelevatedServer) {
  if (IsCurrentProcessElevatedOwner()) {
    GTEST_SKIP() << "Requires an unelevated test process to host the server.";
  }
  mojo::NamedPlatformChannel::Options options;
  options.server_name = mojo::NamedPlatformChannel::GenerateRandomServerName();
  mojo::NamedPlatformChannel server(options);

  base::expected<mojo::PlatformChannelEndpoint, ConnectToServerError> result =
      ConnectToUpdateService(UpdaterScope::kSystem, options.server_name,
                             PipeNameType::kDefault);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ConnectToServerError::kRejected);
}

TEST_F(UpdateServiceDialerWinTest, ClassifyDialErrorAccessDeniedRejects) {
  EXPECT_EQ(ClassifyDialError(E_ACCESSDENIED), ConnectToServerError::kRejected);
}

TEST_F(UpdateServiceDialerWinTest, ClassifyDialErrorOtherFailuresRetry) {
  EXPECT_EQ(ClassifyDialError(REGDB_E_CLASSNOTREG),
            ConnectToServerError::kRetryable);
  EXPECT_EQ(ClassifyDialError(CO_E_SERVER_EXEC_FAILURE),
            ConnectToServerError::kRetryable);
  EXPECT_EQ(ClassifyDialError(E_FAIL), ConnectToServerError::kRetryable);
}

}  // namespace
}  // namespace updater
