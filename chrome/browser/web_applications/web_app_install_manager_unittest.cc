// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

class WebAppInstallManagerTest : public WebAppTest {};

TEST_F(WebAppInstallManagerTest, InstallFromWebContents) {
  EXPECT_EQ(true, AllowWebAppInstallation(profile()));

  auto registrar = std::make_unique<WebAppRegistrar>();
  auto manager =
      std::make_unique<WebAppInstallManager>(profile(), registrar.get());

  auto web_app_info = std::make_unique<WebApplicationInfo>();

  const GURL url = GURL("https://example.com/path");
  const std::string name = "Name";
  const std::string description = "Description";

  const AppId app_id = GenerateAppIdFromURL(url);

  web_app_info->app_url = url;
  web_app_info->title = base::UTF8ToUTF16(name);
  web_app_info->description = base::UTF8ToUTF16(description);
  manager->SetDataRetrieverForTesting(
      std::make_unique<TestDataRetriever>(std::move(web_app_info)));

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  manager->InstallWebApp(
      web_contents(), force_shortcut_app,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kSuccess, code);
            EXPECT_EQ(app_id, installed_app_id);
            callback_called = true;
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, run_loop.QuitClosure());
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  WebApp* web_app = registrar->GetAppById(app_id);
  EXPECT_NE(nullptr, web_app);

  EXPECT_EQ(app_id, web_app->app_id());
  EXPECT_EQ(name, web_app->name());
  EXPECT_EQ(description, web_app->description());
  EXPECT_EQ(url.spec(), web_app->launch_url());
}

TEST_F(WebAppInstallManagerTest, GetWebApplicationInfoFailed) {
  auto registrar = std::make_unique<WebAppRegistrar>();
  auto manager =
      std::make_unique<WebAppInstallManager>(profile(), registrar.get());

  manager->SetDataRetrieverForTesting(std::make_unique<TestDataRetriever>(
      std::unique_ptr<WebApplicationInfo>()));

  base::RunLoop run_loop;
  bool callback_called = false;
  const bool force_shortcut_app = false;

  manager->InstallWebApp(
      web_contents(), force_shortcut_app,
      base::BindLambdaForTesting(
          [&](const AppId& installed_app_id, InstallResultCode code) {
            EXPECT_EQ(InstallResultCode::kGetWebApplicationInfoFailed, code);
            EXPECT_EQ(AppId(), installed_app_id);
            callback_called = true;
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, run_loop.QuitClosure());
          }));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
}

}  // namespace web_app
