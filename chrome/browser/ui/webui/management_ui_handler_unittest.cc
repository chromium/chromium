
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
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
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_signing_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#endif  // defined(OS_CHROMEOS)

using testing::_;
using testing::AnyNumber;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  base::string16 extension_reporting_title;
  base::string16 subtitle;
#if defined(OS_CHROMEOS)
  base::string16 management_overview;
  base::string16 update_required_eol;
  bool show_proxy_server_privacy_disclosure;
#else
  base::string16 browser_management_notice;
#endif  // defined(OS_CHROMEOS)
  bool managed;
};

namespace {
const char kDomain[] = "domain.com";
const char kUser[] = "user@domain.com";
const char kManager[] = "manager@domain.com";
#if defined(OS_CHROMEOS)
const char kGaiaId[] = "gaia_id";
#endif  // defined(OS_CHROMEOS)
}  // namespace

#if defined(OS_CHROMEOS)
// This class is just to mock the behaviour of the few flags we need for
// simulating the behaviour of the policy::DeviceStatusCollector.
// The expected flags are passed to the constructor.
class TestDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  TestDeviceStatusCollector(PrefService* local_state,
                            bool report_activity_times,
                            bool report_nics,
                            bool report_users,
                            bool report_hw_status,
                            bool report_crash_info,
                            bool report_app_info_and_activity)
      : policy::DeviceStatusCollector(local_state, nullptr),
        report_activity_times_(report_activity_times),
        report_nics_(report_nics),
        report_users_(report_users),
        report_hw_status_(report_hw_status),
        report_crash_info_(report_crash_info),
        report_app_info_and_activity_(report_app_info_and_activity) {}
  ~TestDeviceStatusCollector() override = default;

  bool ShouldReportActivityTimes() const override {
    return report_activity_times_;
  }
  bool ShouldReportNetworkInterfaces() const override { return report_nics_; }
  bool ShouldReportUsers() const override { return report_users_; }
  bool ShouldReportHardwareStatus() const override { return report_hw_status_; }
  bool ShouldReportCrashReportInfo() const override {
    return report_crash_info_;
  }
  bool ShouldReportAppInfoAndActivity() const override {
    return report_app_info_and_activity_;
  }

  // empty methods that need to be implemented but are of no use for this
  // case.
  void GetStatusAsync(
      const policy::StatusCollectorCallback& callback) override {}
  void OnSubmittedSuccessfully() override {}

 private:
  bool report_activity_times_;
  bool report_nics_;
  bool report_users_;
  bool report_hw_status_;
  bool report_crash_info_;
  bool report_app_info_and_activity_;
};

class TestDeviceCloudPolicyManagerChromeOS
    : public policy::DeviceCloudPolicyManagerChromeOS {
 public:
  TestDeviceCloudPolicyManagerChromeOS(
      std::unique_ptr<policy::DeviceCloudPolicyStoreChromeOS> store,
      policy::ServerBackedStateKeysBroker* state_keys_broker)
      : DeviceCloudPolicyManagerChromeOS(std::move(store),
                                         nullptr,
                                         nullptr,
                                         state_keys_broker) {
    set_component_policy_disabled_for_testing(true);
  }
  ~TestDeviceCloudPolicyManagerChromeOS() override = default;
};
#endif  // defined(OS_CHROMEOS)

class TestManagementUIHandler : public ManagementUIHandler {
 public:
  TestManagementUIHandler() = default;
  explicit TestManagementUIHandler(policy::PolicyService* policy_service)
      : policy_service_(policy_service) {}
  ~TestManagementUIHandler() override = default;

  void EnableCloudReportingExtension(bool enable) {
    cloud_reporting_extension_exists_ = enable;
  }

  void EnableUpdateRequiredEolInfo(bool enable) {
    update_required_eol_ = enable;
  }

