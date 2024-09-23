// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/channel_util.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/version_info/channel.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::standalone_browser {

using ChannelUtilTest = testing::Test;

TEST_F(ChannelUtilTest, StatefulLacrosSelectionUpdateChannel) {
  // Assert that when no Lacros stability switch is specified, we return the
  // "unknown" channel.
  ASSERT_EQ(version_info::Channel::UNKNOWN,
            GetLacrosSelectionUpdateChannel(LacrosSelection::kStateful));

  // Assert that when a Lacros stability switch is specified, we return the
  // relevant channel name associated to that switch value.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchNative(kLacrosStabilitySwitch,
                              kLacrosStabilityChannelBeta);
  ASSERT_EQ(version_info::Channel::BETA,
            GetLacrosSelectionUpdateChannel(LacrosSelection::kStateful));
  cmdline->RemoveSwitch(kLacrosStabilitySwitch);
}

}  // namespace ash::standalone_browser
