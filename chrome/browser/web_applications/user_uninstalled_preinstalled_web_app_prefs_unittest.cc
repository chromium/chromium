// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/gurl.h"

namespace web_app {

using UserUninstalledPreinstalledWebAppPrefsUnitTest = WebAppTest;

TEST_F(UserUninstalledPreinstalledWebAppPrefsUnitTest, BasicOperations) {
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

}  // namespace web_app
