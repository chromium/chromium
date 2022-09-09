// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "base/command_line.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/app_isolation_info.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;

class IsolatedAppsManifestTest : public ChromeManifestTest {
};

TEST_F(IsolatedAppsManifestTest, IsolatedApps) {
  LoadAndExpectWarning(
      "isolated_app_valid.json",
      "'experimental' requires the 'experimental-extension-apis' "
      "command line switch to be enabled.");

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("isolated_app_valid.json"));
  EXPECT_TRUE(AppIsolationInfo::HasIsolatedStorage(extension2.get()));
}

}  // namespace extensions
