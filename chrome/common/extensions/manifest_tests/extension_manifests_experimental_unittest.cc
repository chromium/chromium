// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "base/command_line.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

TEST_F(ChromeManifestTest, ExperimentalPermission) {
  LoadAndExpectWarning(
      "experimental.json",
      "'experimental' requires the 'experimental-extension-apis' "
      "command line switch to be enabled.");
  LoadAndExpectSuccess("experimental.json",
                       extensions::mojom::ManifestLocation::kComponent);
  LoadAndExpectSuccess("experimental.json",
                       extensions::mojom::ManifestLocation::kInternal,
                       extensions::Extension::FROM_WEBSTORE);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);
  LoadAndExpectSuccess("experimental.json");
}
