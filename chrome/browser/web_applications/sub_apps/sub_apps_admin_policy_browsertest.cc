// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/to_value_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ui/views/web_apps/sub_apps/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

using testing::IsEmpty;
using testing::IsTrue;
using testing::SizeIs;

namespace web_app {

namespace {
constexpr const char kSub1[] = "/sub1/page.html";
constexpr const char kSub2[] = "/sub2/page.html";
}  // namespace

struct TestParam {
  std::string test_name;
  bool in_allow_list = false;
  bool other_origin_in_allow_list = false;
  bool in_block_list = false;
  bool other_origin_in_block_list = false;
  ContentSetting default_policy = CONTENT_SETTING_DEFAULT;
  bool user_accepts_prompt = true;
  bool expected_success = false;
};

class SubAppsAdminPolicyTest : public IsolatedWebAppBrowserTestHarness,
                               public testing::WithParamInterface<TestParam> {
 public:
  SubAppsAdminPolicyTest() = default;

  ~SubAppsAdminPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    IsolatedWebAppBrowserTestHarness::SetUpInProcessBrowserTestFixture();
  }

  IsolatedWebAppUrlInfo InstallIwaParentApp() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(
            ManifestBuilder().AddPermissionsPolicy(
                network::mojom::PermissionsPolicyFeature::kSubApps,
                /*self=*/true,
                /*origins=*/{}))
            .AddFolderFromDisk("/", "web_apps/subapps_isolated_app")
            .BuildBundle();
    app->TrustSigningKey();
    IsolatedWebAppUrlInfo parent_app = app->InstallChecked(profile());
    parent_app_id_ = parent_app.app_id();

    EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
        parent_app_id_, WebAppFilter::InstalledInOperatingSystemForTesting()));
    EXPECT_THAT(provider().registrar_unsafe().AppMatches(
                    parent_app_id_, WebAppFilter::IsIsolatedApp()),
                IsTrue());
    EXPECT_THAT(GetAllSubAppIds(parent_app_id_), IsEmpty());

    return parent_app;
  }

  std::vector<webapps::AppId> GetAllSubAppIds(
      const webapps::AppId& parent_app_id) {
    return provider().registrar_unsafe().GetAllSubAppIds(parent_app_id);
  }

  // sub_app_paths should contain paths, not full URLs.
  std::string AddSubAppsScript(
      const std::vector<std::string>& sub_app_paths) const {
    std::vector<std::string> script_parts;
    const std::string format = R"("$1": {"installURL": "$1"},)";

    script_parts.push_back("navigator.subApps.add({");
    for (const std::string& path : sub_app_paths) {
      script_parts.push_back(
          base::ReplaceStringPlaceholders(format, {path}, nullptr));
    }
    script_parts.push_back("})");

    return base::StrCat(script_parts);
  }

  void SetAllowlistedOrigins(
      const std::vector<std::string>& allowlisted_origins) {
    policy_map_.Set(policy::key::kSubAppsWithoutPromptsAllowedForOrigins,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_PLATFORM,
                    base::Value(base::ToValueList(allowlisted_origins)),
                    nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  void SetBlocklistedOrigins(
      const std::vector<std::string>& blocklisted_origins) {
    policy_map_.Set(policy::key::kSubAppsWithoutPromptsBlockedForOrigins,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_PLATFORM,
                    base::Value(base::ToValueList(blocklisted_origins)),
                    nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  void SetDefaultPolicySetting(ContentSetting value) {
    policy_map_.Set(policy::key::kDefaultSubAppsWithoutPromptsSetting,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_PLATFORM, base::Value(value),
                    nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

 protected:
  base::test::ScopedFeatureList features_{blink::features::kSubApps};

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  policy::PolicyMap policy_map_;

  webapps::AppId parent_app_id_;
};

IN_PROC_BROWSER_TEST_P(SubAppsAdminPolicyTest, InstallPolicyMatrix) {
  const TestParam& param = GetParam();
  auto parent = InstallIwaParentApp();

  std::vector<std::string> allowed_origins;
  if (param.in_allow_list) {
    allowed_origins.push_back(parent.origin().GetURL().spec());
  }
  if (param.other_origin_in_allow_list) {
    allowed_origins.push_back("https://www.google.com");
  }
  if (!allowed_origins.empty()) {
    SetAllowlistedOrigins(allowed_origins);
  }

  std::vector<std::string> blocked_origins;
  if (param.in_block_list) {
    blocked_origins.push_back(parent.origin().GetURL().spec());
  }
  if (param.other_origin_in_block_list) {
    blocked_origins.push_back("https://www.google.com");
  }
  if (!blocked_origins.empty()) {
    SetBlocklistedOrigins(blocked_origins);
  }

  if (param.default_policy != CONTENT_SETTING_DEFAULT) {
    SetDefaultPolicySetting(param.default_policy);
  }

  auto dialog_override =
      SubAppsInstallDialogController::SetAutomaticActionForTesting(
          param.user_accepts_prompt
              ? SubAppsInstallDialogController::DialogActionForTesting::kAccept
              : SubAppsInstallDialogController::DialogActionForTesting::
                    kCancel);

  auto* iwa_frame = OpenApp(parent.app_id());

  std::string script =
      base::StrCat({AddSubAppsScript({kSub1, kSub2}),
                    ".then(() => 'success').catch(() => 'failed')"});

  auto ret = EvalJs(iwa_frame, script,
                    content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  ASSERT_TRUE(ret.is_ok());

  if (param.expected_success) {
    EXPECT_EQ("success", ret.ExtractString());
    EXPECT_THAT(GetAllSubAppIds(parent_app_id_), SizeIs(2));
  } else {
    EXPECT_EQ("failed", ret.ExtractString());
    EXPECT_THAT(GetAllSubAppIds(parent_app_id_), IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SubAppsAdminPolicyTest,
    testing::Values(
        // =================================================================
        // NO DEFAULT POLICY (CONTENT_SETTING_DEFAULT -> Ask)
        // =================================================================
        // Baseline (no policy)
        TestParam{.test_name = "Default_Accept",
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = true,
                  .expected_success = true},
        TestParam{.test_name = "Default_Decline",
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = false,
                  .expected_success = false},
        // Origin only in allowlist
        TestParam{.test_name = "Default_InAllowlist",
                  .in_allow_list = true,
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = false,
                  .expected_success = true},
        // Origin only in blocklist
        TestParam{.test_name = "Default_InBlocklist",
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Origin in both lists (blocklist wins)
        TestParam{.test_name = "Default_InBothLists",
                  .in_allow_list = true,
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Irrelevant origin in allowlist
        TestParam{.test_name = "Default_OtherInAllowlist",
                  .other_origin_in_allow_list = true,
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = false,
                  .expected_success = false},
        // Irrelevant origin in blocklist
        TestParam{.test_name = "Default_OtherInBlocklist",
                  .other_origin_in_block_list = true,
                  .default_policy = CONTENT_SETTING_DEFAULT,
                  .user_accepts_prompt = true,
                  .expected_success = true},

        // =================================================================
        // DEFAULT POLICY = ALLOW
        // =================================================================
        // Baseline (no policy)
        TestParam{.test_name = "Allow_Accept",
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = true,
                  .expected_success = true},
        TestParam{.test_name = "Allow_Decline",
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = false,
                  .expected_success = true},
        // Origin only in allowlist
        TestParam{.test_name = "Allow_InAllowlist",
                  .in_allow_list = true,
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = false,
                  .expected_success = true},
        // Origin only in blocklist
        TestParam{.test_name = "Allow_InBlocklist",
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Origin in both lists (blocklist wins)
        TestParam{.test_name = "Allow_InBothLists",
                  .in_allow_list = true,
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Irrelevant origin in allowlist
        TestParam{.test_name = "Allow_OtherInAllowlist",
                  .other_origin_in_allow_list = true,
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = false,
                  .expected_success = true},
        // Irrelevant origin in blocklist
        TestParam{.test_name = "Allow_OtherInBlocklist",
                  .other_origin_in_block_list = true,
                  .default_policy = CONTENT_SETTING_ALLOW,
                  .user_accepts_prompt = false,
                  .expected_success = true},

        // =================================================================
        // DEFAULT POLICY = BLOCK
        // =================================================================
        // Baseline (no policy)
        TestParam{.test_name = "Block_Accept",
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        TestParam{.test_name = "Block_Decline",
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = false,
                  .expected_success = false},
        // Origin only in allowlist
        TestParam{.test_name = "Block_InAllowlist",
                  .in_allow_list = true,
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = false,
                  .expected_success = true},
        // Origin only in blocklist
        TestParam{.test_name = "Block_InBlocklist",
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Origin in both lists (blocklist wins)
        TestParam{.test_name = "Block_InBothLists",
                  .in_allow_list = true,
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Irrelevant origin in allowlist
        TestParam{.test_name = "Block_OtherInAllowlist",
                  .other_origin_in_allow_list = true,
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Irrelevant origin in blocklist
        TestParam{.test_name = "Block_OtherInBlocklist",
                  .other_origin_in_block_list = true,
                  .default_policy = CONTENT_SETTING_BLOCK,
                  .user_accepts_prompt = true,
                  .expected_success = false},

        // =================================================================
        // DEFAULT POLICY = ASK
        // =================================================================
        // Baseline (no policy)
        TestParam{.test_name = "Ask_Accept",
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = true,
                  .expected_success = true},
        TestParam{.test_name = "Ask_Decline",
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = false,
                  .expected_success = false},
        // Origin only in allowlist
        TestParam{.test_name = "Ask_InAllowlist",
                  .in_allow_list = true,
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = false,
                  .expected_success = true},
        // Origin only in blocklist
        TestParam{.test_name = "Ask_InBlocklist",
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Origin in both lists (blocklist wins)
        TestParam{.test_name = "Ask_InBothLists",
                  .in_allow_list = true,
                  .in_block_list = true,
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = true,
                  .expected_success = false},
        // Irrelevant origin in allowlist
        TestParam{.test_name = "Ask_OtherInAllowlist",
                  .other_origin_in_allow_list = true,
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = false,
                  .expected_success = false},
        // Irrelevant origin in blocklist
        TestParam{.test_name = "Ask_OtherInBlocklist",
                  .other_origin_in_block_list = true,
                  .default_policy = CONTENT_SETTING_ASK,
                  .user_accepts_prompt = true,
                  .expected_success = true}),
    [](const testing::TestParamInfo<TestParam>& info) {
      return info.param.test_name;
    });

}  // namespace web_app
