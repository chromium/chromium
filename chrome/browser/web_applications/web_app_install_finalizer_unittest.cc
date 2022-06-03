// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

struct FinalizeInstallResult {
  AppId installed_app_id;
  InstallResultCode code;
};

}  // namespace

class WebAppInstallFinalizerUnitTest : public WebAppTest {
 public:
  WebAppInstallFinalizerUnitTest() = default;
  WebAppInstallFinalizerUnitTest(const WebAppInstallFinalizerUnitTest&) =
      delete;
  WebAppInstallFinalizerUnitTest& operator=(
      const WebAppInstallFinalizerUnitTest&) = delete;
  ~WebAppInstallFinalizerUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());
    icon_manager_ = std::make_unique<WebAppIconManager>(
        profile(), registrar(), base::MakeRefCounted<FileUtilsWrapper>());
    policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());
    ui_manager_ = std::make_unique<FakeWebAppUiManager>();
    finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), icon_manager_.get(), policy_manager_.get());

    finalizer_->SetSubsystems(
        &registrar(), ui_manager_.get(),
        &fake_registry_controller_->sync_bridge(),
        &fake_registry_controller_->os_integration_manager());
    fake_registry_controller_->Init();
    finalizer_->Start();
  }

  void TearDown() override {
    finalizer_.reset();
    ui_manager_.reset();
    policy_manager_.reset();
    icon_manager_.reset();
    fake_registry_controller_.reset();
    WebAppTest::TearDown();
  }

  // Synchronous version of FinalizeInstall.
  FinalizeInstallResult AwaitFinalizeInstall(
      WebApplicationInfo info,
      WebAppInstallFinalizer::FinalizeOptions options) {
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

  WebAppInstallFinalizer& finalizer() { return *finalizer_.get(); }
  WebAppRegistrar& registrar() {
    return fake_registry_controller_->registrar();
  }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppPolicyManager> policy_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<WebAppInstallFinalizer> finalizer_;
};

TEST_F(WebAppInstallFinalizerUnitTest, BasicInstallSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(result.installed_app_id,
            GenerateAppId(/*manifest_id=*/absl::nullopt, info->start_url));
}

TEST_F(WebAppInstallFinalizerUnitTest, ConcurrentInstallSucceeds) {
  auto info1 = std::make_unique<WebApplicationInfo>();
  info1->start_url = GURL("https://foo1.example");
  info1->title = u"Foo1 Title";

  auto info2 = std::make_unique<WebApplicationInfo>();
  info2->start_url = GURL("https://foo2.example");
  info2->title = u"Foo2 Title";

  WebAppInstallFinalizer::FinalizeOptions options;
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
          EXPECT_EQ(
              installed_app_id,
              GenerateAppId(/*manifest_id=*/absl::nullopt, info1->start_url));
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
          EXPECT_EQ(
              installed_app_id,
              GenerateAppId(/*manifest_id=*/absl::nullopt, info2->start_url));
          callback2_called = true;
          if (callback1_called)
            run_loop.Quit();
        }));
  }

  run_loop.Run();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}

TEST_F(WebAppInstallFinalizerUnitTest, InstallStoresLatestWebAppInstallSource) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = u"Foo Title";
  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = webapps::WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  absl::optional<int> install_source =
      GetWebAppInstallSource(profile()->GetPrefs(), result.installed_app_id);
  EXPECT_TRUE(install_source.has_value());
  EXPECT_EQ(static_cast<webapps::WebappInstallSource>(*install_source),
            webapps::WebappInstallSource::INTERNAL_DEFAULT);
}

}  // namespace web_app
