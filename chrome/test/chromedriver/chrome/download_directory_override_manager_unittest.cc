// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/download_directory_override_manager.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
void AssertDownloadDirectoryCommand(const Command& command,
                                    const std::string& download_directory) {
  std::string behavior;
  std::string download_path;
  ASSERT_EQ("Page.setDownloadBehavior", command.method);
  ASSERT_TRUE(command.params.GetString("behavior", &behavior));
  ASSERT_TRUE(command.params.GetString("downloadPath", &download_path));
  ASSERT_EQ(download_directory, download_path);
  ASSERT_EQ(behavior, "allow");
}
}  // namespace

TEST(DownloadDirectoryOverrideManager,
     OnConnectedSendsCommandIfDownloadDirectoryPopulated) {
  RecorderDevToolsClient client;
  DownloadDirectoryOverrideManager manager(&client);
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(0u, client.commands_.size());
}

TEST(DownloadDirectoryOverrideManager, OverrideSendsCommand) {
  RecorderDevToolsClient client;
  DownloadDirectoryOverrideManager manager(&client);

  // No command should be sent until OnConnect is called
  const std::string directory = "download/directory";
  ASSERT_EQ(kOk,
            manager.OverrideDownloadDirectoryWhenConnected(directory).code());
  ASSERT_EQ(0u, client.commands_.size());

  // On connected is called and the directory should now
  // be overridden to 'download/directory'
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(1u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDownloadDirectoryCommand(client.commands_[0], directory));

  const std::string directory2 = "download/directory2";
  ASSERT_EQ(kOk,
            manager.OverrideDownloadDirectoryWhenConnected(directory2).code());
  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDownloadDirectoryCommand(client.commands_[1], directory2));
}
