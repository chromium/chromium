// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_settings_policy_handler.h"

#include <string_view>

#include "base/json/json_reader.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

const char kWebAppSettingWithDefaultConfiguration_Blocked[] = R"([
  {
    "manifest_id": "https://windowed.example/",
    "run_on_os_login": "run_windowed"
  },
  {
    "manifest_id": "https://allowed.example/",
    "run_on_os_login": "allowed"
  },
  {
    "manifest_id": "*",
    "run_on_os_login": "blocked"
  }
])";

const char kWebAppSettingWithDefaultConfiguration_Allowed[] = R"([
  {
    "manifest_id": "https://windowed.example/",
    "run_on_os_login": "run_windowed"
  },
  {
    "manifest_id": "https://allowed.example/",
    "run_on_os_login": "blocked"
  },
  {
    "manifest_id": "*",
    "run_on_os_login": "allowed"
  }
])";

const char kWebAppSettingNoDefaultConfiguration[] = R"([
  {
    "manifest_id": "https://windowed.example/",
    "run_on_os_login": "run_windowed"
  }
])";

const char kWebAppSettingDefaultConfiguration_RunOnOsLoginInvalid[] = R"([
  {
    "manifest_id": "*",
    "run_on_os_login": "unsupported_value"
  }
])";

const char kWebAppSettingDefaultConfiguration_MissingManifestId[] = R"([
  {
    "run_on_os_login": "unsupported_value"
  }
])";

const char kWebAppSettingForceUnregistration_WildCardManifestId[] = R"([
  {
    "manifest_id": "*",
    "force_unregister_os_integration": true
  }
])";

const char kWebAppSetting_InvalidForceUnregisterValue[] = R"([
  {
    "manifest_id": "https://windowed.example/",
    "force_unregister_os_integration": "invalid"
  }
])";

const char kWebAppSetting_ValidForceUnregisterValue[] = R"([
  {
    "manifest_id": "https://abc.example/",
    "force_unregister_os_integration": true
  }
])";

base::Value ReturnPolicyValueFromJson(std::string_view policy) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(
      policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  DCHECK(result.has_value()) << result.error().message;
  DCHECK(result->is_list());
  return std::move(*result);
}

}  // namespace

TEST(WebAppSettingsPolicyHandlerTest, CheckPolicySettings_ValidPatterns) {
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  WebAppSettingsPolicyHandler handler(chrome_schema);

  auto valid_configs = {kWebAppSettingWithDefaultConfiguration_Blocked,
                        kWebAppSettingWithDefaultConfiguration_Allowed,
                        kWebAppSettingNoDefaultConfiguration,
                        kWebAppSetting_ValidForceUnregisterValue};

  for (const char* config : valid_configs) {
    policy::PolicyErrorMap errors;
    policy::PolicyMap policy;

    policy.Set(policy::key::kWebAppSettings, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               ReturnPolicyValueFromJson(config), nullptr);

    EXPECT_TRUE(handler.CheckPolicySettings(policy, &errors));
    EXPECT_TRUE(errors.empty());
  }
}

TEST(WebAppSettingsPolicyHandlerTest, CheckPolicySettings_RunOnOsLoginInvalid) {
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  WebAppSettingsPolicyHandler handler(chrome_schema);

  policy::PolicyErrorMap errors;
  policy::PolicyMap policy;

  policy.Set(policy::key::kWebAppSettings, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             ReturnPolicyValueFromJson(
                 kWebAppSettingDefaultConfiguration_RunOnOsLoginInvalid),
             nullptr);

  EXPECT_FALSE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(policy::key::kWebAppSettings).empty());
}

TEST(WebAppSettingsPolicyHandlerTest, CheckPolicySettings_MissingManifestId) {
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  WebAppSettingsPolicyHandler handler(chrome_schema);

  policy::PolicyErrorMap errors;
  policy::PolicyMap policy;

  policy.Set(policy::key::kWebAppSettings, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             ReturnPolicyValueFromJson(
                 kWebAppSettingDefaultConfiguration_MissingManifestId),
             nullptr);

  EXPECT_FALSE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(policy::key::kWebAppSettings).empty());
}

TEST(WebAppSettingsPolicyHandlerTest,
     CheckPolicySettings_ManifestWildCardForceUnregistration) {
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  WebAppSettingsPolicyHandler handler(chrome_schema);

  policy::PolicyErrorMap errors;
  policy::PolicyMap policy;

  policy.Set(policy::key::kWebAppSettings, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
             ReturnPolicyValueFromJson(
                 kWebAppSettingForceUnregistration_WildCardManifestId),
             nullptr);

  EXPECT_FALSE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(policy::key::kWebAppSettings).empty());
}

TEST(WebAppSettingsPolicyHandlerTest,
     CheckPolicySettings_InvalidForceUnregistration) {
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  WebAppSettingsPolicyHandler handler(chrome_schema);

  policy::PolicyErrorMap errors;
  policy::PolicyMap policy;

  policy.Set(
      policy::key::kWebAppSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
      ReturnPolicyValueFromJson(kWebAppSetting_InvalidForceUnregisterValue),
      nullptr);

  EXPECT_FALSE(handler.CheckPolicySettings(policy, &errors));
  EXPECT_FALSE(errors.empty());
  EXPECT_FALSE(errors.GetErrors(policy::key::kWebAppSettings).empty());
}

}  // namespace web_app
