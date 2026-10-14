// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/experiences/extensions/common/api/file_browser_handlers/file_browser_handler.h"
#include "chromeos/ash/experiences/extensions/common/chromeos_extensions_api_provider.h"
#include "components/version_info/channel.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_test.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "extensions/test/test_extensions_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace errors = extensions::manifest_errors;

namespace extensions {
namespace {

class FileBrowserHandlerManifestTest : public ManifestTest {
 public:
  FileBrowserHandlerManifestTest()
      : current_channel_(version_info::Channel::UNKNOWN) {
    url::AddStandardScheme(kExtensionScheme, url::SCHEME_WITH_HOST);
  }

  // `ExtensionsClient::Set()` initializes the process-wide `PermissionsInfo`
  // singleton, which `DCHECK`s that permissions are not registered more than
  // once and cannot be reset between individual tests. Therefore, set it up
  // once per test suite rather than in `SetUp()`/`TearDown()`.
  // Bare pointers and `new`/`delete` are used for the static members to avoid
  // static initializers and exit-time destructors.
  static void SetUpTestSuite() {
    ManifestTest::SetUpTestSuite();
    scoped_manifest_handler_registry_ =
        new ScopedTestingManifestHandlerRegistry();
    extensions_client_ = new TestExtensionsClient();
    extensions_client_->AddAPIProvider(
        std::make_unique<ash::ChromeOSExtensionsAPIProvider>());
    ExtensionsClient::Set(extensions_client_);
  }

  static void TearDownTestSuite() {
    ExtensionsClient::Set(nullptr);
    delete extensions_client_;
    extensions_client_ = nullptr;
    delete scoped_manifest_handler_registry_;
    scoped_manifest_handler_registry_ = nullptr;
    ManifestTest::TearDownTestSuite();
  }

