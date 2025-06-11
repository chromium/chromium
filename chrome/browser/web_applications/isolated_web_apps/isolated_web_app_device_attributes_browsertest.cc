// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/test/regular_logged_in_browser_test_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/policy/device_policy/cached_device_policy_updater.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::HasSubstr;

constexpr std::array<const char*, 5> kDeviceAttributeNames = {
    "AnnotatedAssetId", "AnnotatedLocation", "DirectoryId", "Hostname",
    "SerialNumber"};

content::EvalJsResult CallDeviceAttributesApi(
    const content::ToRenderFrameHost& frame,
    const std::string& attribute_name) {
  return content::EvalJs(
      frame, base::ReplaceStringPlaceholders("navigator.managed.get$1()",
                                             {attribute_name}, nullptr));
}

constexpr char kDeviceAnnotatedAssetId[] = "iwa_test_asset_id";
constexpr char kDeviceAnnotatedLocation[] = "iwa_test_location";
constexpr char kDeviceDirectoryApiId[] = "iwa_test_directory_id";
constexpr char kDeviceHostname[] = "iwa_test_hostname";
constexpr char kDeviceSerialNumber[] = "iwa_test_serial_number";

constexpr std::array<const char*, 5> kExpectedDeviceAttributeValues = {
    kDeviceAnnotatedAssetId, kDeviceAnnotatedLocation, kDeviceDirectoryApiId,
    kDeviceHostname, kDeviceSerialNumber};

constexpr char kManagedUserEmail[] = "example@example.com";
constexpr GaiaId::Literal kGaiaId("123456");
constexpr char kTestAffiliationId[] = "test-affiliation-id";

constexpr char kPermissionsPolicyError[] =
    "Permissions-Policy: device-attributes are disabled.";

constexpr char kAdminPolicyError[] =
    "The current origin cannot use this web API because it is not allowed by "
    "the DeviceAttributesAllowedForOrigins policy.";

constexpr char kChildFrameError[] =
    "This API is allowed only in top level frames.";

}  // namespace

class IsolatedWebAppDeviceAttributesBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  IsolatedWebAppDeviceAttributesBrowserTest() {
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
    if (IsFeatureFlagEnabled()) {
      features_.InitAndEnableFeature(
          blink::features::kDeviceAttributesPermissionPolicy);
    } else {
      features_.InitAndDisableFeature(
          blink::features::kDeviceAttributesPermissionPolicy);
    }
  }
  void SetUpInProcessBrowserTestFixture() override {
    IsolatedWebAppBrowserTestHarness::SetUpInProcessBrowserTestFixture();
    SetUpPolicies();
  }

 protected:
  bool IsFeatureFlagEnabled() { return std::get<0>(GetParam()); }
  bool IsPermissionsPolicyGranted() { return std::get<1>(GetParam()); }
  bool IsAdminPolicyAllowed() { return std::get<2>(GetParam()); }

  void AllowDeviceAttributesForOrigin(const std::string& origin) {
    profile()->GetPrefs()->SetList(prefs::kDeviceAttributesAllowedForOrigins,
                                   base::Value::List().Append(origin));
  }

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);

    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kDeviceSerialNumber);
  }

  IsolatedWebAppUrlInfo InstallApp(
      bool device_attributes_permissions_policy_enabled) {
    auto web_bundle_id = web_package::test::GetDefaultEd25519WebBundleId();
    auto iwa_url_info =
        web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
            web_bundle_id);

    web_app::WebAppTestInstallObserver observer(profile());
    observer.BeginListening({iwa_url_info.app_id()});

    auto manifest_builder = ManifestBuilder();
    if (device_attributes_permissions_policy_enabled) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kDeviceAttributes, true,
          {});
    }
    isolated_web_app_update_server_mixin_.AddBundle(
        IsolatedWebAppBuilder(manifest_builder)
            .BuildBundle(web_bundle_id,
                         {web_package::test::GetDefaultEd25519KeyPair()}));
    web_app::test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        isolated_web_app_update_server_mixin_.CreateForceInstallPolicyEntry(
            web_bundle_id));

    EXPECT_EQ(iwa_url_info.app_id(), observer.Wait());
    return iwa_url_info;
  }

 private:
  void SetUpPolicies() {
    {
      policy::CachedDevicePolicyUpdater updater;
      updater.policy_data().set_annotated_asset_id(kDeviceAnnotatedAssetId);
      updater.policy_data().set_annotated_location(kDeviceAnnotatedLocation);
      updater.policy_data().set_directory_api_id(kDeviceDirectoryApiId);
      updater.payload()
          .mutable_network_hostname()
          ->set_device_hostname_template(kDeviceHostname);
      updater.policy_data().add_device_affiliation_ids(kTestAffiliationId);
      updater.Commit();
    }

    // Mark as affiliated.
    {
      auto updater = user_policy_.RequestPolicyUpdate();
      updater->policy_data()->add_user_affiliation_ids(kTestAffiliationId);
    }
  }
  policy::DevicePolicyBuilder& device_policy() {
    return *(policy_helper_.device_policy());
  }

  base::test::ScopedFeatureList features_;
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::UserPolicyMixin user_policy_{
      &mixin_host_, AccountId::FromUserEmailGaiaId(kManagedUserEmail, kGaiaId)};
  ash::RegularLoggedInBrowserTestMixin logged_in_{
      &mixin_host_, AccountId::FromUserEmailGaiaId(kManagedUserEmail, kGaiaId)};
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  web_app::IsolatedWebAppUpdateServerMixin
      isolated_web_app_update_server_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDeviceAttributesBrowserTest,
                       ObtainingDeviceAttributes) {
  IsolatedWebAppUrlInfo url_info = InstallApp(IsPermissionsPolicyGranted());
  if (IsAdminPolicyAllowed()) {
    AllowDeviceAttributesForOrigin(url_info.origin().Serialize());
  }
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_NE(app_frame, nullptr);
  ASSERT_EQ(kDeviceAttributeNames.size(),
            kExpectedDeviceAttributeValues.size());
  bool device_attributes_should_work = IsFeatureFlagEnabled()
                                           ? IsPermissionsPolicyGranted()
                                           : IsAdminPolicyAllowed();
  for (size_t i = 0; i < kDeviceAttributeNames.size(); ++i) {
    if (device_attributes_should_work) {
      EXPECT_EQ(kExpectedDeviceAttributeValues[i],
                CallDeviceAttributesApi(app_frame, kDeviceAttributeNames[i]));
    } else {
      EXPECT_THAT(
          CallDeviceAttributesApi(app_frame, kDeviceAttributeNames[i]).error,
          HasSubstr(IsFeatureFlagEnabled() ? kPermissionsPolicyError
                                           : kAdminPolicyError));
    }
  }
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDeviceAttributesBrowserTest,
                       ObtainingDeviceAttributesFromChildFrame) {
  IsolatedWebAppUrlInfo url_info = InstallApp(IsPermissionsPolicyGranted());
  if (IsAdminPolicyAllowed()) {
    AllowDeviceAttributesForOrigin(url_info.origin().Serialize());
  }
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_NE(app_frame, nullptr);

  ASSERT_TRUE(ExecJs(app_frame, R"(
      const noopPolicy = trustedTypes.createPolicy("policy", {
        createHTML: (string) => string,
      });
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.srcdoc = noopPolicy.createHTML('<p>Child frame</p>');
        f.addEventListener('load', resolve);
        document.body.appendChild(f);
      });
  )"));
  content::RenderFrameHost* iframe = ChildFrameAt(app_frame, 0);
  ASSERT_NE(iframe, nullptr);

  for (const std::string& attribute_name : kDeviceAttributeNames) {
    EXPECT_THAT(CallDeviceAttributesApi(iframe, attribute_name).error,
                HasSubstr(kChildFrameError));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppDeviceAttributesBrowserTest,
    ::testing::Combine(::testing::Bool(),  // feature flag
                       ::testing::Bool(),  // permissions policy
                       ::testing::Bool()   // admin policy
                       ),
    [](const ::testing::TestParamInfo<std::tuple<bool, bool, bool>>& info) {
      // Generate a descriptive name for each test case.
      return base::StringPrintf(
          "FeatureFlag%s_PermissionsPolicy%s_AdminPolicy%s",
          std::get<0>(info.param) ? "Enabled" : "Disabled",
          std::get<1>(info.param) ? "Granted" : "Denied",
          std::get<2>(info.param) ? "Allowed" : "Denied");
    });
}  // namespace web_app
