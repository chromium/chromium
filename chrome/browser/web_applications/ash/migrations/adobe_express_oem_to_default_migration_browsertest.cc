// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/ash/migrations/adobe_express_oem_to_default_migration.h"

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app::migrations {
namespace {

class AdobeExpressOemToDefaultMigrationTest : public InProcessBrowserTest {
 public:
  Profile* profile() { return browser()->profile(); }
};

// Installs Adobe Express as an OEM installed app.
IN_PROC_BROWSER_TEST_F(AdobeExpressOemToDefaultMigrationTest,
                       PRE_MigrateOemInstall) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://new.express.adobe.com/"));

  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_info),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::PRELOADED_OEM);
  ASSERT_EQ(app_id, kAdobeExpressAppId);

  auto* provider = WebAppProvider::GetForWebApps(profile());
  ASSERT_TRUE(provider->registrar_unsafe().GetAppById(app_id)->HasOnlySource(
      WebAppManagement::Type::kOem));
}

// Verifies that on next startup of the WebAppProvider, the OEM installed app is
// migrated to be Default installed.
IN_PROC_BROWSER_TEST_F(AdobeExpressOemToDefaultMigrationTest,
                       MigrateOemInstall) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  ASSERT_TRUE(provider->registrar_unsafe()
                  .GetAppById(kAdobeExpressAppId)
                  ->HasOnlySource(WebAppManagement::Type::kApsDefault));
}

// Installs Adobe Express as a user installed app.
IN_PROC_BROWSER_TEST_F(AdobeExpressOemToDefaultMigrationTest,
                       PRE_DoNotMigrateUserInstall) {
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://new.express.adobe.com/"));

  webapps::AppId app_id =
      test::InstallWebApp(profile(), std::move(web_app_info),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
  ASSERT_EQ(app_id, kAdobeExpressAppId);

  auto* provider = WebAppProvider::GetForWebApps(profile());
  ASSERT_EQ(provider->registrar_unsafe().GetAppById(app_id)->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kUserInstalled,
                                   WebAppManagement::Type::kSync}));
}

// Verifies that the user-installed app is not changed on next startup.
IN_PROC_BROWSER_TEST_F(AdobeExpressOemToDefaultMigrationTest,
                       DoNotMigrateUserInstall) {
  auto* provider = WebAppProvider::GetForWebApps(profile());
  ASSERT_EQ(
      provider->registrar_unsafe().GetAppById(kAdobeExpressAppId)->GetSources(),
      WebAppManagementTypes({WebAppManagement::Type::kUserInstalled,
                             WebAppManagement::Type::kSync}));
}

}  // namespace
}  // namespace web_app::migrations
