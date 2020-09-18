// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_finalizer.h"

#include <memory>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

void InitializeEmptyExtensionService(Profile* profile) {
  // CrxInstaller in BookmarkAppInstallFinalizer needs an ExtensionService, so
  // create one for the profile.
  auto* test_system = static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile));
  test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                      profile->GetPath(),
                                      false /* autoupdate_enabled */);
}

struct FinalizeInstallResult {
  AppId installed_app_id;
  InstallResultCode code;
};

}  // namespace

// Tests both implementations of InstallFinalizer to ensure same behavior with
// and without BMO enabled.
// TODO(crbug.com/1068081): Migrate remaining tests from
// bookmark_app_install_finalizer_unittest.
class InstallFinalizerUnitTest
    : public WebAppTest,
      public ::testing::WithParamInterface<ProviderType> {
 public:
  InstallFinalizerUnitTest() {
    switch (GetParam()) {
      case ProviderType::kWebApps:
        scoped_feature_list_.InitAndEnableFeature(
            features::kDesktopPWAsWithoutExtensions);
        break;
      case ProviderType::kBookmarkApps:
        scoped_feature_list_.InitAndDisableFeature(
            features::kDesktopPWAsWithoutExtensions);
        break;
    }
  }
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

    switch (GetParam()) {
      case ProviderType::kWebApps:
        finalizer_ = std::make_unique<WebAppInstallFinalizer>(
            profile(), icon_manager_.get(), /*legacy_finalizer=*/nullptr);
        break;
      case ProviderType::kBookmarkApps:
        InitializeEmptyExtensionService(profile());
        finalizer_ = std::make_unique<extensions::BookmarkAppInstallFinalizer>(
            profile());
        break;
    }

    finalizer_->SetSubsystems(&registrar(), ui_manager_.get(),
                              &test_registry_controller_->sync_bridge());
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
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<InstallFinalizer> finalizer_;

  DISALLOW_COPY_AND_ASSIGN(InstallFinalizerUnitTest);
};

TEST_P(InstallFinalizerUnitTest, BasicInstallSucceeds) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = base::ASCIIToUTF16("Foo Title");
  InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_FALSE(result.installed_app_id.empty());
}

TEST_P(InstallFinalizerUnitTest, InstallStoresLatestWebAppInstallSource) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL("https://foo.example");
  info->title = base::ASCIIToUTF16("Foo Title");
  InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::INTERNAL_DEFAULT;

  FinalizeInstallResult result = AwaitFinalizeInstall(*info, options);

  base::Optional<int> install_source =
      GetIntWebAppPref(profile()->GetPrefs(), result.installed_app_id,
                       kLatestWebAppInstallSource);
  EXPECT_TRUE(install_source.has_value());
  EXPECT_EQ(static_cast<WebappInstallSource>(*install_source),
            WebappInstallSource::INTERNAL_DEFAULT);
}

INSTANTIATE_TEST_SUITE_P(All,
                         InstallFinalizerUnitTest,
                         ::testing::ValuesIn({ProviderType::kBookmarkApps,
                                              ProviderType::kWebApps}));

}  // namespace web_app
