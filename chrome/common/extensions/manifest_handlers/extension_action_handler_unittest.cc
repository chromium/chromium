// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/extension_action_handler.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::FilePath GetTestDataDir() {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.AppendASCII("extensions").AppendASCII("manifest_handlers");
}

}  // namespace

namespace extensions {

// Tests that an unpacked extension with an invisible browser action
// default icon fails as expected.
TEST(ExtensionActionHandlerTest, LoadInvisibleBrowserActionIconUnpacked) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("browser_action_invisible_icon");
  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ("The icon is not sufficiently visible 'invisible_icon.png'.",
            error);
}

// Tests that an unpacked extension with an invisible page action
// default icon fails as expected.
TEST(ExtensionActionHandlerTest, LoadInvisiblePageActionIconUnpacked) {
  base::FilePath extension_dir =
      GetTestDataDir().AppendASCII("page_action_invisible_icon");
  // Set the flag that enables the error.
  file_util::SetReportErrorForInvisibleIconForTesting(true);
  std::string error;
  scoped_refptr<Extension> extension(file_util::LoadExtension(
      extension_dir, Manifest::UNPACKED, Extension::NO_FLAGS, &error));
  file_util::SetReportErrorForInvisibleIconForTesting(false);
  EXPECT_FALSE(extension);
  EXPECT_EQ("The icon is not sufficiently visible 'invisible_icon.png'.",
            error);
}

}  // namespace extensions
