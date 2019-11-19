// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/test/base/testing_profile.h"

#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/devicetype_utils.h"
#endif  // defined(OS_CHROMEOS)

using testing::_;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  base::string16* extension_reporting_title;
  base::string16* subtitle;
#if defined(OS_CHROMEOS)
  base::string16* management_overview;
#else
  base::string16* browser_management_notice;
#endif  // defined(OS_CHROMEOS)
  bool* managed;
};

class TestManagementUIHandler : public ManagementUIHandler {
 public:
  TestManagementUIHandler() = default;
  explicit TestManagementUIHandler(policy::PolicyService* policy_service)
      : policy_service_(policy_service) {}
  ~TestManagementUIHandler() override = default;

  void EnableCloudReportingExtension(bool enable) {
    cloud_reporting_extension_exists_ = enable;
  }

  base::DictionaryValue GetContextualManagedDataForTesting(Profile* profile) {
    return GetContextualManagedData(profile);
  }

  base::Value GetExtensionReportingInfo() {
    base::Value report_sources(base::Value::Type::LIST);
    AddReportingInfo(&report_sources);
    return report_sources;
  }

  base::Value GetThreatProtectionInfo(Profile* profile) {
    return ManagementUIHandler::GetThreatProtectionInfo(profile);
  }

  policy::PolicyService* GetPolicyService() const override {
    return policy_service_;
  }

  const extensions::Extension* GetEnabledExtension(
      const std::string& extensionId) const override {
    if (cloud_reporting_extension_exists_)
      return extensions::ExtensionBuilder("dummy").SetID("id").Build().get();
    return nullptr;
  }

#if defined(OS_CHROMEOS)
  const std::string GetDeviceDomain() const override { return device_domain; }
  void SetDeviceDomain(const std::string& domain) { device_domain = domain; }
#endif  // defined(OS_CHROMEOS)

 private:
  bool cloud_reporting_extension_exists_ = false;
  policy::PolicyService* policy_service_ = nullptr;
  std::string device_domain = "devicedomain.com";
};

class ManagementUIHandlerTests : public testing::Test {
 public:
  ManagementUIHandlerTests()
      : handler_(&policy_service_),
        device_domain_(base::UTF8ToUTF16("devicedomain.com")) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }

  base::string16 device_domain() { return device_domain_; }
  void EnablePolicy(const char* policy_key, policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      policy::PolicyMap& policies,
                      int value) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(value), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      policy::PolicyMap& policies,
                      bool value) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(value), nullptr);
  }

  void ExtractContextualSourceUpdate(
      const base::DictionaryValue& data,
      const ContextualManagementSourceUpdate& extracted) {
    data.GetString("extensionReportingTitle",
                   extracted.extension_reporting_title);
    data.GetString("pageSubtitle", extracted.subtitle);
#if defined(OS_CHROMEOS)
    data.GetString("overview", extracted.management_overview);
#else
    data.GetString("browserManagementNotice",
                   extracted.browser_management_notice);
#endif  // defined(OS_CHROMEOS)
    data.GetBoolean("managed", extracted.managed);
  }

 protected:
  TestManagementUIHandler handler_;
  content::BrowserTaskEnvironment task_environment_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
  base::string16 device_domain_;
};

#if !defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  auto profile = TestingProfile::Builder().Build();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 browser_management_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &browser_management_notice,
      &managed};

  handler_.SetAccountManagedForTesting(false);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  auto profile = TestingProfile::Builder().Build();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 browser_management_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &browser_management_notice,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle, l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedConsumerDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@gmail.com");
  auto profile = builder.Build();

  base::string16 extensions_installed;
  base::string16 subtitle;
  base::string16 browser_management_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extensions_installed,
      &subtitle,
      &browser_management_notice,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extensions_installed,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle, l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 browser_management_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &browser_management_notice,
      &managed};

  handler_.SetAccountManagedForTesting(false);

  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedCustomerDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@googlemail.com");
  auto profile = builder.Build();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 browser_management_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &browser_management_notice,
      &managed};

  handler_.SetAccountManagedForTesting(false);

  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@gmail.com.manager.com.gmail.com");
  auto profile = builder.Build();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 browser_management_notice;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &browser_management_notice,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  handler_.SetDeviceManagedForTesting(false);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(data.DictSize(), 4u);
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                base::UTF8ToUTF16("gmail.com.manager.com.gmail.com")));
  EXPECT_EQ(browser_management_notice,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                base::UTF8ToUTF16("gmail.com.manager.com.gmail.com")));
  EXPECT_TRUE(managed);
}

