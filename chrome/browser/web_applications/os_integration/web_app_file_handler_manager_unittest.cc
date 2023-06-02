// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/test/fake_web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

TEST(FileHandlerUtilsTest, GetFileExtensionsFromFileHandler) {
  apps::FileHandler file_handler;
  file_handler.action = GURL("https://app.site/open-foo");
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foo";
    accept_entry.file_extensions.insert(".foo");
    file_handler.accept.push_back(accept_entry);
  }
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foobar";
    accept_entry.file_extensions.insert(".foobar");
    file_handler.accept.push_back(accept_entry);
  }

  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandler(file_handler);

  EXPECT_EQ(2u, file_extensions.size());
  EXPECT_THAT(file_extensions,
              testing::UnorderedElementsAre(".foo", ".foobar"));
}

TEST(FileHandlerUtilsTest, GetFileExtensionsFromFileHandlers) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-bar");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  std::set<std::string> file_extensions =
      GetFileExtensionsFromFileHandlers(file_handlers);

  EXPECT_EQ(4u, file_extensions.size());
  EXPECT_THAT(file_extensions,
              testing::UnorderedElementsAre(".foo", ".foobar", ".bar", ".baz"));
}

TEST(FileHandlerUtilsTest, GetMimeTypesFromFileHandler) {
  apps::FileHandler file_handler;
  file_handler.action = GURL("https://app.site/open-foo");
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foo";
    accept_entry.file_extensions.insert(".foo");
    file_handler.accept.push_back(accept_entry);
  }
  {
    apps::FileHandler::AcceptEntry accept_entry;
    accept_entry.mime_type = "application/foobar";
    accept_entry.file_extensions.insert(".foobar");
    file_handler.accept.push_back(accept_entry);
  }
  std::set<std::string> mime_types = GetMimeTypesFromFileHandler(file_handler);

  EXPECT_EQ(2u, mime_types.size());
  EXPECT_THAT(mime_types, testing::UnorderedElementsAre("application/foo",
                                                        "application/foobar"));
}

TEST(FileHandlerUtilsTest, GetMimeTypesFromFileHandlers) {
  apps::FileHandlers file_handlers;

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-foo");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foo";
      accept_entry.file_extensions.insert(".foo");
      file_handler.accept.push_back(accept_entry);
    }
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/foobar";
      accept_entry.file_extensions.insert(".foobar");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  {
    apps::FileHandler file_handler;
    file_handler.action = GURL("https://app.site/open-bar");
    {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = "application/bar";
      accept_entry.file_extensions.insert(".bar");
      accept_entry.file_extensions.insert(".baz");
      file_handler.accept.push_back(accept_entry);
    }
    file_handlers.push_back(file_handler);
  }

  std::set<std::string> mime_types =
      GetMimeTypesFromFileHandlers(file_handlers);

  EXPECT_EQ(3u, mime_types.size());
  EXPECT_THAT(mime_types,
              testing::UnorderedElementsAre(
                  "application/foo", "application/foobar", "application/bar"));
}

class WebAppFileHandlerManagerTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    provider_ = FakeWebAppProvider::Get(profile());
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    // This is not a WebAppProvider subsystem, so this can be
    // set after the WebAppProvider has been initialized.
    file_handler_manager_ =
        std::make_unique<FakeWebAppFileHandlerManager>(profile());
    file_handler_manager_->SetSubsystems(&sync_bridge());

    auto web_app = test::CreateWebApp();
    app_id_ = web_app->app_id();
    {
      ScopedRegistryUpdate update(&sync_bridge());
      update->CreateApp(std::move(web_app));
    }
  }

  FakeWebAppFileHandlerManager& file_handler_manager() {
    return *file_handler_manager_;
  }

  WebAppProvider& provider() { return *provider_; }

  WebAppSyncBridge& sync_bridge() { return provider_->sync_bridge_unsafe(); }

  const AppId& app_id() const { return app_id_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_;
  std::unique_ptr<FakeWebAppFileHandlerManager> file_handler_manager_;

  AppId app_id_;
};