  base::Value GetContextualManagedDataForTesting(Profile* profile) {
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
  MOCK_METHOD(policy::DeviceCloudPolicyManagerChromeOS*,
              GetDeviceCloudPolicyManager,
              (),
              (const, override));
  bool IsUpdateRequiredEol() const override { return update_required_eol_; }

  base::Value GetDeviceReportingInfo(
      const TestDeviceCloudPolicyManagerChromeOS* manager,
      const TestDeviceStatusCollector* collector,
      const policy::SystemLogUploader* uploader,
      Profile* profile) {
    base::Value report_sources = base::Value(base::Value::Type::LIST);
    AddDeviceReportingInfo(&report_sources, collector, uploader, profile);
    return report_sources;
  }

  const std::string GetDeviceDomain() const override { return device_domain; }
  void SetDeviceDomain(const std::string& domain) { device_domain = domain; }
#endif  // defined(OS_CHROMEOS)

 private:
  bool cloud_reporting_extension_exists_ = false;
  policy::PolicyService* policy_service_ = nullptr;
  bool update_required_eol_ = false;
  std::string device_domain = "devicedomain.com";
};

// We need to use a different base class for ChromeOS and non ChromeOS case.
// TODO(marcgrimme): refactor so that ChromeOS and non ChromeOS part is better
// separated.
#if defined(OS_CHROMEOS)
using TestingBaseClass = chromeos::DeviceSettingsTestBase;
#else
using TestingBaseClass = testing::Test;
#endif
class ManagementUIHandlerTests : public TestingBaseClass {
 public:
#if defined(OS_CHROMEOS)
  ManagementUIHandlerTests()
      : TestingBaseClass(),
        handler_(&policy_service_),
        device_domain_(base::UTF8ToUTF16("devicedomain.com")),
        task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        state_keys_broker_(&session_manager_client_) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }
#else
  ManagementUIHandlerTests() : TestingBaseClass(), handler_(&policy_service_) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }
#endif

  ~ManagementUIHandlerTests() override = default;

  base::string16 device_domain() { return device_domain_; }
  void EnablePolicy(const char* policy_key, policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(true), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      int value,
                      policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(value), nullptr);
  }
  void SetPolicyValue(const char* policy_key,
                      bool value,
                      policy::PolicyMap& policies) {
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(value), nullptr);
  }
  void SetConnectorPolicyValue(const char* policy_key,
                               const std::string& value,
                               policy::PolicyMap& policies) {
    auto policy_value = base::JSONReader::Read(value);
    EXPECT_TRUE(policy_value.has_value());
    policies.Set(policy_key, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 std::move(policy_value.value()), nullptr);
  }

  base::string16 ExtractPathFromDict(const base::Value& data,
                                     const std::string path) {
    const std::string* buf = data.FindStringPath(path);
    if (!buf)
      return base::string16();
    return base::UTF8ToUTF16(*buf);
  }

  void ExtractContextualSourceUpdate(const base::Value& data) {
    extracted_.extension_reporting_title =
        ExtractPathFromDict(data, "extensionReportingTitle");
    extracted_.subtitle = ExtractPathFromDict(data, "pageSubtitle");
#if defined(OS_CHROMEOS)
    extracted_.management_overview = ExtractPathFromDict(data, "overview");
    extracted_.update_required_eol = ExtractPathFromDict(data, "eolMessage");
    base::Optional<bool> showProxyDisclosure =
        data.FindBoolPath("showProxyServerPrivacyDisclosure");
    extracted_.show_proxy_server_privacy_disclosure =
        showProxyDisclosure.has_value() && showProxyDisclosure.value();
#else
    extracted_.browser_management_notice =
        ExtractPathFromDict(data, "browserManagementNotice");
#endif  // defined(OS_CHROMEOS)
    base::Optional<bool> managed = data.FindBoolPath("managed");
    extracted_.managed = managed.has_value() && managed.value();
  }

  /* Structure to organize the different configuration settings for each test
   * into configuration for a test case. */
  struct TestConfig {
    bool report_activity_times;
    bool report_nics;
    bool report_users;
    bool report_hw_status;
    bool report_crash_info;
    bool report_app_info_and_activity;
    bool upload_enabled;
    bool printing_send_username_and_filename;
    bool crostini_report_usage;
    bool cloud_reporting_enabled;
    std::string profile_name;
    bool override_policy_connector_is_managed;
    bool managed_account;
    bool managed_device;
    std::string device_domain;
    base::FilePath crostini_ansible_playbook_filepath;
  };

  void ResetTestConfig() { ResetTestConfig(true); }

  void ResetTestConfig(bool default_value) {
    setup_config_.report_activity_times = default_value;
    setup_config_.report_nics = default_value;
    setup_config_.report_users = default_value;
    setup_config_.report_hw_status = default_value;
    setup_config_.report_crash_info = default_value;
    setup_config_.report_app_info_and_activity = default_value;
    setup_config_.upload_enabled = default_value;
    setup_config_.printing_send_username_and_filename = default_value;
    setup_config_.crostini_report_usage = default_value;
    setup_config_.cloud_reporting_enabled = default_value;
    setup_config_.profile_name = "";
    setup_config_.override_policy_connector_is_managed = false;
    setup_config_.managed_account = true;
    setup_config_.managed_device = false;
    setup_config_.device_domain = "devicedomain.com";
  }

