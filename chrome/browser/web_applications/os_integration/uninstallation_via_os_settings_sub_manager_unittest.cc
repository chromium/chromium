// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class UninstallationViaOsSettingsSubManagerTest : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  UninstallationViaOsSettingsSubManagerTest() = default;
  ~UninstallationViaOsSettingsSubManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    test::UninstallAllWebApps(profile());
    WebAppTest::TearDown();
  }

  webapps::AppId InstallWebApp(webapps::WebappInstallSource install_source) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    auto source = install_source;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true, source,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
};

bool IsOsUninstallationSupported() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

TEST_F(UninstallationViaOsSettingsSubManagerTest, TestUserUninstallable) {
  base::HistogramTester histogram_tester;
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  std::vector<base::Bucket> samples = histogram_tester.GetAllSamples(
      "WebApp.OsSettingsUninstallRegistration.Result");
#if BUILDFLAG(IS_WIN)
  EXPECT_THAT(samples, base::BucketsAre(base::Bucket(/*min=*/1, 1)));
#else
  EXPECT_THAT(samples, testing::IsEmpty());
#endif  // BUILDFLAG(IS_WIN)

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    EXPECT_EQ(
        IsOsUninstallationSupported(),
        os_integration_state.uninstall_registration().registered_with_os());
    base::expected<bool, std::string> result =
        fake_os_integration().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                          profile());
#if BUILDFLAG(IS_WIN)
    EXPECT_THAT(result, base::test::ValueIs(true));
#else
    EXPECT_FALSE(result.has_value());
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(UninstallationViaOsSettingsSubManagerTest, TestNotUserUninstallable) {
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::EXTERNAL_POLICY);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    EXPECT_FALSE(
        os_integration_state.uninstall_registration().registered_with_os());
  if (IsOsUninstallationSupported()) {
    ASSERT_FALSE(
        os_integration_state.uninstall_registration().registered_with_os());
  }
  base::expected<bool, std::string> result =
      fake_os_integration().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                        profile());
#if BUILDFLAG(IS_WIN)
  EXPECT_THAT(result, base::test::ValueIs(false));
#else
  EXPECT_FALSE(result.has_value());
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(UninstallationViaOsSettingsSubManagerTest, UninstallApp) {
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  base::HistogramTester histogram_tester;
  test::UninstallAllWebApps(profile());
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
  base::expected<bool, std::string> install_result =
      fake_os_integration().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                        profile());
  std::vector<base::Bucket> samples = histogram_tester.GetAllSamples(
      "WebApp.OsSettingsUninstallUnregistration.Result");
#if BUILDFLAG(IS_WIN)
  EXPECT_THAT(samples, base::BucketsAre(base::Bucket(/*min=*/1, 1)));
  EXPECT_THAT(install_result, base::test::ValueIs(false));
#else
  EXPECT_THAT(samples, testing::IsEmpty());
  EXPECT_FALSE(install_result.has_value());
#endif  // BUILDFLAG(IS_WIN)
}

// Testing crbug.com/1434577, that OS states can be cleaned up even after
// the app has been uninstalled.
TEST_F(UninstallationViaOsSettingsSubManagerTest,
       OsStatesCleanupAfterAppUninstallation) {
  base::HistogramTester histogram_tester;
  const webapps::AppId& app_id =
      InstallWebApp(webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);

  std::vector<base::Bucket> samples = histogram_tester.GetAllSamples(
      "WebApp.OsSettingsUninstallRegistration.Result");
#if BUILDFLAG(IS_WIN)
  EXPECT_THAT(samples, base::BucketsAre(base::Bucket(/*min=*/1, 1)));
#else
  EXPECT_THAT(samples, testing::IsEmpty());
#endif  // BUILDFLAG(IS_WIN)

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  EXPECT_EQ(IsOsUninstallationSupported(),
            os_integration_state.uninstall_registration().registered_with_os());
  base::expected<bool, std::string> install_result =
      fake_os_integration().IsUninstallRegisteredWithOs(app_id, "Test App",
                                                        profile());
  std::vector<base::Bucket> unregistration_samples =
      histogram_tester.GetAllSamples(
          "WebApp.OsSettingsUninstallUnregistration.Result");
#if BUILDFLAG(IS_WIN)
  EXPECT_THAT(unregistration_samples, testing::IsEmpty());
  EXPECT_THAT(install_result, base::test::ValueIs(true));
#else
  EXPECT_THAT(unregistration_samples, testing::IsEmpty());
  EXPECT_FALSE(install_result.has_value());
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

}  // namespace web_app
