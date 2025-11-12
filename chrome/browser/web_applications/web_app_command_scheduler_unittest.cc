// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

class WebAppCommandSchedulerTest : public WebAppTest {
 public:
  bool IsCommandQueued(std::string_view command_name) {
    // Note: Accessing & using the debug value for tests is poor practice and
    // should not be done, given how easily the format can be changed.
    // TODO(b/318858671): Update logic to not read command errors from debug
    // log.
    base::Value::Dict log =
        fake_provider().command_manager().ToDebugValue().TakeDict();
    for (const base::Value& command : *log.FindList("command_queue")) {
      if (*command.GetDict().FindDict("!metadata")->FindString("!name") ==
          command_name) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(WebAppCommandSchedulerTest, FetchManifestAndInstall) {
  EXPECT_FALSE(fake_provider().is_registry_ready());
  fake_provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(), base::DoNothing(), base::DoNothing(),
      FallbackBehavior::kCraftedManifestOnly);

  fake_provider().StartWithSubsystems();
  EXPECT_EQ(
      fake_provider().command_manager().GetStartedCommandCountForTesting(), 0);
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 1u);

  EXPECT_TRUE(IsCommandQueued("FetchManifestAndInstallCommand"));
}

TEST_F(WebAppCommandSchedulerTest, PersistFileHandlersUserChoice) {
  EXPECT_FALSE(fake_provider().is_registry_ready());
  fake_provider().scheduler().PersistFileHandlersUserChoice(
      "app id", /*allowed=*/true, base::DoNothing());

  fake_provider().StartWithSubsystems();
  EXPECT_EQ(
      fake_provider().command_manager().GetStartedCommandCountForTesting(), 0);
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 1u);

  EXPECT_TRUE(IsCommandQueued("UpdateFileHandlerCommand"));

  test::WaitUntilReady(&fake_provider());
  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(IsCommandQueued("UpdateFileHandlerCommand"));

  fake_provider().Shutdown();
  base::test::TestFuture<void> after_shutdown;
  fake_provider().scheduler().PersistFileHandlersUserChoice(
      "app id", /*allowed=*/true, after_shutdown.GetCallback());
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 0u);
  ASSERT_TRUE(after_shutdown.Wait());
}

}  // namespace
}  // namespace web_app