#if defined(OS_CHROMEOS)
  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    install_attributes_ =
        std::make_unique<chromeos::ScopedStubInstallAttributes>(
            chromeos::StubInstallAttributes::CreateUnset());
    scoped_feature_list_.Init();

    crostini_features_ = std::make_unique<crostini::FakeCrostiniFeatures>();
    SetUpConnectManager();
    chromeos::NetworkHandler::Initialize();
    // The |DeviceSettingsTestBase| setup above instantiates
    // |FakeShillManagerClient| with a default environment which will post
    // tasks on the current thread to setup a initial network configuration with
    // a connected default network.
    base::RunLoop().RunUntilIdle();
  }
  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    DeviceSettingsTestBase::TearDown();
  }

  void SetUpConnectManager() {
    RegisterLocalState(local_state_.registry());
    std::unique_ptr<policy::DeviceCloudPolicyStoreChromeOS> store =
        std::make_unique<policy::DeviceCloudPolicyStoreChromeOS>(
            device_settings_service_.get(), install_attributes_->Get(),
            base::ThreadTaskRunnerHandle::Get());
    manager_ = std::make_unique<TestDeviceCloudPolicyManagerChromeOS>(
        std::move(store), &state_keys_broker_);
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    manager_.get()->Initialize(&local_state_);
  }

  base::Value SetUpForReportingInfo() {
    GetTestConfig().override_policy_connector_is_managed = true;
    GetTestConfig().managed_device = true;
    SetUpProfileAndHandler();
    const TestDeviceStatusCollector* status_collector =
        new TestDeviceStatusCollector(
            &local_state_, GetTestConfig().report_activity_times,
            GetTestConfig().report_nics, GetTestConfig().report_users,
            GetTestConfig().report_hw_status, GetTestConfig().report_crash_info,
            GetTestConfig().report_app_info_and_activity);
    settings_.device_settings()->SetTrustedStatus(
        chromeos::CrosSettingsProvider::TRUSTED);
    settings_.device_settings()->SetBoolean(chromeos::kSystemLogUploadEnabled,
                                            GetTestConfig().upload_enabled);
    profile_->GetPrefs()->SetBoolean(
        prefs::kPrintingSendUsernameAndFilenameEnabled,
        GetTestConfig().printing_send_username_and_filename);
    profile_->GetPrefs()->SetBoolean(
        crostini::prefs::kReportCrostiniUsageEnabled,
        GetTestConfig().crostini_report_usage);
    local_state_.SetBoolean(enterprise_reporting::kCloudReportingEnabled,
                            GetTestConfig().cloud_reporting_enabled);
    scoped_feature_list()->Reset();
    scoped_feature_list()->InitAndEnableFeature(
        features::kEnterpriseReportingInChromeOS);

    profile_->GetPrefs()->SetFilePath(
        crostini::prefs::kCrostiniAnsiblePlaybookFilePath,
        GetTestConfig().crostini_ansible_playbook_filepath);
    crostini_features()->set_allowed(true);

    const policy::SystemLogUploader* system_uploader =
        new policy::SystemLogUploader(/*syslog_delegate=*/nullptr,
                                      /*task_runner=*/task_runner_);
    ON_CALL(testing::Const(handler_), GetDeviceCloudPolicyManager())
        .WillByDefault(Return(manager_.get()));

    return handler_.GetDeviceReportingInfo(manager_.get(), status_collector,
                                           system_uploader, GetProfile());
  }
