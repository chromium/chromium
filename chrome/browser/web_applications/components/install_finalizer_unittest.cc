// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_finalizer.h"

#include <memory>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

struct FinalizeInstallResult {
  AppId installed_app_id;
  InstallResultCode code;
};

}  // namespace

// TODO(crbug.com/1068081): Migrate remaining tests from
// bookmark_app_install_finalizer_unittest.
class InstallFinalizerUnitTest : public WebAppTest {
 public:
  InstallFinalizerUnitTest() = default;
  InstallFinalizerUnitTest(const InstallFinalizerUnitTest&) = delete;
  InstallFinalizerUnitTest& operator=(const InstallFinalizerUnitTest&) = delete;
  ~InstallFinalizerUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();
    test_registry_controller_->SetUp(profile());
    auto file_utils = std::make_unique<TestFileUtils>();
    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), registrar(),
                                                        std::move(file_utils));
    ui_manager_ = std::make_unique<TestWebAppUiManager>();
    finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), icon_manager_.get(), /*legacy_finalizer=*/nullptr);

    finalizer_->SetSubsystems(
        &registrar(), ui_manager_.get(),
        &test_registry_controller_->sync_bridge(),
        &test_registry_controller_->os_integration_manager());
    test_registry_controller_->Init();
    finalizer_->Start();
  }

  void TearDown() override {
    finalizer_.reset();
    ui_manager_.reset();
    icon_manager_.reset();
    test_registry_controller_.reset();
    WebAppTest::TearDown();
  }

  // Synchronous version of FinalizeInstall.
  FinalizeInstallResult AwaitFinalizeInstall(
      WebApplicationInfo info,
      InstallFinalizer::FinalizeOptions options) {
    FinalizeInstallResult result{};
    base::RunLoop run_loop;
    finalizer().FinalizeInstall(
        info, options,
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              result.installed_app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  InstallFinalizer& finalizer() { return *finalizer_.get(); }
  WebAppRegistrar& registrar() {
    return test_registry_controller_->registrar();
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<InstallFinalizer> finalizer_;
};

TEST_F(InstallFinalizerUnitTest, BasicInstallSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  InstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id, GenerateAppIdFromURL(info->start_url));
}

TEST_F(InstallFinalizerUnitTest, ConcurrentInstallSucceeds) {
  auto info1 = std::make_unique<WebApplicationInfo>();
  info1->start_url = GURL("https://foo1.example");
  info1->title = u"Foo1 Title";

  auto info2 = std::make_unique<WebApplicationInfo>();
  info2->start_url = GURL("https://foo2.example");
  info2->title = u"Foo2 Title";

  InstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  base::RunLoop run_loop;
  bool callback1_called = false;
  bool callback2_called = false;

  // Start install finalization for the 1st app.
  {
    finalizer().FinalizeInstall(
        *info1, options,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       InstallResultCode code) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(installed_app_id, GenerateAppIdFromURL(info1->start_url));
          callback1_called = true;
          if (callback2_called)
            run_loop.Quit();
        }));
  }

  // Start install finalization for the 2nd app.
  {
    finalizer().FinalizeInstall(
        *info2, options,
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       InstallResultCode code) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, code);
          EXPECT_EQ(installed_app_id, GenerateAppIdFromURL(info2->start_url));
          callback2_called = true;
          if (callback1_called)
            run_loop.Quit();
        }));
  }

  run_loop.Run();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}

TEST_F(InstallFinalizerUnitTest, InstallStoresLatestWebAppInstallSource) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  InstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  base::Optional<int> install_source =
      GetIntWebAppPref(profile()->GetPrefs(), result.installed_app_id,
                       kLatestWebAppInstallSource);
  EXPECT_TRUE(install_source.has_value());
  EXPECT_EQ(static_cast<webapps::WebappInstallSource>(*install_source),
            webapps::WebappInstallSource::INTERNAL_DEFAULT);
}

}  // namespace web_app
