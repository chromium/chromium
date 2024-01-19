// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

// Tests that ChromePermissionMessageProvider provides correct permission
// messages for given permissions.
// NOTE: No extensions are created as part of these tests. Integration tests
// that test the messages are generated properly for extensions can be found in
// chrome/browser/extensions/permission_messages_unittest.cc.
class ChromePermissionMessageProviderUnittest : public ChromeManifestTest {
 public:
  ChromePermissionMessageProviderUnittest()
      : message_provider_(new ChromePermissionMessageProvider()) {}

  ChromePermissionMessageProviderUnittest(
      const ChromePermissionMessageProviderUnittest&) = delete;
  ChromePermissionMessageProviderUnittest& operator=(
      const ChromePermissionMessageProviderUnittest&) = delete;

  ~ChromePermissionMessageProviderUnittest() override {}

 protected:
  PermissionMessages GetMessages(const APIPermissionSet& permissions,
                                 Manifest::Type type) {
    return message_provider_->GetPermissionMessages(
        message_provider_->GetAllPermissionIDs(
            PermissionSet(permissions.Clone(), ManifestPermissionSet(),
                          URLPatternSet(), URLPatternSet()),
            type));
  }

  PermissionMessages GetManagementUIPermissionIDs(
      const APIPermissionSet& api_permissions,
      const ManifestPermissionSet& manifest_permissions,
      Manifest::Type type) {
    return message_provider_->GetPermissionMessages(
        message_provider_->GetManagementUIPermissionIDs(
            PermissionSet(api_permissions.Clone(), manifest_permissions.Clone(),
                          URLPatternSet(), URLPatternSet()),
            type));
  }

  bool IsPrivilegeIncrease(const APIPermissionSet& granted_permissions,
                           const URLPatternSet& granted_hosts,
                           const APIPermissionSet& requested_permissions,
                           const URLPatternSet& requested_hosts) {
    return message_provider_->IsPrivilegeIncrease(
        PermissionSet(granted_permissions.Clone(), ManifestPermissionSet(),
                      granted_hosts.Clone(), URLPatternSet()),
        PermissionSet(requested_permissions.Clone(), ManifestPermissionSet(),
                      requested_hosts.Clone(), URLPatternSet()),
        Manifest::TYPE_EXTENSION);
  }

  ChromePermissionMessageProvider* message_provider() {
    return message_provider_.get();
  }

 private:
  void SetUp() override {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  std::unique_ptr<ChromePermissionMessageProvider> message_provider_;
};

// Checks that if an app has a superset and a subset permission, only the
// superset permission message is displayed if they are both present.
TEST_F(ChromePermissionMessageProviderUnittest,
       SupersetOverridesSubsetPermission) {
  {
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kTab);
    PermissionMessages messages =
        GetMessages(permissions, Manifest::TYPE_PLATFORM_APP);
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
        messages.front().message());
  }
  {
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kTopSites);
    PermissionMessages messages =
        GetMessages(permissions, Manifest::TYPE_PLATFORM_APP);
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_TOPSITES),
              messages.front().message());
  }
  {
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kTab);
    permissions.insert(APIPermissionID::kTopSites);
    PermissionMessages messages =
        GetMessages(permissions, Manifest::TYPE_PLATFORM_APP);
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
        messages.front().message());
  }
}

// Checks that when permissions are merged into a single message, their details
// are merged as well.
TEST_F(ChromePermissionMessageProviderUnittest,
       WarningsAndDetailsCoalesceTogether) {
  // kTab and kTopSites should be merged into a single message.
  APIPermissionSet permissions;
  permissions.insert(APIPermissionID::kTab);
  permissions.insert(APIPermissionID::kTopSites);
  // The USB device permission message has a non-empty details string.
  std::unique_ptr<UsbDevicePermission> usb(
      new UsbDevicePermission(PermissionsInfo::GetInstance()->GetByID(
          mojom::APIPermissionID::kUsbDevice)));
  base::Value devices_list(base::Value::Type::LIST);
  devices_list.GetList().Append(base::Value::FromUniquePtrValue(
      UsbDevicePermissionData(0x02ad, 0x138c, -1, -1).ToValue()));
  devices_list.GetList().Append(base::Value::FromUniquePtrValue(
      UsbDevicePermissionData(0x02ad, 0x138d, -1, -1).ToValue()));
  ASSERT_TRUE(usb->FromValue(&devices_list, nullptr, nullptr));
  permissions.insert(std::move(usb));

  PermissionMessages messages =
      GetMessages(permissions, Manifest::TYPE_EXTENSION);

  ASSERT_EQ(2U, messages.size());
  auto it = messages.begin();
  const PermissionMessage& message0 = *it++;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
      message0.message());
  EXPECT_TRUE(message0.submessages().empty());
  const PermissionMessage& message1 = *it++;
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
      message1.message());
  EXPECT_FALSE(message1.submessages().empty());
}