#endif  // defined(OS_CHROMEOS)

  void SetUpProfileAndHandler() {
    TestingProfile::Builder builder;
    builder.SetProfileName(GetTestConfig().profile_name);
    if (GetTestConfig().override_policy_connector_is_managed) {
      builder.OverridePolicyConnectorIsManagedForTesting(true);
    }
    profile_ = builder.Build();
    handler_.SetAccountManagedForTesting(GetTestConfig().managed_account);
    handler_.SetDeviceManagedForTesting(GetTestConfig().managed_device);
#if defined(OS_CHROMEOS)
    handler_.SetDeviceDomain(GetTestConfig().device_domain);
#endif
    base::Value data =
        handler_.GetContextualManagedDataForTesting(profile_.get());
    ExtractContextualSourceUpdate(data);
  }

  bool GetManaged() const { return extracted_.managed; }

#if defined(OS_CHROMEOS)
  base::string16 GetManagementOverview() const {
    return extracted_.management_overview;
  }
  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

  crostini::FakeCrostiniFeatures* crostini_features() {
    return crostini_features_.get();
  }

  base::string16 GetUpdateRequiredEolMessage() const {
    return extracted_.update_required_eol;
  }

  bool GetShowProxyServerPrivacyDisclosure() const {
    return extracted_.show_proxy_server_privacy_disclosure;
  }
#else

  base::string16 GetBrowserManagementNotice() const {
    return extracted_.browser_management_notice;
  }

#endif

  base::string16 GetExtensionReportingTitle() const {
    return extracted_.extension_reporting_title;
  }

  base::string16 GetPageSubtitle() const { return extracted_.subtitle; }

  TestingProfile* GetProfile() const { return profile_.get(); }

  TestConfig& GetTestConfig() { return setup_config_; }

#if defined(OS_CHROMEOS)
  void OnFatalError() { DCHECK(false); }

  std::unique_ptr<policy::UserCloudPolicyManagerChromeOS>
  BuildCloudPolicyManager() {
    auto store = std::make_unique<policy::MockCloudPolicyStore>();
    EXPECT_CALL(*store, Load()).Times(AnyNumber());

    const AccountId account_id = AccountId::FromUserEmailGaiaId(kUser, kGaiaId);

    TestingProfile::Builder builder_managed_user;
    builder_managed_user.SetProfileName(kUser);
    builder_managed_user.OverridePolicyConnectorIsManagedForTesting(true);
    auto managed_user = builder_managed_user.Build();

    auto data_manager =
        std::make_unique<policy::MockCloudExternalDataManager>();
    EXPECT_CALL(*data_manager, Disconnect());

    return std::make_unique<policy::UserCloudPolicyManagerChromeOS>(
        managed_user.get(), std::move(store), std::move(data_manager),
        base::FilePath() /* component_policy_cache_path */,
        policy::UserCloudPolicyManagerChromeOS::PolicyEnforcement::
            kPolicyRequired,
        base::TimeDelta::FromMinutes(1) /* policy_refresh_timeout */,
        base::BindOnce(&ManagementUIHandlerTests::OnFatalError,
                       base::Unretained(this)),
        account_id, task_runner_);
  }