#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountKnownDomain) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 management_overview;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &management_overview,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  handler_.SetDeviceManagedForTesting(false);
  handler_.SetDeviceDomain("");
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_EQ(management_overview,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                       base::UTF8ToUTF16("manager.com")));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountUnknownDomain) {
  TestingProfile::Builder builder;
  auto profile = builder.Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 management_overview;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &management_overview,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  handler_.SetDeviceManagedForTesting(false);
  handler_.SetDeviceDomain("");
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(management_overview, base::string16());
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDevice) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 management_overview;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &management_overview,
      &managed};

  handler_.SetAccountManagedForTesting(false);
  handler_.SetDeviceManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(management_overview, base::string16());
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccount) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@devicedomain.com");
  auto profile = builder.Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 management_overview;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &management_overview,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  handler_.SetDeviceManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(management_overview,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY, device_domain()));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccountMultipleDomains) {
  TestingProfile::Builder builder;
  builder.SetProfileName("managed@manager.com");
  auto profile = builder.Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 management_overview;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &management_overview,
      &managed};

  handler_.SetAccountManagedForTesting(true);
  handler_.SetDeviceManagedForTesting(true);
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);

  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(management_overview,
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                device_domain(), base::UTF8ToUTF16("manager.com")));
  EXPECT_TRUE(managed);
}

TEST_F(ManagementUIHandlerTests, ManagementContextualSourceUpdateUnmanaged) {
  auto profile = TestingProfile::Builder().Build();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();

  base::string16 extension_reporting_title;
  base::string16 subtitle;
  base::string16 management_overview;
  bool managed;
  ContextualManagementSourceUpdate extracted{
      &extension_reporting_title,
      &subtitle,
      &management_overview,
      &managed};

  handler_.SetAccountManagedForTesting(false);
  handler_.SetDeviceDomain("");
  auto data = handler_.GetContextualManagedDataForTesting(profile.get());
  ExtractContextualSourceUpdate(data, extracted);
  EXPECT_EQ(subtitle,
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(extension_reporting_title,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(management_overview,
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED));
  EXPECT_FALSE(managed);
}
#endif

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoNoPolicySetNoMessage) {
  handler_.EnableCloudReportingExtension(false);
  auto reporting_info = handler_.GetExtensionReportingInfo();
  EXPECT_EQ(reporting_info.GetList().size(), 0u);
}

TEST_F(ManagementUIHandlerTests,
       ExtensionReportingInfoCloudExtensionAddsDefaultPolicies) {
  handler_.EnableCloudReportingExtension(true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings};

  auto reporting_info = handler_.GetExtensionReportingInfo();
  const auto& reporting_info_list = reporting_info.GetList();

  for (const base::Value& info : reporting_info_list) {
    const std::string* messageId = info.FindStringKey("messageId");
    EXPECT_TRUE(expected_messages.find(*messageId) != expected_messages.end());
  }
  EXPECT_EQ(reporting_info.GetList().size(), expected_messages.size());
}