TEST_F(WebAppFileHandlerManagerTest, FileHandlersAreNotAvailableUnlessEnabled) {
  // TODO(crbug/1288442): re-enable this test
  if (!ShouldRegisterFileHandlersWithOs()) {
    GTEST_SKIP();
  }

  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}}, absl::nullopt,
      /*enable=*/false);

  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-bar"),
      {{"application/bar", {".bar"}}}, absl::nullopt,
      /*enable=*/false);

  // File handlers are disabled by default.
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id());
    EXPECT_EQ(nullptr, handlers);
  }

  // Ensure they can be enabled.
  base::RunLoop run_loop;
  Result file_handling_enabled;
  file_handler_manager().EnableAndRegisterOsFileHandlers(
      app_id(), base::BindLambdaForTesting([&](Result result) {
        file_handling_enabled = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  {
    EXPECT_EQ(file_handling_enabled, Result::kOk);
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id());
    EXPECT_EQ(2u, handlers->size());
  }

  // Ensure they can be disabled.
  base::RunLoop run_loop_disabled;
  Result file_handling_disabled;
  file_handler_manager().DisableAndUnregisterOsFileHandlers(
      app_id(), base::BindLambdaForTesting([&](Result result) {
        file_handling_disabled = result;
        run_loop_disabled.Quit();
      }));
  run_loop_disabled.Run();
  {
    EXPECT_EQ(file_handling_disabled, Result::kOk);
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id());
    EXPECT_EQ(nullptr, handlers);
  }
}

TEST_F(WebAppFileHandlerManagerTest, NoHandlersRegistered) {
  // Returns an empty list when no file handlers are registered.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {path});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(WebAppFileHandlerManagerTest, NoLaunchFilesPassed) {
  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}}, absl::nullopt);

  // Returns an empty list when no launch files are passed.
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(WebAppFileHandlerManagerTest,
       SingleValidExtensionSingleExtensionHandler) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), url, {{"application/foo", {".foo"}}}, absl::nullopt);

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {path});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(url, std::get<GURL>(launch_infos[0]));
}

TEST_F(WebAppFileHandlerManagerTest, ExtensionCaseInsensitive) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), url, {{"application/foo", {".foo"}}}, absl::nullopt);

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.FOO"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {path});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(url, std::get<GURL>(launch_infos[0]));
}

TEST_F(WebAppFileHandlerManagerTest,
       SingleInvalidExtensionSingleExtensionHandler) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), url, {{"application/foo", {".foo"}}}, absl::nullopt);

  // Returns nullopt on single invalid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.bar"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {path});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(WebAppFileHandlerManagerTest,
       SingleValidExtensionMultiExtensionHandler) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}, {"application/bar", {".bar"}}},
      absl::nullopt);

  // Matches on single valid extension for multi-extension handler.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {path});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(url, std::get<GURL>(launch_infos[0]));
}

TEST_F(WebAppFileHandlerManagerTest, MultipleValidExtensions) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}, {"application/bar", {".bar"}}},
      absl::nullopt);

  // Matches on multiple valid extensions for multi-extension handler.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(),
                                                        {path1, path2});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(url, std::get<GURL>(launch_infos[0]));
  const auto& paths = std::get<std::vector<base::FilePath>>(launch_infos[0]);
  EXPECT_EQ(2u, paths.size());
  EXPECT_TRUE(base::Contains(paths, path1));
  EXPECT_TRUE(base::Contains(paths, path2));
}

TEST_F(WebAppFileHandlerManagerTest, PartialExtensionMatch) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), url, {{"application/foo", {".foo"}}}, absl::nullopt);

  // Works with partial extension match.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(),
                                                        {path1, path2});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(url, std::get<GURL>(launch_infos[0]));
  const auto& paths = std::get<std::vector<base::FilePath>>(launch_infos[0]);
  EXPECT_EQ(1u, paths.size());
  EXPECT_TRUE(base::Contains(paths, path1));
  EXPECT_FALSE(base::Contains(paths, path2));
}

