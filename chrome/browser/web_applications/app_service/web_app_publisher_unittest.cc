// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/url_constants.h"
#include "net/base/url_util.h"
#endif

namespace web_app {

class WebAppPublisherTest : public testing::Test {
 public:
  // testing::Test implementation.
  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::string CreateWebApp(const GURL& app_url, const std::string& app_name) {
    // Create a web app entry with scope, which would be recognised
    // as normal web app in the web app system.
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->scope = app_url;

    std::string app_id =
        test::InstallWebApp(profile(), std::move(web_app_info));
    return app_id;
  }

  apps::AppServiceProxy* proxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  void InitializeWebAppPublisher() {
    apps::AppServiceTest app_service_test;
    app_service_test.SetUp(profile());
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

}  // namespace web_app