#else
  std::unique_ptr<policy::UserCloudPolicyManager> BuildCloudPolicyManager() {
    auto store = std::make_unique<policy::MockUserCloudPolicyStore>();
    EXPECT_CALL(*store, Load()).Times(AnyNumber());

    return std::make_unique<policy::UserCloudPolicyManager>(
        std::move(store), base::FilePath(),
        /*cloud_external_data_manager=*/nullptr,
        base::ThreadTaskRunnerHandle::Get(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }
#endif  // defined(OS_CHROMEOS)

 protected:
  TestConfig setup_config_;
  TestManagementUIHandler handler_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
  base::string16 device_domain_;
  ContextualManagementSourceUpdate extracted_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::ScopedStubInstallAttributes> install_attributes_;
  std::unique_ptr<crostini::FakeCrostiniFeatures> crostini_features_;
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple user_prefs_;
  std::unique_ptr<TestDeviceCloudPolicyManagerChromeOS> manager_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  policy::ServerBackedStateKeysBroker state_keys_broker_;
  chromeos::ScopedTestingCrosSettings settings_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#else
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
#endif
};

AssertionResult MessagesToBeEQ(const char* infolist_expr,
                               const char* expected_infolist_expr,
                               base::Value::ConstListView infolist,
                               const std::set<std::string>& expected_messages) {
  if (infolist.size() != expected_messages.size()) {
    return AssertionFailure()
           << " " << infolist_expr << " and " << expected_infolist_expr
           << " don't have the same size. (" << infolist.size() << ", "
           << expected_messages.size() << ")";
  }
  std::set<std::string> tmp_expected(expected_messages);
  for (const base::Value& info : infolist) {
    const std::string* message_id = info.FindStringKey("messageId");
    if (message_id) {
      if (tmp_expected.erase(*message_id) != 1u) {
        return AssertionFailure() << " message " << *message_id << " is not in "
                                  << expected_infolist_expr;
      }
    }
  }
  if (!tmp_expected.empty()) {
    return AssertionFailure()
           << " " << infolist_expr << " and " << expected_infolist_expr
           << " have different contents " << infolist.data();
  }
  return AssertionSuccess();
}

#if defined(OS_CHROMEOS)
AssertionResult ReportingElementsToBeEQ(
    const char* elements_expr,
    const char* expected_elements_expr,
    base::Value::ConstListView elements,
    const std::map<std::string, std::string> expected_elements) {
  if (elements.size() != expected_elements.size()) {
    return AssertionFailure()
           << elements_expr << " and " << expected_elements_expr
           << " don't have the same size. (" << elements.size() << ", "
           << expected_elements.size() << ")";
  }
  std::map<std::string, std::string> tmp_expected(expected_elements);
  for (const base::Value& element : elements) {
    const std::string* message_id = element.FindStringKey("messageId");
    const std::string* js_reporting_type =
        element.FindStringKey("reportingType");
    if (message_id && js_reporting_type) {
      auto tmp_reporting_type = tmp_expected.find(*message_id);
      if (tmp_reporting_type == tmp_expected.end()) {
        return AssertionFailure() << " could not find message " << *message_id
                                  << " in " << expected_elements_expr;
      }
      if (tmp_reporting_type->second != *js_reporting_type) {
        return AssertionFailure()
               << " expected reporting element \"" << *js_reporting_type
               << "\" with key \"" << *message_id << "\" doesn't match \""
               << tmp_reporting_type->second << "\" in \""
               << expected_elements_expr << "\"";
      }
      tmp_expected.erase(tmp_reporting_type);
    } else {
      return AssertionFailure()
             << " couldn't find key messageId or reportingType in "
             << elements.data();
    }
  }
  if (!tmp_expected.empty()) {
    AssertionResult result = AssertionFailure();
    result << " the following messageId and reportingTypes could not be "
              "matched {";
    for (const auto& element : tmp_expected) {
      result << " messageId: " << element.first << ", reportingType "
             << element.second;
    }
    result << "}";
    return result;
  }
  return AssertionSuccess();
}
#endif

#if !defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  ResetTestConfig();
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedConsumerDomain) {
  ResetTestConfig();
  GetTestConfig().override_policy_connector_is_managed = true;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  const std::string domain = "manager.com";
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().managed_account = false;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedCustomerDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedKnownDomain) {
  const std::string domain = "gmail.com.manager.com.gmail.com";
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_TRUE(GetManaged());
}