TEST_F(WebAppFileHandlerManagerTest, SingleFileWithoutExtension) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), url, {{"application/foo", {".foo"}}}, absl::nullopt);

  // Returns nullopt where a file has no extension.
  const base::FilePath path(FILE_PATH_LITERAL("file"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(), {path});
  EXPECT_TRUE(launch_infos.empty());
}

TEST_F(WebAppFileHandlerManagerTest, FileWithoutExtensionAmongMultipleFiles) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), url, {{"application/foo", {".foo"}}}, absl::nullopt);

  // Returns nullopt where one file has no extension while others do.
  const base::FilePath path1(FILE_PATH_LITERAL("file"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.foo"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(app_id(),
                                                        {path1, path2});
  ASSERT_EQ(1u, launch_infos.size());
  EXPECT_EQ(url, std::get<GURL>(launch_infos[0]));
  const auto& paths = std::get<std::vector<base::FilePath>>(launch_infos[0]);
  EXPECT_EQ(1u, paths.size());
  EXPECT_FALSE(base::Contains(paths, path1));
  EXPECT_TRUE(base::Contains(paths, path2));
}

TEST_F(WebAppFileHandlerManagerTest, MultiLaunch) {
  const GURL foo_url("https://app.site/handle-foo");
  const GURL bar_url("https://app.site/handle-bar");
  const GURL baz_url("https://app.site/handle-baz");

  file_handler_manager().InstallFileHandler(
      app_id(), foo_url, {{"application/foo", {".foo"}}}, absl::nullopt);
  file_handler_manager().InstallFileHandler(
      app_id(), bar_url, {{"application/bar", {".bar"}}}, absl::nullopt);
  file_handler_manager().InstallFileHandler(
      app_id(), baz_url, {{"application/baz", {".baz"}}},
      apps::FileHandler::LaunchType::kMultipleClients);

  // Finds multiple different handlers for multiple different file types, but
  // coalesces matching file types.
  const base::FilePath foo_path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath foo_path2(FILE_PATH_LITERAL("file2.foo"));
  const base::FilePath bar_path(FILE_PATH_LITERAL("file.bar"));
  const base::FilePath baz_path1(FILE_PATH_LITERAL("file.baz"));
  const base::FilePath baz_path2(FILE_PATH_LITERAL("file2.baz"));
  WebAppFileHandlerManager::LaunchInfos launch_infos =
      file_handler_manager().GetMatchingFileHandlerUrls(
          app_id(), {foo_path1, foo_path2, bar_path, baz_path1, baz_path2});
  ASSERT_EQ(4u, launch_infos.size());
  // The expected number of launches for each action URL.
  std::map<GURL, int> expected_counts;
  expected_counts[foo_url] = 1;
  expected_counts[bar_url] = 1;
  expected_counts[baz_url] = 2;
  for (int i = 0; i < 4; ++i) {
    GURL launch_url = std::get<GURL>(launch_infos[i]);
    const auto& launch_paths =
        std::get<std::vector<base::FilePath>>(launch_infos[i]);
    expected_counts[launch_url]--;
    if (launch_url == foo_url) {
      EXPECT_EQ(2u, launch_paths.size());
      EXPECT_TRUE(base::Contains(launch_paths, foo_path1));
      EXPECT_TRUE(base::Contains(launch_paths, foo_path2));
    } else if (launch_url == bar_url) {
      EXPECT_EQ(1u, launch_paths.size());
      EXPECT_TRUE(base::Contains(launch_paths, bar_path));
    } else if (launch_url == baz_url) {
      EXPECT_EQ(1u, launch_paths.size());
      bool has_path1 = base::Contains(launch_paths, baz_path1);
      bool has_path2 = base::Contains(launch_paths, baz_path2);
      EXPECT_NE(has_path1, has_path2);
    } else {
      FAIL() << " Got unexpected URL " << launch_url;
    }
  }

  for (const auto& launch_count : expected_counts) {
    EXPECT_EQ(0, launch_count.second)
        << " Didn't see enough launches for " << launch_count.first.spec();
  }
}

}  // namespace web_app
