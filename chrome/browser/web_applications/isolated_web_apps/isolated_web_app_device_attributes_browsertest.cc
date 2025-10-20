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
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/policy/device_policy/cached_device_policy_updater.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
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

constexpr char kNoDeviceAttributesPermissionErrorMessage[] =
    "The current origin cannot use this web API because it was not granted the "
    "'device-attributes' permission.";

constexpr char kChildFrameError[] =
    "This API is allowed only in top level frames.";

struct IsolatedWebAppDeviceAttributesBrowserTestParams {
  using TupleT = std::tuple<bool, bool, bool, bool>;
  bool feature_flag;
  bool allow_policy;
  bool block_policy;
  bool permissions_policy;
  explicit IsolatedWebAppDeviceAttributesBrowserTestParams(TupleT t)
      : feature_flag(std::get<0>(t)),
        allow_policy(std::get<1>(t)),
        block_policy(std::get<2>(t)),
        permissions_policy(std::get<3>(t)) {}
};

}  // namespace

class IsolatedWebAppDeviceAttributesBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<
          IsolatedWebAppDeviceAttributesBrowserTestParams> {
 public:
  IsolatedWebAppDeviceAttributesBrowserTest() {
    InitFeatureList();
    fake_statistics_provider_.SetVpdStatus(
        ash::system::StatisticsProvider::VpdStatus::kValid);
  }
  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    IsolatedWebAppBrowserTestHarness::SetUpInProcessBrowserTestFixture();
    SetUpPolicies();
  }

 protected:
  bool IsDeviceAttributesPermissionPolicyFeatureFlagEnabled() {
    return GetParam().feature_flag;
  }
  bool IsAllowPolicySet() { return GetParam().allow_policy; }
  bool IsBlockPolicySet() { return GetParam().block_policy; }
  bool IsPermissionsPolicyGranted() { return GetParam().permissions_policy; }

  void MaybeSetEnterprisePoliciesForOrigin(const std::string& origin) {
    if (!IsAllowPolicySet() && !IsBlockPolicySet()) {
      return;
    }
    policy::PolicyMap policies;
    if (IsBlockPolicySet()) {
      policies.Set(policy::key::kDeviceAttributesBlockedForOrigins,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(base::Value::List().Append(origin)), nullptr);
    }
    if (IsAllowPolicySet()) {
      policies.Set(policy::key::kDeviceAttributesAllowedForOrigins,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(base::Value::List().Append(origin)), nullptr);
    }
    policy_provider_.UpdateChromePolicy(policies);
  }

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(true);
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);

    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kDeviceSerialNumber);
  }

  void TearDownOnMainThread() override {
    IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(false);
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  IsolatedWebAppUrlInfo InstallApp() {
    auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
    auto iwa_url_info =
        web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
            web_bundle_id);

    web_app::WebAppTestInstallObserver observer(profile());
    observer.BeginListening({iwa_url_info.app_id()});

    isolated_web_app_iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(GetIwaManifestBuilder())
            .BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair()}));
    web_app::test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        isolated_web_app_iwa_test_update_server_.CreateForceInstallPolicyEntry(
            web_bundle_id));

    EXPECT_EQ(iwa_url_info.app_id(), observer.Wait());
    return iwa_url_info;
  }

 private:
  void InitFeatureList() {
    if (IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()) {
      features_.InitAndEnableFeature(
          blink::features::kDeviceAttributesPermissionPolicy);
    } else {
      features_.InitAndDisableFeature(
          blink::features::kDeviceAttributesPermissionPolicy);
    }
  }

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

  web_app::ManifestBuilder GetIwaManifestBuilder() {
    auto manifest_builder = ManifestBuilder();
    if (IsPermissionsPolicyGranted()) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kDeviceAttributes, true,
          {});
    }
    return manifest_builder;
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
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  web_app::IsolatedWebAppTestUpdateServer
      isolated_web_app_iwa_test_update_server_;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDeviceAttributesBrowserTest,
                       ObtainingDeviceAttributes) {
  IsolatedWebAppUrlInfo url_info = InstallApp();
  const bool device_attributes_should_work =
      IsDeviceAttributesPermissionPolicyFeatureFlagEnabled()
          ? !IsBlockPolicySet() && IsPermissionsPolicyGranted()
          : IsAllowPolicySet();
  MaybeSetEnterprisePoliciesForOrigin(url_info.origin().Serialize());

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());
  ASSERT_NE(app_frame, nullptr);
  ASSERT_EQ(kDeviceAttributeNames.size(),
            kExpectedDeviceAttributeValues.size());
  for (size_t i = 0; i < kDeviceAttributeNames.size(); ++i) {
    if (device_attributes_should_work) {
      EXPECT_EQ(kExpectedDeviceAttributeValues[i],
                CallDeviceAttributesApi(app_frame, kDeviceAttributeNames[i]));
    } else {
      std::string expected_error;
      if (IsDeviceAttributesPermissionPolicyFeatureFlagEnabled() &&
          !IsPermissionsPolicyGranted()) {
        expected_error = kPermissionsPolicyError;
      } else {
        expected_error = kNoDeviceAttributesPermissionErrorMessage;
      }
      EXPECT_THAT(CallDeviceAttributesApi(app_frame, kDeviceAttributeNames[i]),
                  content::EvalJsResult::ErrorIs(HasSubstr(expected_error)));
    }
  }
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppDeviceAttributesBrowserTest,
                       ObtainingDeviceAttributesFromChildFrame) {
  IsolatedWebAppUrlInfo url_info = InstallApp();
  MaybeSetEnterprisePoliciesForOrigin(url_info.origin().Serialize());

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
    EXPECT_THAT(CallDeviceAttributesApi(iframe, attribute_name),
                content::EvalJsResult::ErrorIs(HasSubstr(kChildFrameError)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppDeviceAttributesBrowserTest,
    ::testing::ConvertGenerator<
        IsolatedWebAppDeviceAttributesBrowserTestParams::TupleT>(
        ::testing::Combine(
            ::testing::Bool(),  // kDeviceAttributesPermissionPolicy
                                // feature flag
            ::testing::Bool(),  // allow policy
            ::testing::Bool(),  // block policy
            ::testing::Bool()   // permissions policy
            )),
    [](const ::testing::TestParamInfo<
        IsolatedWebAppDeviceAttributesBrowserTestParams>& info) {
      return base::StringPrintf(
          "FeatureFlag%s_AllowPolicy%s_BlockPolicy%s_PermissionsPolicy%s",
          info.param.feature_flag ? "Enabled" : "Disabled",
          info.param.allow_policy ? "Set" : "Unset",
          info.param.block_policy ? "Set" : "Unset",
          info.param.permissions_policy ? "Granted" : "Denied");
    });
}  // namespace web_app