#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountKnownDomain) {
  const std::string domain = "manager.com";
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().device_domain = "";
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountUnknownDomain) {
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().device_domain = "";
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(GetManagementOverview(), base::string16());
  EXPECT_EQ(GetUpdateRequiredEolMessage(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDevice) {
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(GetManagementOverview(), base::string16());
  EXPECT_EQ(GetUpdateRequiredEolMessage(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccount) {
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@devicedomain.com";
  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY, device_domain()));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccountMultipleDomains) {
  const std::string domain = "manager.com";
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                device_domain(), base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), base::string16());
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests, ManagementContextualSourceUpdateUnmanaged) {
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().profile_name = "";
  GetTestConfig().managed_account = false;
  GetTestConfig().device_domain = "";
  SetUpProfileAndHandler();

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), base::string16());
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedDeviceAndAccountEol) {
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().managed_account = true;
  GetTestConfig().managed_device = true;
  handler_.EnableUpdateRequiredEolInfo(true);
  SetUpProfileAndHandler();

  EXPECT_EQ(
      GetUpdateRequiredEolMessage(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_UPDATE_REQUIRED_EOL_MESSAGE,
                                 device_domain(), ui::GetChromeOSDeviceName()));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                       device_domain()));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests, NoDeviceReportingInfo) {
  ResetTestConfig();
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().managed_account = false;
  SetUpProfileAndHandler();

  base::Value info =
      handler_.GetDeviceReportingInfo(nullptr, nullptr, nullptr, GetProfile());

  EXPECT_EQ(info.GetList().size(), 0u);
}

TEST_F(ManagementUIHandlerTests, AllEnabledDeviceReportingInfo) {
  ResetTestConfig(true);
  GetTestConfig().report_users = false;
  const base::Value info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportHardwareStatus, "device statistics"},
      {kManagementReportNetworkInterfaces, "device"},
      {kManagementReportCrashReports, "crash report"},
      {kManagementReportAppInfoAndActivity, "app info and activity"},
      {kManagementLogUploadEnabled, "logs"},
      {kManagementPrinting, "print"},
      {kManagementCrostini, "crostini"},
      {kManagementExtensionReportUsername, "username"},
      {kManagementReportExtensions, "extension"},
      {kManagementReportAndroidApplications, "android application"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info.GetList(),
                      expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       AllEnabledCrostiniAnsiblePlaybookDeviceReportingInfo) {
  ResetTestConfig(true);
  GetTestConfig().crostini_ansible_playbook_filepath = base::FilePath("/tmp/");
  const base::Value info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportHardwareStatus, "device statistics"},
      {kManagementReportNetworkInterfaces, "device"},
      {kManagementReportCrashReports, "crash report"},
      {kManagementReportAppInfoAndActivity, "app info and activity"},
      {kManagementLogUploadEnabled, "logs"},
      {kManagementPrinting, "print"},
      {kManagementCrostiniContainerConfiguration, "crostini"},
      {kManagementExtensionReportUsername, "username"},
      {kManagementReportExtensions, "extension"},
      {kManagementReportAndroidApplications, "android application"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info.GetList(),
                      expected_elements);
}

