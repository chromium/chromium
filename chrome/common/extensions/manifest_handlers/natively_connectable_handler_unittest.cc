// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/natively_connectable_handler.h"

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {
using NativelyConnectableManifestTest = ChromeManifestTest;

TEST_F(NativelyConnectableManifestTest, Basic) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("natively_connectable.json");
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(VerifyNoPermissionMessages(extension->permissions_data()));

  auto* hosts =
      NativelyConnectableHosts::GetConnectableNativeMessageHosts(*extension);
  ASSERT_TRUE(hosts);
  EXPECT_EQ(*hosts, (std::set<std::string>{
                        "com.google.chrome.test.echo",
                        "com.google.chrome.test.host_binary_missing",
                    }));
}

TEST_F(NativelyConnectableManifestTest, Unset) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("natively_connectable_unset.json");
  ASSERT_TRUE(extension.get());

  EXPECT_TRUE(VerifyNoPermissionMessages(extension->permissions_data()));

  EXPECT_FALSE(
      NativelyConnectableHosts::GetConnectableNativeMessageHosts(*extension));
}

TEST_F(NativelyConnectableManifestTest, IncorrectType) {
  LoadAndExpectError("natively_connectable_incorrect_type.json",
                     manifest_errors::kInvalidNativelyConnectable);
}

TEST_F(NativelyConnectableManifestTest, IncorrectValuesType) {
  LoadAndExpectError("natively_connectable_incorrect_values_type.json",
                     manifest_errors::kInvalidNativelyConnectableValue);
}

TEST_F(NativelyConnectableManifestTest, EmptyHost) {
  LoadAndExpectError("natively_connectable_empty_host.json",
                     manifest_errors::kInvalidNativelyConnectableValue);
}

}  // namespace extensions
