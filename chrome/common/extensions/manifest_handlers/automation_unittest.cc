// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/automation.h"

#include "base/command_line.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

class AutomationManifestTest : public ChromeManifestTest {
 public:
  AutomationManifestTest() : channel_(version_info::Channel::UNKNOWN) {}

 protected:
  AutomationInfo* GetAutomationInfo(scoped_refptr<Extension> extension) {
    return static_cast<AutomationInfo*>(
        extension->GetManifestData(manifest_keys::kAutomation));
  }

 private:
  void SetUp() override {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  ScopedCurrentChannel channel_;
};

TEST_F(AutomationManifestTest, AsBooleanFalse) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_boolean_false.json");
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(VerifyNoPermissionMessages(extension->permissions_data()));

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_FALSE(info);
}

TEST_F(AutomationManifestTest, AsBooleanTrue) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_boolean_true.json");
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(VerifyOnePermissionMessage(
      extension->permissions_data(),
      "Read and change your data on www.google.com"));

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
}

TEST_F(AutomationManifestTest, DesktopFalse) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_desktop_false.json");
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(VerifyOnePermissionMessage(
      extension->permissions_data(),
      "Read and change your data on www.google.com"));

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_FALSE(info->desktop);
}

TEST_F(AutomationManifestTest, DesktopTrue) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("automation_desktop_true.json");
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(VerifyOnePermissionMessage(
      extension->permissions_data(),
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS)));

  const AutomationInfo* info = AutomationInfo::Get(extension.get());
  ASSERT_TRUE(info);

  EXPECT_TRUE(info->desktop);
}

}  // namespace extensions
