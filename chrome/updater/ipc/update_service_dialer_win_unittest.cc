// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer_win.h"

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
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
  std::optional<mojo::PlatformChannelEndpoint> Select() {
    return SelectUpdateServiceEndpoint(
        [&](PipeNameType pipe_name_type) { return Connect(pipe_name_type); },
        [&](PipeNameType pipe_name_type) {
          return !present_.contains(pipe_name_type);
        });
  }

  std::optional<mojo::PlatformChannelEndpoint> Connect(
      PipeNameType pipe_name_type) {
    connect_attempts_.push_back(pipe_name_type);
    if (!connectable_.contains(pipe_name_type)) {
      return std::nullopt;
    }
    mojo::PlatformChannel channel;
    return channel.TakeLocalEndpoint();
  }

  base::flat_set<PipeNameType> connectable_;
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

  EXPECT_FALSE(Select().has_value());
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, ConnectsWhenOnlyProtectedPipeIsPresent) {
  connectable_ = {PipeNameType::kAdminProtected};
  present_ = {PipeNameType::kAdminProtected};

  EXPECT_TRUE(Select().has_value());
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

TEST_F(UpdateServiceDialerWinTest, DoesNotFallBackWhileServerIsStarting) {
  EXPECT_FALSE(Select().has_value());
  EXPECT_THAT(connect_attempts_, ElementsAre(PipeNameType::kAdminProtected));
}

}  // namespace
}  // namespace updater
