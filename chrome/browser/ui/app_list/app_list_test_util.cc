// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_test_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

const char AppListTestBase::kHostedAppId[] =
    "dceacbkfkmllgmjmbhgkpjegnodmildf";
const char AppListTestBase::kPackagedApp1Id[] =
    "emfkafnhnpcmabnnkckkchdilgeoekbo";
const char AppListTestBase::kPackagedApp2Id[] =
    "jlklkagmeajbjiobondfhiekepofmljl";

AppListTestBase::AppListTestBase() {}

AppListTestBase::~AppListTestBase() {}

void AppListTestBase::SetUp() {
  extensions::ExtensionServiceTestBase::SetUp();

  // Load "app_list" extensions test profile.
  // The test profile has 4 extensions:
  // - 1 dummy extension (which should not be visible in the launcher)
  // - 2 packaged extension apps
  // - 1 hosted extension app
  base::FilePath source_install_dir =
      data_dir().AppendASCII("app_list").AppendASCII("Extensions");
  base::FilePath pref_path = source_install_dir
      .DirName()
      .Append(chrome::kPreferencesFilename);
  InitializeInstalledExtensionService(pref_path, source_install_dir);
  service_->Init();

  // Let any async services complete their set-up.
  base::RunLoop().RunUntilIdle();

  // There should be 4 extensions in the test profile.
  ASSERT_EQ(4U, registry()->enabled_extensions().size());
}
