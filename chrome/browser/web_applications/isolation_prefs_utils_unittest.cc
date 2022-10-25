// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolation_prefs_utils.h"

#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace web_app {

namespace {
const AppId app_id = "test_app";
const std::string scope("https://example.com/pwa");
}  // namespace

class IsolationPrefsUtilsTest : public testing::Test {
 public:
  IsolationPrefsUtilsTest() {
    IsolationPrefsUtilsRegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(IsolationPrefsUtilsTest, TestInstallNonIsolatedWebApp) {
  WebApp app(app_id);
  app.SetScope(GURL(scope));

  RecordOrRemoveAppIsolationState(prefs(), app);

  const std::string* storage_isolation_key =
      GetStorageIsolationKey(prefs(), url::Origin::Create(app.scope()));
  EXPECT_EQ(storage_isolation_key, nullptr);
}

TEST_F(IsolationPrefsUtilsTest, TestInstallIsolatedWebApp) {
  WebApp app(app_id);
  app.SetScope(GURL(scope));
  app.SetStorageIsolated(true);

  RecordOrRemoveAppIsolationState(prefs(), app);

  const std::string* storage_isolation_key =
      GetStorageIsolationKey(prefs(), url::Origin::Create(app.scope()));
  EXPECT_NE(storage_isolation_key, nullptr);
  EXPECT_EQ(*storage_isolation_key, app_id);
}

TEST_F(IsolationPrefsUtilsTest, TestUpdateIsolatedWebApp) {
  WebApp app(app_id);
  app.SetScope(GURL(scope));
  app.SetStorageIsolated(true);

  {
    RecordOrRemoveAppIsolationState(prefs(), app);

    const std::string* storage_isolation_key =
        GetStorageIsolationKey(prefs(), url::Origin::Create(app.scope()));
    EXPECT_NE(storage_isolation_key, nullptr);
    EXPECT_EQ(*storage_isolation_key, app_id);
  }

  {
    app.SetStorageIsolated(false);
    RecordOrRemoveAppIsolationState(prefs(), app);

    const std::string* storage_isolation_key =
        GetStorageIsolationKey(prefs(), url::Origin::Create(app.scope()));
    EXPECT_EQ(storage_isolation_key, nullptr);
  }
}

TEST_F(IsolationPrefsUtilsTest, TestUninstallIsolatedWebApp) {
  WebApp app(app_id);
  app.SetScope(GURL(scope));
  app.SetStorageIsolated(true);

  {
    RecordOrRemoveAppIsolationState(prefs(), app);

    const std::string* storage_isolation_key =
        GetStorageIsolationKey(prefs(), url::Origin::Create(app.scope()));
    EXPECT_NE(storage_isolation_key, nullptr);
    EXPECT_EQ(*storage_isolation_key, app_id);
  }

  {
    RemoveAppIsolationState(prefs(), url::Origin::Create(app.scope()));

    const std::string* storage_isolation_key =
        GetStorageIsolationKey(prefs(), url::Origin::Create(app.scope()));
    EXPECT_EQ(storage_isolation_key, nullptr);
  }
}
}  // namespace web_app
