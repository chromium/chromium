// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"

#include <memory>
#include <utility>

#include "apps/saved_files_service.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

using base::FilePath;
using testing::Contains;
using testing::Eq;

class AppInfoPermissionsPanelTest : public testing::Test {
 protected:
  AppInfoPermissionsPanelTest() {}

  std::unique_ptr<base::DictionaryValue> ValidAppManifest() {
    return extensions::DictionaryBuilder()
        .Set("name", "Test App Name")
        .Set("version", "2.0")
        .Set("manifest_version", 2)
        .Set("app", extensions::DictionaryBuilder()
                        .Set("background",
                             extensions::DictionaryBuilder()
                                 .Set("scripts", extensions::ListBuilder()
                                                     .Append("background.js")
                                                     .Build())
                                 .Build())
                        .Build())
        .Build();
  }

  // We need the UI thread in order to construct UI elements in the view.
  content::BrowserTaskEnvironment task_environment_;
  ChromeTestViewsDelegate views_delegate_;
  TestingProfile profile_;  // Needs BrowserThread::UI.
};

// Tests that an app with no permissions is treated correctly.
TEST_F(AppInfoPermissionsPanelTest, NoPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());

  EXPECT_TRUE(VerifyNoPermissionMessages(panel.app_->permissions_data()));
  EXPECT_TRUE(panel.GetRetainedFilePaths().empty());
}

// Tests that an app's required permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsPanelTest, RequiredPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(
              extensions::DictionaryBuilder()
                  .Set("permissions", extensions::ListBuilder()
                                          // A valid permission with a message
                                          .Append("desktopCapture")
                                          // An invalid permission
                                          .Append("bad_perm")
                                          // An valid permission with no message
                                          .Append("cookies")
                                          // A valid permission with a message
                                          .Append("serial")
                                          .Build())
                  .Build())
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());

  EXPECT_TRUE(VerifyTwoPermissionMessages(
      panel.app_->permissions_data(),
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE),
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_SERIAL),
      false));
}

// Tests that an app's optional permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsPanelTest, OptionalPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder()
                             .Set("optional_permissions",
                                  extensions::ListBuilder()
                                      // A valid permission with a message
                                      .Append("clipboardRead")
                                      // An invalid permission
                                      .Append("bad_perm")
                                      // A valid permission with no message
                                      .Append("idle")
                                      // Another valid permission with a message
                                      .Append("serial")
                                      .Build())
                             .Build())
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());

  // Optional permissions don't appear until they are 'activated' at runtime.
  // TODO(sashab): Activate the optional permissions and ensure they are
  // successfully added to the dialog.
  EXPECT_TRUE(VerifyNoPermissionMessages(panel.app_->permissions_data()));
  EXPECT_TRUE(panel.GetRetainedFilePaths().empty());
}

// Tests that an app's retained files are detected and converted to paths
// correctly.
TEST_F(AppInfoPermissionsPanelTest, RetainedFilePermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(
              extensions::DictionaryBuilder()
                  .Set("permissions",
                       extensions::ListBuilder()
                           .Append(extensions::DictionaryBuilder()
                                       .Set("fileSystem",
                                            extensions::ListBuilder()
                                                .Append("retainEntries")
                                                .Build())
                                       .Build())
                           .Build())
                  .Build())
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());
  apps::SavedFilesService* files_service =
      apps::SavedFilesService::Get(&profile_);
  files_service->RegisterFileEntry(
      app->id(), "file_id_1", FilePath(FILE_PATH_LITERAL("file_1.ext")), false);
  files_service->RegisterFileEntry(
      app->id(), "file_id_2", FilePath(FILE_PATH_LITERAL("file_2.ext")), false);
  files_service->RegisterFileEntry(
      app->id(), "file_id_3", FilePath(FILE_PATH_LITERAL("file_3.ext")), false);

  ASSERT_TRUE(VerifyNoPermissionMessages(panel.app_->permissions_data()));

  // Since we have no guarantees on the order of retained files, make sure the
  // list is the expected length and all required entries are present.
  const std::vector<base::string16> retained_file_paths =
      panel.GetRetainedFilePaths();
  ASSERT_EQ(3U, retained_file_paths.size());
  EXPECT_THAT(retained_file_paths,
              Contains(Eq(base::UTF8ToUTF16("file_1.ext"))));
  EXPECT_THAT(retained_file_paths,
              Contains(Eq(base::UTF8ToUTF16("file_2.ext"))));
  EXPECT_THAT(retained_file_paths,
              Contains(Eq(base::UTF8ToUTF16("file_3.ext"))));
}