// Check that if IDN domains are provided in host permissions, then those
// domains are converted to punycode.
TEST_F(ChromePermissionMessageProviderUnittest,
       IDNDomainsInHostPermissionsArePunycoded) {
  extensions::URLPatternSet explicit_hosts;

  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "https://ɡoogle.com/"));
  explicit_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "https://*.ɡoogle.com/"));
  extensions::PermissionSet permissions(
      APIPermissionSet(), ManifestPermissionSet(), std::move(explicit_hosts),
      URLPatternSet());

  PermissionMessages messages = message_provider()->GetPermissionMessages(
      message_provider()->GetAllPermissionIDs(permissions,
                                              Manifest::TYPE_EXTENSION));

  ASSERT_EQ(1U, messages.size());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
                                       u"all xn--oogle-qmc.com sites",
                                       u"xn--oogle-qmc.com"),
            messages.front().message());
}

// Checks whether powerful permissions are returned correctly.
TEST_F(ChromePermissionMessageProviderUnittest, PowerfulPermissions) {
  {
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kTab);
    PermissionMessages messages = GetManagementUIPermissionIDs(
        permissions, ManifestPermissionSet(), Manifest::TYPE_EXTENSION);
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
        messages.front().message());
  }
  {
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kBookmark);
    PermissionMessages messages = GetManagementUIPermissionIDs(
        permissions, ManifestPermissionSet(), Manifest::TYPE_EXTENSION);
    ASSERT_EQ(0U, messages.size());
  }
  {
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kTab);
    permissions.insert(APIPermissionID::kBookmark);
    PermissionMessages messages = GetManagementUIPermissionIDs(
        permissions, ManifestPermissionSet(), Manifest::TYPE_EXTENSION);
    ASSERT_EQ(1U, messages.size());
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ),
        messages.front().message());
  }
  {
    scoped_refptr<Extension> extension =
        ManifestTest::LoadAndExpectSuccess("automation_desktop_true.json");
    ASSERT_TRUE(extension.get());
    ManifestPermissionSet manifest_permissions = extension->permissions_data()
                                                     ->active_permissions()
                                                     .manifest_permissions()
                                                     .Clone();
    APIPermissionSet permissions;
    permissions.insert(APIPermissionID::kTab);
    permissions.insert(APIPermissionID::kBookmark);
    permissions.insert(APIPermissionID::kDebugger);
    PermissionMessages messages = GetManagementUIPermissionIDs(
        permissions, manifest_permissions, Manifest::TYPE_EXTENSION);
    ASSERT_EQ(2U, messages.size());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_DEBUGGER),
              messages.front().message());
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS),
        messages[1].message());
  }
}

// Checks that granted hosts that may cause API permission messages are
// processed as part of IsPrivilegeIncrease. Regression test for
// crbug.com/1014505.
TEST_F(ChromePermissionMessageProviderUnittest, PrivilegeIncreaseAllUrls) {
  APIPermissionSet granted_permissions;
  granted_permissions.insert(APIPermissionID::kWebRequest);

  extensions::URLPatternSet granted_hosts;
  granted_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL, "<all_urls>"));

  APIPermissionSet requested_permissions;
  requested_permissions.insert(APIPermissionID::kWebRequest);
  requested_permissions.insert(APIPermissionID::kDeclarativeNetRequest);

  extensions::URLPatternSet requested_hosts;
  requested_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL, "<all_urls>"));

  // While |kDeclarativeNetRequest| would cause a permission message, the
  // inclusion of <all_urls> for both granted and request permissions should
  // subsume the permission message for |kDeclarativeNetRequest| with its own
  // message. Since this message would be identical between
  // |granted_permissions| and |requested_permissions|, there should not be a
  // privilege increase.
  EXPECT_FALSE(IsPrivilegeIncrease(granted_permissions, granted_hosts,
                                   requested_permissions, requested_hosts));
}

}  // namespace extensions
