// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"

#include <set>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/test/fake_web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
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
 public:
  WebAppFileHandlerManagerTest() {
    // |features_| needs to be initialized before SetUp kicks off tasks that
    // check if a feature is enabled.
    features_.InitAndEnableFeature(blink::features::kFileHandlingAPI);
  }

 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());

    file_handler_manager_ =
        std::make_unique<FakeWebAppFileHandlerManager>(profile());
    file_handler_manager_->SetSubsystems(&controller().sync_bridge());

    controller().Init();

    auto web_app = test::CreateWebApp();
    app_id_ = web_app->app_id();
    controller().RegisterApp(std::move(web_app));
  }

  FakeWebAppFileHandlerManager& file_handler_manager() {
    return *file_handler_manager_;
  }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  const AppId& app_id() const { return app_id_; }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<FakeWebAppFileHandlerManager> file_handler_manager_;

  base::test::ScopedFeatureList features_;
  AppId app_id_;
};

TEST_F(WebAppFileHandlerManagerTest, FileHandlersAreNotAvailableUnlessEnabled) {
  // TODO(crbug/1288442): re-enable this test
  if (!ShouldRegisterFileHandlersWithOs()) {
    GTEST_SKIP();
  }

  file_handler_manager().InstallFileHandler(app_id(),
                                            GURL("https://app.site/handle-foo"),
                                            {{"application/foo", {".foo"}}},
                                            /*enable=*/false);

  file_handler_manager().InstallFileHandler(app_id(),
                                            GURL("https://app.site/handle-bar"),
                                            {{"application/bar", {".bar"}}},
                                            /*enable=*/false);

  // File handlers are disabled by default.
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id());
    EXPECT_EQ(nullptr, handlers);
  }

  // Ensure they can be enabled.
  file_handler_manager().EnableAndRegisterOsFileHandlers(app_id());
  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id());
    EXPECT_EQ(2u, handlers->size());
  }

  // Ensure they can be disabled.
  file_handler_manager().DisableAndUnregisterOsFileHandlers(app_id(),
                                                            base::DoNothing());

  {
    const auto* handlers =
        file_handler_manager().GetEnabledFileHandlers(app_id());
    EXPECT_EQ(nullptr, handlers);
  }
}

TEST_F(WebAppFileHandlerManagerTest, NoHandlersRegistered) {
  // Returns nullopt when no file handlers are registered.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(absl::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id(), {path}));
}

TEST_F(WebAppFileHandlerManagerTest, NoLaunchFilesPassed) {
  file_handler_manager().InstallFileHandler(app_id(),
                                            GURL("https://app.site/handle-foo"),
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt when no launch files are passed.
  EXPECT_EQ(absl::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id(), {}));
}

TEST_F(WebAppFileHandlerManagerTest,
       SingleValidExtensionSingleExtensionHandler) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id(), url,
                                            {{"application/foo", {".foo"}}});

  // Matches on single valid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(url,
            file_handler_manager().GetMatchingFileHandlerURL(app_id(), {path}));
}

TEST_F(WebAppFileHandlerManagerTest,
       SingleInvalidExtensionSingleExtensionHandler) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id(), url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt on single invalid extension.
  const base::FilePath path(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(absl::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id(), {path}));
}

TEST_F(WebAppFileHandlerManagerTest,
       SingleValidExtensionMultiExtensionHandler) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}, {"application/bar", {".bar"}}});

  // Matches on single valid extension for multi-extension handler.
  const base::FilePath path(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(url,
            file_handler_manager().GetMatchingFileHandlerURL(app_id(), {path}));
}

TEST_F(WebAppFileHandlerManagerTest, MultipleValidExtensions) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(
      app_id(), GURL("https://app.site/handle-foo"),
      {{"application/foo", {".foo"}}, {"application/bar", {".bar"}}});

  // Matches on multiple valid extensions for multi-extension handler.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(url, file_handler_manager().GetMatchingFileHandlerURL(
                     app_id(), {path1, path2}));
}

TEST_F(WebAppFileHandlerManagerTest, PartialExtensionMatch) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id(), url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt on partial extension match.
  const base::FilePath path1(FILE_PATH_LITERAL("file.foo"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.bar"));
  EXPECT_EQ(absl::nullopt, file_handler_manager().GetMatchingFileHandlerURL(
                               app_id(), {path1, path2}));
}

TEST_F(WebAppFileHandlerManagerTest, SingleFileWithoutExtension) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id(), url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt where a file has no extension.
  const base::FilePath path(FILE_PATH_LITERAL("file"));
  EXPECT_EQ(absl::nullopt,
            file_handler_manager().GetMatchingFileHandlerURL(app_id(), {path}));
}

TEST_F(WebAppFileHandlerManagerTest, FileWithoutExtensionAmongMultipleFiles) {
  const GURL url("https://app.site/handle-foo");

  file_handler_manager().InstallFileHandler(app_id(), url,
                                            {{"application/foo", {".foo"}}});

  // Returns nullopt where one file has no extension while others do.
  const base::FilePath path1(FILE_PATH_LITERAL("file"));
  const base::FilePath path2(FILE_PATH_LITERAL("file.foo"));
  EXPECT_EQ(absl::nullopt, file_handler_manager().GetMatchingFileHandlerURL(
                               app_id(), {path1, path2}));
}

}  // namespace web_app
