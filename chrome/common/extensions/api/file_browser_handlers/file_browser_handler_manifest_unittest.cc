// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

using extensions::Extension;
using extensions::ExtensionBuilder;

namespace {

class FileBrowserHandlerManifestTest : public ChromeManifestTest {
};

TEST_F(FileBrowserHandlerManifestTest, PermissionAllowed) {
  RunTestcase(Testcase("filebrowser_valid.json"), EXPECT_TYPE_SUCCESS);
}

TEST_F(FileBrowserHandlerManifestTest, GetHandlersRequiresPermission) {
  auto bad_manifest =
      base::Value::Dict()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("file_browser_handlers",
               base::Value::List().Append(
                   base::Value::Dict()
                       .Set("id", "open")
                       .Set("default_title", "open")
                       .Set("file_filters", base::Value::List()
                                                .Append("filesystem:*.txt")
                                                .Append("filesystem:*.html"))));
  // Create a good manifest by extending the bad one with the missing
  // permission.
  auto good_manifest = bad_manifest.Clone();
  good_manifest.Set("permissions",
                    base::Value::List().Append("fileBrowserHandler"));

  extensions::ExtensionBuilder bad_app_builder;
  bad_app_builder.SetManifest(std::move(bad_manifest));
  scoped_refptr<const extensions::Extension> bad_app = bad_app_builder.Build();
  EXPECT_FALSE(FileBrowserHandler::GetHandlers(bad_app.get()));

  extensions::ExtensionBuilder good_app_builder;
  good_app_builder.SetManifest(std::move(good_manifest));
  scoped_refptr<const extensions::Extension> good_app =
      good_app_builder.Build();
  EXPECT_TRUE(FileBrowserHandler::GetHandlers(good_app.get()));
}

TEST_F(FileBrowserHandlerManifestTest, InvalidFileBrowserHandlers) {
  Testcase testcases[] = {
      Testcase("filebrowser_invalid_access_permission.json",
               extensions::ErrorUtils::FormatErrorMessage(
                   errors::kInvalidFileAccessValue, base::NumberToString(1))),
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
               extensions::ErrorUtils::FormatErrorMessage(
                   errors::kInvalidFileFilterValue, base::NumberToString(0))),
      Testcase("filebrowser_invalid_file_filters_url.json",
               extensions::ErrorUtils::FormatErrorMessage(
                   errors::kInvalidURLPatternError, "http:*.html"))};
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
  RunTestcase(Testcase("filebrowser_missing_permission.json",
                       errors::kInvalidFileBrowserHandlerMissingPermission),
              EXPECT_TYPE_WARNING);
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandler) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "file browser handler test")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       base::Value::List().Append("fileBrowserHandler"))
                  .Set("file_browser_handlers",
                       base::Value::List().Append(
                           base::Value::Dict()
                               .Set("id", "ExtremelyCoolAction")
                               .Set("default_title", "Be Amazed")
                               .Set("default_icon", "icon.png")
                               .Set("file_filters", base::Value::List().Append(
                                                        "filesystem:*.txt")))))
          .Build();

  ASSERT_TRUE(extension.get());
  FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension.get());
  ASSERT_TRUE(handlers != nullptr);
  ASSERT_EQ(1U, handlers->size());
  const FileBrowserHandler* action = handlers->at(0).get();

  EXPECT_EQ("ExtremelyCoolAction", action->id());
  EXPECT_EQ("Be Amazed", action->title());
  EXPECT_EQ("icon.png", action->icon_path());
  const extensions::URLPatternSet& patterns = action->file_url_patterns();
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
              base::Value::Dict()
                  .Set("name", "file browser handler test")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       base::Value::List().Append("fileBrowserHandler"))
                  .Set("file_browser_handlers",
                       base::Value::List().Append(
                           base::Value::Dict()
                               .Set("id", "ID")
                               .Set("default_title", "Default title")
                               .Set("default_icon", "icon.png")
                               .Set("file_filters", base::Value::List().Append(
                                                        "filesystem:*.txt")))))
          .Build();

  ASSERT_TRUE(extension.get());
  FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension.get());
  ASSERT_TRUE(handlers != nullptr);
  ASSERT_EQ(1U, handlers->size());
  const FileBrowserHandler* action = handlers->at(0).get();

  const extensions::URLPatternSet& patterns = action->file_url_patterns();
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
              base::Value::Dict()
                  .Set("name", "file browser handler test create")
                  .Set("version", "1.0.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       base::Value::List().Append("fileBrowserHandler"))
                  .Set("file_browser_handlers",
                       base::Value::List().Append(
                           base::Value::Dict()
                               .Set("id", "ID")
                               .Set("default_title", "Default title")
                               .Set("default_icon", "icon.png")
                               .Set("file_filters", base::Value::List().Append(
                                                        "filesystem:*.txt"))
                               .Set("file_access",
                                    base::Value::List().Append("create")))))
          .Build();

  ASSERT_TRUE(extension.get());
  FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension.get());
  ASSERT_TRUE(handlers != nullptr);
  ASSERT_EQ(1U, handlers->size());
  const FileBrowserHandler* action = handlers->at(0).get();
  const extensions::URLPatternSet& patterns = action->file_url_patterns();

  EXPECT_EQ(0U, patterns.patterns().size());
  EXPECT_TRUE(action->HasCreateAccessPermission());
  EXPECT_FALSE(action->CanRead());
  EXPECT_FALSE(action->CanWrite());

  EXPECT_EQ(action, FileBrowserHandler::FindForActionId(extension.get(), "ID"));
  EXPECT_EQ(nullptr, FileBrowserHandler::FindForActionId(extension.get(),
                                                         "(does not exist)"));
}

}  // namespace