 protected:
  base::FilePath GetTestDataDir() override {
    base::FilePath path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path);
    return path.Append(FILE_PATH_LITERAL("chromeos"))
        .Append(FILE_PATH_LITERAL("ash"))
        .Append(FILE_PATH_LITERAL("experiences"))
        .Append(FILE_PATH_LITERAL("extensions"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"));
  }

 private:
  url::ScopedSchemeRegistryForTests scoped_scheme_registry_;
  ScopedCurrentChannel current_channel_;
  static ScopedTestingManifestHandlerRegistry*
      scoped_manifest_handler_registry_;
  static TestExtensionsClient* extensions_client_;
};

ScopedTestingManifestHandlerRegistry*
    FileBrowserHandlerManifestTest::scoped_manifest_handler_registry_ = nullptr;
TestExtensionsClient* FileBrowserHandlerManifestTest::extensions_client_ =
    nullptr;

TEST_F(FileBrowserHandlerManifestTest, PermissionAllowed) {
  RunTestcase(Testcase("filebrowser_valid.json"), ExpectType::kSuccess);
}

TEST_F(FileBrowserHandlerManifestTest, GetHandlersRequiresPermission) {
  auto bad_manifest =
      base::DictValue()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("file_browser_handlers",
               base::ListValue().Append(
                   base::DictValue()
                       .Set("id", "open")
                       .Set("default_title", "open")
                       .Set("file_filters", base::ListValue()
                                                .Append("filesystem:*.txt")
                                                .Append("filesystem:*.html"))));
  // Create a good manifest by extending the bad one with the missing
  // permission.
  auto good_manifest = bad_manifest.Clone();
  good_manifest.Set("permissions",
                    base::ListValue().Append("fileBrowserHandler"));

  ExtensionBuilder bad_app_builder;
  bad_app_builder.SetManifest(std::move(bad_manifest));
  scoped_refptr<const Extension> bad_app = bad_app_builder.Build();
  EXPECT_FALSE(FileBrowserHandler::GetHandlers(bad_app.get()));

  ExtensionBuilder good_app_builder;
  good_app_builder.SetManifest(std::move(good_manifest));
  scoped_refptr<const Extension> good_app = good_app_builder.Build();
  EXPECT_TRUE(FileBrowserHandler::GetHandlers(good_app.get()));
}

TEST_F(FileBrowserHandlerManifestTest, InvalidFileBrowserHandlers) {
  const Testcase testcases[] = {
      Testcase("filebrowser_invalid_access_permission.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidFileAccessValue,
                                              base::NumberToString(1))),
      Testcase("filebrowser_invalid_access_permission_list.json",
               errors::kInvalidFileAccessList),
      Testcase("filebrowser_invalid_empty_access_permission_list.json",
               errors::kInvalidFileAccessList),
      Testcase("filebrowser_invalid_value.json",
               errors::kInvalidFileBrowserHandler),
      Testcase("filebrowser_invalid_actions_1.json",
               errors::kInvalidFileBrowserHandler),
      Testcase("filebrowser_invalid_actions_2.json",
               errors::kInvalidFileBrowserHandler),
      Testcase("filebrowser_invalid_action_id.json",
               errors::kInvalidFileBrowserHandlerId),
      Testcase("filebrowser_invalid_action_title.json",
               errors::kInvalidActionDefaultTitle),
      Testcase("filebrowser_invalid_file_filters_1.json",
               errors::kInvalidFileFiltersList),
      Testcase("filebrowser_invalid_file_filters_2.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidFileFilterValue,
                                              base::NumberToString(0))),
      Testcase("filebrowser_invalid_file_filters_url.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidURLPatternError,
                                              "http:*.html"))};
  RunTestcases(testcases, ExpectType::kError);
  RunTestcase(Testcase("filebrowser_missing_permission.json",
                       errors::kInvalidFileBrowserHandlerMissingPermission),
              ExpectType::kWarning);
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandler) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              base::DictValue()
                  .Set("name", "file browser handler test")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       base::ListValue().Append("fileBrowserHandler"))
                  .Set("file_browser_handlers",
                       base::ListValue().Append(
                           base::DictValue()
                               .Set("id", "ExtremelyCoolAction")
                               .Set("default_title", "Be Amazed")
                               .Set("default_icon", "icon.png")
                               .Set("file_filters", base::ListValue().Append(
                                                        "filesystem:*.txt")))))
          .Build();

  ASSERT_TRUE(extension.get());
  const FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension.get());
  ASSERT_TRUE(handlers);
  ASSERT_EQ(1U, handlers->size());
  const FileBrowserHandler* action = handlers->at(0).get();

  EXPECT_EQ("ExtremelyCoolAction", action->id());
  EXPECT_EQ("Be Amazed", action->title());
  EXPECT_EQ("icon.png", action->icon_path());
  const URLPatternSet& patterns = action->file_url_patterns();
  ASSERT_EQ(1U, patterns.patterns().size());
  EXPECT_TRUE(action->MatchesURL(
      GURL("filesystem:chrome-extension://foo/local/test.txt")));
  EXPECT_FALSE(action->HasCreateAccessPermission());
  EXPECT_TRUE(action->CanRead());
  EXPECT_TRUE(action->CanWrite());

  EXPECT_EQ(action, FileBrowserHandler::FindForActionId(extension.get(),
                                                        "ExtremelyCoolAction"));
  EXPECT_EQ(nullptr, FileBrowserHandler::FindForActionId(extension.get(),
                                                         "(does not exist)"));
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandlerMIMETypes) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetID(extension_misc::kQuickOfficeExtensionId)
          .SetManifest(
              base::DictValue()
                  .Set("name", "file browser handler test")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       base::ListValue().Append("fileBrowserHandler"))
                  .Set("file_browser_handlers",
                       base::ListValue().Append(
                           base::DictValue()
                               .Set("id", "ID")
                               .Set("default_title", "Default title")
                               .Set("default_icon", "icon.png")
                               .Set("file_filters", base::ListValue().Append(
                                                        "filesystem:*.txt")))))
          .Build();

  ASSERT_TRUE(extension.get());
  const FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension.get());
  ASSERT_TRUE(handlers);
  ASSERT_EQ(1U, handlers->size());
  const FileBrowserHandler* action = handlers->at(0).get();

  const URLPatternSet& patterns = action->file_url_patterns();
  ASSERT_EQ(1U, patterns.patterns().size());
  EXPECT_TRUE(action->MatchesURL(
      GURL("filesystem:chrome-extension://foo/local/test.txt")));

  EXPECT_EQ(action, FileBrowserHandler::FindForActionId(extension.get(), "ID"));
  EXPECT_EQ(nullptr, FileBrowserHandler::FindForActionId(extension.get(),
                                                         "(does not exist)"));
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandlerWithCreate) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              base::DictValue()
                  .Set("name", "file browser handler test create")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       base::ListValue().Append("fileBrowserHandler"))
                  .Set("file_browser_handlers",
                       base::ListValue().Append(
                           base::DictValue()
                               .Set("id", "ID")
                               .Set("default_title", "Default title")
                               .Set("default_icon", "icon.png")
                               .Set("file_filters", base::ListValue().Append(
                                                        "filesystem:*.txt"))
                               .Set("file_access",
                                    base::ListValue().Append("create")))))
          .Build();

  ASSERT_TRUE(extension.get());
  const FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension.get());
  ASSERT_TRUE(handlers);
  ASSERT_EQ(1U, handlers->size());
  const FileBrowserHandler* action = handlers->at(0).get();
  const URLPatternSet& patterns = action->file_url_patterns();

  EXPECT_EQ(0U, patterns.patterns().size());
  EXPECT_TRUE(action->HasCreateAccessPermission());
  EXPECT_FALSE(action->CanRead());
  EXPECT_FALSE(action->CanWrite());

  EXPECT_EQ(action, FileBrowserHandler::FindForActionId(extension.get(), "ID"));
  EXPECT_EQ(nullptr, FileBrowserHandler::FindForActionId(extension.get(),
                                                         "(does not exist)"));
}

}  // namespace
}  // namespace extensions
