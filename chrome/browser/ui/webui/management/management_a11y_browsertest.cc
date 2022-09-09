// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_a11y_browsertest.h"

#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"

ManagementA11yUIBrowserTest::ManagementA11yUIBrowserTest()
    : test_data_dir_(base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
                         .AppendASCII("extensions")) {}

ManagementA11yUIBrowserTest::~ManagementA11yUIBrowserTest() {}

void ManagementA11yUIBrowserTest::InstallPowerfulPolicyEnforcedExtension() {
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  loader.set_ignore_manifest_warnings(true);
  loader.set_grant_permissions(true);
  loader.set_location(
      extensions::mojom::ManifestLocation::kExternalPolicyDownload);
  loader.LoadExtension(test_data_dir_.AppendASCII("good.crx"));
}