TEST_F(ManagementUIHandlerTests, OnlyReportUsersDeviceReportingInfo) {
  ResetTestConfig(false);
  GetTestConfig().report_users = true;
  base::Value info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportUsers, "supervised user"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info.GetList(),
                      expected_elements);
}

TEST_F(ManagementUIHandlerTests, AllDisabledDeviceReportingInfo) {
  ResetTestConfig(false);
  const base::Value info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info.GetList(),
                      expected_elements);
}

TEST_F(ManagementUIHandlerTests, ShowProxyServerDisclosure) {
  ResetTestConfig();
  // Set pref to use a proxy.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  chromeos::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                          &local_state_);
  base::Value policy_prefs_config = ProxyConfigDictionary::CreateAutoDetect();
  user_prefs_.SetUserPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_TRUE(GetShowProxyServerPrivacyDisclosure());
}

TEST_F(ManagementUIHandlerTests, ProxyServerDisclosureDeviceOffline) {
  ResetTestConfig();
  // Simulate network disconnected state.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  chromeos::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                          &local_state_);
  chromeos::NetworkStateHandler::NetworkStateList networks;
  chromeos::NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkListByType(chromeos::NetworkTypePattern::Default(),
                             true,   // configured_only
                             false,  // visible_only,
                             0,      // no limit to number of results
                             &networks);
  chromeos::ShillServiceClient::TestInterface* service =
      chromeos::DBusThreadManager::Get()
          ->GetShillServiceClient()
          ->GetTestInterface();
  for (const auto* const network : networks) {
    service->SetServiceProperty(network->path(), shill::kStateProperty,
                                base::Value(shill::kStateOffline));
  }
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_FALSE(GetShowProxyServerPrivacyDisclosure());

  chromeos::NetworkHandler::Get()->NetworkHandler::ShutdownPrefServices();
}

TEST_F(ManagementUIHandlerTests, HideProxyServerDisclosureForDirectProxy) {
  ResetTestConfig();
  // Set pref not to use proxy.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  chromeos::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                          &local_state_);
  base::Value policy_prefs_config = ProxyConfigDictionary::CreateDirect();
  user_prefs_.SetUserPref(
      proxy_config::prefs::kProxy,
      base::Value::ToUniquePtrValue(std::move(policy_prefs_config)));
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_FALSE(GetShowProxyServerPrivacyDisclosure());

  chromeos::NetworkHandler::Get()->NetworkHandler::ShutdownPrefServices();
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

  const std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportSafeBrowsingWarnings};

  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetExtensionReportingInfo().GetList(),
                      expected_messages);
}

TEST_F(ManagementUIHandlerTests, CloudReportingPolicy) {
  handler_.EnableCloudReportingExtension(false);

  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kCloudReportingEnabled, true, chrome_policies);

  const std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin};

  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetExtensionReportingInfo().GetList(),
                      expected_messages);
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

  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetExtensionReportingInfo().GetList(),
                      expected_messages);
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
  SetConnectorPolicyValue(policy::key::kOnFileAttachedEnterpriseConnector, "[]",
                          chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnFileDownloadedEnterpriseConnector,
                          "[]", chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnBulkDataEntryEnterpriseConnector,
                          "[]", chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnSecurityEventEnterpriseConnector,
                          "[]", chrome_policies);

  info = handler_.GetThreatProtectionInfo(profile_known_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_TRUE(threat_protection_info->FindListKey("info")->GetList().empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  // When policies are set to values that enable the feature, report it.
  SetConnectorPolicyValue(policy::key::kOnFileAttachedEnterpriseConnector,
                          "[{\"service_provider\":\"google\"}]",
                          chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnFileDownloadedEnterpriseConnector,
                          "[{\"service_provider\":\"google\"}]",
                          chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnBulkDataEntryEnterpriseConnector,
                          "[{\"service_provider\":\"google\"}]",
                          chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnSecurityEventEnterpriseConnector,
                          "[{\"service_provider\":\"google\"}]",
                          chrome_policies);
  SetConnectorPolicyValue(policy::key::kEnterpriseRealTimeUrlCheckMode, "1",
                          chrome_policies);

  info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  info.GetAsDictionary(&threat_protection_info);
  EXPECT_EQ(5u, threat_protection_info->FindListKey("info")->GetList().size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*threat_protection_info->FindStringKey("description")));

  base::Value expected_info(base::Value::Type::LIST);
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnFileAttachedEvent);
    value.SetStringKey("permission", kManagementOnFileAttachedVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnFileDownloadedEvent);
    value.SetStringKey("permission", kManagementOnFileDownloadedVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnBulkDataEntryEvent);
    value.SetStringKey("permission", kManagementOnBulkDataEntryVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementEnterpriseReportingEvent);
    value.SetStringKey("permission", kManagementEnterpriseReportingVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnPageVisitedEvent);
    value.SetStringKey("permission", kManagementOnPageVisitedVisibleData);
    expected_info.Append(std::move(value));
  }

  EXPECT_EQ(expected_info, *threat_protection_info->FindListKey("info"));
}

