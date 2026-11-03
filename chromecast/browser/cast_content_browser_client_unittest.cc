// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/scoped_command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_feature_list_creator.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromecast::shell {
namespace {

class CastContentBrowserClientTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
        switches::kEnableLocalFileAccesses);
  }

  base::test::ScopedCommandLine scoped_command_line_;
  content::BrowserTaskEnvironment task_environment_;
  CastFeatureListCreator feature_list_creator_;
  std::unique_ptr<CastContentBrowserClient> browser_client_ =
      CastContentBrowserClient::Create(&feature_list_creator_);
};

TEST_F(CastContentBrowserClientTest, HandlesInternalSchemes) {
  EXPECT_TRUE(browser_client_->IsHandledURL(GURL("chrome://home")));
  EXPECT_TRUE(browser_client_->IsHandledURL(GURL("devtools://devtools/")));
  EXPECT_TRUE(browser_client_->IsHandledURL(GURL("chrome-resource://app/")));
  EXPECT_TRUE(
      browser_client_->IsHandledURL(GURL("blob:https://example.com/id")));
  EXPECT_TRUE(browser_client_->IsHandledURL(GURL("data:text/plain,hello")));
  EXPECT_TRUE(browser_client_->IsHandledURL(
      GURL("filesystem:https://example.com/temporary/test.txt")));
  EXPECT_TRUE(browser_client_->IsHandledURL(GURL("file:///path/to/page.html")));
}

TEST_F(CastContentBrowserClientTest, RejectsInvalidAndUnhandledUrls) {
  EXPECT_FALSE(browser_client_->IsHandledURL(GURL()));
  EXPECT_FALSE(browser_client_->IsHandledURL(GURL("not a valid url")));
  EXPECT_FALSE(browser_client_->IsHandledURL(GURL("mailto:user@example.com")));
  EXPECT_FALSE(browser_client_->IsHandledURL(GURL("custom-scheme://example/")));
}

TEST_F(CastContentBrowserClientTest,
       HandlesFileUrlRegardlessOfLocalFileAccessSwitch) {
  const GURL file_url("file:///path/to/page.html");

  EXPECT_TRUE(browser_client_->IsHandledURL(file_url));

  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableLocalFileAccesses);
  EXPECT_TRUE(browser_client_->IsHandledURL(file_url));
}

TEST_F(CastContentBrowserClientTest, DisallowsFileAccessByDefault) {
  const base::FilePath file_path(FILE_PATH_LITERAL("/path/to/page.html"));
  const base::FilePath profile_path(FILE_PATH_LITERAL("/path/to/profile"));

  EXPECT_FALSE(
      browser_client_->IsFileAccessAllowed(file_path, file_path, profile_path));
}

TEST_F(CastContentBrowserClientTest, AllowsFileAccessWhenSwitchPresent) {
  const base::FilePath file_path(FILE_PATH_LITERAL("/path/to/page.html"));
  const base::FilePath profile_path(FILE_PATH_LITERAL("/path/to/profile"));

  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableLocalFileAccesses);

  EXPECT_TRUE(
      browser_client_->IsFileAccessAllowed(file_path, file_path, profile_path));
}

}  // namespace
}  // namespace chromecast::shell