TEST_F(ManagementUIHandlerTests, CloudReportingPolicy) {
  handler_.EnableCloudReportingExtension(false);

  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kCloudReportingEnabled, chrome_policies, true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin};

  auto reporting_info = handler_.GetExtensionReportingInfo();
  const auto& reporting_info_list = reporting_info.GetList();

  for (const base::Value& info : reporting_info_list) {
    const std::string* messageId = info.FindStringKey("messageId");
    EXPECT_TRUE(expected_messages.find(*messageId) != expected_messages.end());
  }
  EXPECT_EQ(reporting_info.GetList().size(), expected_messages.size());
}

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoPoliciesMerge) {
  policy::PolicyMap on_prem_reporting_extension_beta_policies;
  policy::PolicyMap on_prem_reporting_extension_stable_policies;

  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kManagementExtensionReportVersion,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportUserIdData,
               on_prem_reporting_extension_beta_policies);
  EnablePolicy(kPolicyKeyReportPolicyData,
               on_prem_reporting_extension_stable_policies);

  EnablePolicy(kPolicyKeyReportMachineIdData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSafeBrowsingData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportSystemTelemetryData,
               on_prem_reporting_extension_stable_policies);
  EnablePolicy(kPolicyKeyReportUserBrowsingData,
               on_prem_reporting_extension_stable_policies);

  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_stable_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_stable_policies));

  EXPECT_CALL(policy_service_,
              GetPolicies(on_prem_reporting_extension_beta_policy_namespace))
      .WillOnce(ReturnRef(on_prem_reporting_extension_beta_policies));
  policy::PolicyMap empty_policy_map;
  EXPECT_CALL(policy_service_,
              GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                  std::string())))
      .WillOnce(ReturnRef(empty_policy_map));

  handler_.EnableCloudReportingExtension(true);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineNameAddress,
      kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings,
      kManagementExtensionReportUserBrowsingData,
      kManagementExtensionReportPerfCrash};

  auto reporting_info = handler_.GetExtensionReportingInfo();
  const auto& reporting_info_list = reporting_info.GetList();

  for (const base::Value& info : reporting_info_list) {
    const std::string* message_id = info.FindStringKey("messageId");
    ASSERT_TRUE(message_id != nullptr);
    EXPECT_TRUE(expected_messages.find(*message_id) != expected_messages.end());
  }
  EXPECT_EQ(reporting_info.GetList().size(), expected_messages.size());
}

TEST_F(ManagementUIHandlerTests, ThreatReportingInfo) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());

  TestingProfile::Builder builder_no_domain;
  auto profile_no_domain = builder_no_domain.Build();

  TestingProfile::Builder builder_known_domain;
  builder_known_domain.SetProfileName("managed@manager.com");
  auto profile_known_domain = builder_known_domain.Build();

#if defined(OS_CHROMEOS)
  handler_.SetDeviceDomain("");
#endif  // !defined(OS_CHROMEOS)

  EXPECT_CALL(policy_service_, GetPolicies(chrome_policies_namespace))
      .WillRepeatedly(ReturnRef(chrome_policies));

  base::DictionaryValue* threat_protection_info = nullptr;

  // When no policies are set, nothing to report.
  auto info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_TRUE(threat_protection_info->FindListKey("info")->GetList().empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  // When policies are set to uninteresting values, nothing to report.
  SetPolicyValue(policy::key::kCheckContentCompliance, chrome_policies, 0);
  SetPolicyValue(policy::key::kSendFilesForMalwareCheck, chrome_policies, 0);
  SetPolicyValue(policy::key::kUnsafeEventsReportingEnabled, chrome_policies,
                 false);
  info = handler_.GetThreatProtectionInfo(profile_known_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_TRUE(threat_protection_info->FindListKey("info")->GetList().empty());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION_BY,
          base::UTF8ToUTF16("manager.com")),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  // When policies are set to values that enable the feature, report it.
  SetPolicyValue(policy::key::kCheckContentCompliance, chrome_policies, 1);
  SetPolicyValue(policy::key::kSendFilesForMalwareCheck, chrome_policies, 2);
  SetPolicyValue(policy::key::kUnsafeEventsReportingEnabled, chrome_policies,
                 true);
  info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_EQ(3u, threat_protection_info->FindListKey("info")->GetList().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  base::Value expected_info(base::Value::Type::LIST);
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementDataLossPreventionName);
    value.SetStringKey("permission", kManagementDataLossPreventionPermissions);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementMalwareScanningName);
    value.SetStringKey("permission", kManagementMalwareScanningPermissions);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementEnterpriseReportingName);
    value.SetStringKey("permission", kManagementEnterpriseReportingPermissions);
    expected_info.Append(std::move(value));
  }

  EXPECT_EQ(expected_info, *threat_protection_info->FindListKey("info"));
}