TEST_F(ManagementUIHandlerTests, GetAccountDomain) {
  TestingProfile::Builder builder_unmanaged_user;
  builder_unmanaged_user.SetProfileName(kUser);
  builder_unmanaged_user.OverridePolicyConnectorIsManagedForTesting(false);
  auto unmanaged_user = builder_unmanaged_user.Build();
  EXPECT_EQ("", handler_.GetAccountDomain(unmanaged_user.get()));

  TestingProfile::Builder builder_managed_user;
  builder_managed_user.SetProfileName(kUser);
  builder_managed_user.OverridePolicyConnectorIsManagedForTesting(true);
  auto managed_user = builder_managed_user.Build();
  EXPECT_EQ(kDomain, handler_.GetAccountDomain(managed_user.get()));
}

TEST_F(ManagementUIHandlerTests, GetAccountManager) {
  TestingProfile::Builder builder_managed_user;
  builder_managed_user.SetProfileName(kUser);
  builder_managed_user.OverridePolicyConnectorIsManagedForTesting(true);

#if defined(OS_CHROMEOS)
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  std::unique_ptr<TestingProfileManager> profile_manager =
      std::make_unique<TestingProfileManager>(
          TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager->SetUp());
  builder_managed_user.SetUserCloudPolicyManagerChromeOS(
      BuildCloudPolicyManager());
#else
  builder_managed_user.SetUserCloudPolicyManager(BuildCloudPolicyManager());
#endif
  auto managed_user = builder_managed_user.Build();

#if defined(OS_CHROMEOS)
  policy::UserCloudPolicyManagerChromeOS* policy_manager =
      managed_user->GetUserCloudPolicyManagerChromeOS();
  policy::MockCloudPolicyStore* mock_store =
      static_cast<policy::MockCloudPolicyStore*>(
          policy_manager->core()->store());
#else
  policy::UserCloudPolicyManager* policy_manager =
      managed_user->GetUserCloudPolicyManager();
  policy::MockUserCloudPolicyStore* mock_store =
      static_cast<policy::MockUserCloudPolicyStore*>(
          policy_manager->core()->store());
#endif

  DCHECK(mock_store);
  mock_store->policy_ = std::make_unique<enterprise_management::PolicyData>();

  // If no managed_by, then just calculate the domain from the user.
  EXPECT_FALSE(mock_store->policy_->has_managed_by());
  EXPECT_EQ(kDomain, handler_.GetAccountManager(managed_user.get()));

  // If managed_by is set, then use that value.
  mock_store->policy_->set_managed_by(kManager);
  EXPECT_EQ(kManager, handler_.GetAccountManager(managed_user.get()));
}
