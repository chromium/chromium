// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_test_util.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

const char AppListTestBase::kHostedAppId[] = "dceacbkfkmllgmjmbhgkpjegnodmildf";
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
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);
  InitializeInstalledExtensionService(pref_path, source_install_dir);
  service_->Init();

  ConfigureWebAppProvider();

  // Let any async services complete their set-up.
  base::RunLoop().RunUntilIdle();

  // There should be 4 extensions in the test profile.
  ASSERT_EQ(4U, registry()->enabled_extensions().size());
}

void AppListTestBase::ConfigureWebAppProvider() {
  Profile* const profile = testing_profile();

  auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
  url_loader_ = url_loader.get();

  auto externally_managed_app_manager =
      std::make_unique<web_app::ExternallyManagedAppManagerImpl>(profile);
  externally_managed_app_manager->SetUrlLoaderForTesting(std::move(url_loader));

  auto* const provider = web_app::FakeWebAppProvider::Get(profile);
  provider->SetExternallyManagedAppManager(
      std::move(externally_managed_app_manager));
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile);
}

// Test util functions----------------------------------------------------------

scoped_refptr<extensions::Extension> MakeApp(
    const std::string& name,
    const std::string& id,
    extensions::Extension::InitFromValueFlags flags) {
  std::string err;
  base::DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", "0.0");
  value.SetString("app.launch.web_url", "http://google.com");
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      flags, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

std::string CreateNextAppId(const std::string& app_id) {
  DCHECK(crx_file::id_util::IdIsValid(app_id));
  std::string next_app_id = app_id;
  size_t index = next_app_id.length() - 1;
  while (index > 0 && next_app_id[index] == 'p')
    next_app_id[index--] = 'a';
  DCHECK_NE(next_app_id[index], 'p');
  next_app_id[index]++;
  DCHECK(crx_file::id_util::IdIsValid(next_app_id));
  return next_app_id;
}
