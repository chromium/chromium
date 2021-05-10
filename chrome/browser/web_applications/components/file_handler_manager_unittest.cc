// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

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

class FileHandlerManagerTest : public WebAppTest {
 public:
  FileHandlerManagerTest() {
    // |features_| needs to be initialized before SetUp kicks off tasks that
    // check if a feature is enabled.
    features_.InitAndEnableFeature(blink::features::kFileHandlingAPI);
  }

 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    registrar_ = std::make_unique<TestAppRegistrar>();
    file_handler_manager_ = std::make_unique<TestFileHandlerManager>(profile());

    file_handler_manager_->SetSubsystems(registrar_.get());
  }

  TestFileHandlerManager& file_handler_manager() {
    return *file_handler_manager_.get();
  }

 private:
  std::unique_ptr<TestAppRegistrar> registrar_;
  std::unique_ptr<TestFileHandlerManager> file_handler_manager_;

  base::test::ScopedFeatureList features_;
};

TEST_F(FileHandlerManagerTest, FileHandlersAreNotAvailableUnlessEnabled) {
  const AppId app_id = "app-id";

  file_handler_manager().InstallFileHandler(app_id,
                                            GURL("https://app.site/handle-foo"),
                                            {{"application/foo", {".foo"}}},
                                            /*enable=*/false);

  file_handler_manager().InstallFileHandler(app_id,
                                            GURL("https://app.site/handle-bar"),
                                            {{"application/bar", {".bar"}}},
                                            /*enable=*/false);

  // File handlers are disabled by default.
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id);
    EXPECT_EQ(nullptr, handlers);
  }

  // Ensure they can be enabled.
  file_handler_manager().EnableAndRegisterOsFileHandlers(app_id);
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id);
    EXPECT_EQ(2u, handlers->size());
  }

  // Ensure they can be disabled.
  file_handler_manager().DisableAndUnregisterOsFileHandlers(app_id, nullptr,
                                                            base::DoNothing());

  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id);
    EXPECT_EQ(nullptr, handlers);
  }
}

TEST_F(FileHandlerManagerTest, NoHandlersRegistered) {
  const AppId app_id = "app-id";

  // Returns nullopt when no file handlers are registered.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, NoLaunchFilesPassed) {
  const AppId app_id = "app-id";

  file_handler_manager().InstallFileHandler(app_id,
                                            GURL("https://app.site/handle-foo"),
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt when no launch files are passed.
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {}));
}

TEST_F(FileHandlerManagerTest, SingleValidExtensionSingleExtensionHandler) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url,
                                            {{"application/foo", {".foo"}}});

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(url,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, SingleInvalidExtensionSingleExtensionHandler) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt on single invalid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, SingleValidExtensionMultiExtensionHandler) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id, GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}, {"application/bar", {".bar"}}});

  // Matches on single valid extension for multi-extension handler.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(url,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, MultipleValidExtensions) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id, GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}, {"application/bar", {".bar"}}});

  // Matches on multiple valid extensions for multi-extension handler.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(url, file_handler_manager().GetMatchingFileHandlerURL(
                     app_id, {path1, path2}));
}

TEST_F(FileHandlerManagerTest, PartialExtensionMatch) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt on partial extension match.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(base::nullopt, file_handler_manager().GetMatchingFileHandlerURL(
                               app_id, {path1, path2}));
}

TEST_F(FileHandlerManagerTest, SingleFileWithoutExtension) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt where a file has no extension.
  const base::FilePath path(FILE_PATH_LITERAL("file"));
  EXPECT_EQ(base::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id, {path}));
}

TEST_F(FileHandlerManagerTest, FileWithoutExtensionAmongMultipleFiles) {
  const AppId app_id = "app-id";
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id, url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt where one file has no extension while others do.
  const base::FilePath path1(FILE_PATH_LITERAL("file"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(base::nullopt, file_handler_manager().GetMatchingFileHandlerURL(
                               app_id, {path1, path2}));
}

}  // namespace web_app
