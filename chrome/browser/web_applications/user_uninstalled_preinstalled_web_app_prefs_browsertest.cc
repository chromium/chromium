// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

using UserUninstalledPreinstalledWebAppPrefsBrowserTest =
    WebAppControllerBrowserTest;

IN_PROC_BROWSER_TEST_F(UserUninstalledPreinstalledWebAppPrefsBrowserTest,
                       BasicOperations) {
  GURL url1("https://foo.com");
  GURL url2("https://bar1.com");
  GURL url3("https://bar2.com");
  AppId app_id1 = "foo";
  AppId app_id2 = "bar";

  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  preinstalled_prefs.Add(app_id1, {url1});
  preinstalled_prefs.Add(app_id2, {url2});
  // To test that url3 gets appended.
  preinstalled_prefs.Add(app_id2, {url3});

  // Basic checks to verify app id exists in preinstalled prefs or not.
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id2));
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist("baz"));

  // Basic checks to verify if install_urls exist in preinstalled prefs or not.
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(url1));
  EXPECT_EQ(app_id2, preinstalled_prefs.LookUpAppIdByInstallUrl(url2));
  EXPECT_EQ(app_id2, preinstalled_prefs.LookUpAppIdByInstallUrl(url3));
  EXPECT_NE(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(url3));
  EXPECT_EQ(absl::nullopt, preinstalled_prefs.LookUpAppIdByInstallUrl(GURL()));
  EXPECT_EQ(absl::nullopt, preinstalled_prefs.LookUpAppIdByInstallUrl(
                               GURL("https://baz.com")));
}

IN_PROC_BROWSER_TEST_F(UserUninstalledPreinstalledWebAppPrefsBrowserTest,
                       RemovalOperations) {
  GURL url1("https://foo.com");
  GURL url2("https://bar1.com");
  GURL url3("https://bar2.com");
  AppId app_id1 = "foo";
  AppId app_id2 = "bar";
  AppId app_id3 = "baz";

  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  preinstalled_prefs.Add(app_id1, {url1});
  preinstalled_prefs.Add(app_id2, {url2, url3});

  // Should be false because app_id3 does not exist in the prefs.
  // 2nd case should be false because random URL does not exist
  // in the prefs.
  EXPECT_FALSE(preinstalled_prefs.RemoveByInstallUrl(app_id3, url1));
  EXPECT_FALSE(preinstalled_prefs.RemoveByInstallUrl(
      app_id3, GURL("https://random_url.com")));

  // Should be true because url2 exists for app_id2 and url1 exists
  // for app_id1.
  EXPECT_TRUE(preinstalled_prefs.RemoveByInstallUrl(app_id2, url2));
  EXPECT_TRUE(preinstalled_prefs.RemoveByInstallUrl(app_id1, url1));

  // The only remaining left pref should be app_id2 -> {url3}
  EXPECT_EQ(app_id2, preinstalled_prefs.LookUpAppIdByInstallUrl(url3));
  // url1 has been deleted, so app_id1 has been wiped from the prefs, and url2
  // has been plain deleted.
  EXPECT_EQ(absl::nullopt, preinstalled_prefs.LookUpAppIdByInstallUrl(url1));
  EXPECT_EQ(absl::nullopt, preinstalled_prefs.LookUpAppIdByInstallUrl(url2));
}

IN_PROC_BROWSER_TEST_F(UserUninstalledPreinstalledWebAppPrefsBrowserTest,
                       PrefsPropagateProperlyOnDefaultUninstall) {
  auto app_info1 = std::make_unique<WebAppInstallInfo>();
  app_info1->start_url = GURL("https://example_url1.com/");
  app_info1->title = u"Example App1";
  app_info1->install_url = GURL("https://example_url1.com/install");
  auto app_info2 = std::make_unique<WebAppInstallInfo>();
  app_info2->start_url = GURL("https://example_url2.com/");
  app_info2->title = u"Example App2";
  app_info2->install_url = GURL("https://example_url2.com/install");
  AppId app_id1 =
      test::InstallWebApp(profile(), std::move(app_info1),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::EXTERNAL_DEFAULT);
  AppId app_id2 =
      test::InstallWebApp(profile(), std::move(app_info2),
                          /*overwrite_existing_manifest_fields=*/false,
                          webapps::WebappInstallSource::ARC);

  // Verify that the prefs aren't filled just on installation.
  UserUninstalledPreinstalledWebAppPrefs preinstalled_prefs(
      profile()->GetPrefs());
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id2));

  // Verify that the prefs are filled on uninstall for app_id1. For app_id2,
  // this will not happen because external apps installed by sources that are
  // not preinstalled are not added to this pref.
  test::UninstallWebApp(profile(), app_id1);
  test::UninstallWebApp(profile(), app_id2);
  EXPECT_TRUE(preinstalled_prefs.DoesAppIdExist(app_id1));
  EXPECT_EQ(app_id1, preinstalled_prefs.LookUpAppIdByInstallUrl(
                         GURL("https://example_url1.com/install")));
  EXPECT_FALSE(preinstalled_prefs.DoesAppIdExist(app_id2));
}

}  // namespace web_app
