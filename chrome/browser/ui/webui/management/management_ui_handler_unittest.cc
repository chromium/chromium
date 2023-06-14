// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/policy/uploading/status_uploader.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/power/power_manager_client.h"
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
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/mock_user_permission_service.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

using testing::_;
using testing::AnyNumber;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  std::u16string extension_reporting_title;
  std::u16string managed_websites_title;
  std::u16string subtitle;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::u16string management_overview;
  std::u16string update_required_eol;
  bool show_monitored_network_privacy_disclosure;
#else
  std::u16string browser_management_notice;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  bool managed;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {
const char kUser[] = "user@domain.com";
const char kGaiaId[] = "gaia_id";
}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This class is just to mock the behaviour of the few flags we need for
// simulating the behaviour of the policy::DeviceStatusCollector.
// The expected flags are passed to the constructor.
class TestDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  TestDeviceStatusCollector(PrefService* local_state,
                            bool report_activity_times,
                            bool report_nics,
                            bool report_hardware_data,
                            bool report_users,
                            bool report_crash_info,
                            bool report_app_info_and_activity)
      : policy::DeviceStatusCollector(local_state, nullptr, nullptr, nullptr),
        report_activity_times_(report_activity_times),
        report_nics_(report_nics),
        report_hardware_data_(report_hardware_data),
        report_users_(report_users),
        report_crash_info_(report_crash_info),
        report_app_info_and_activity_(report_app_info_and_activity) {}
  ~TestDeviceStatusCollector() override = default;

  bool IsReportingActivityTimes() const override {
    return report_activity_times_;
  }
  bool IsReportingNetworkData() const override { return report_nics_; }
  bool IsReportingHardwareData() const override {
    return report_hardware_data_;
  }
  bool IsReportingUsers() const override { return report_users_; }
  bool IsReportingCrashReportInfo() const override {
    return report_crash_info_;
  }
  bool IsReportingAppInfoAndActivity() const override {
    return report_app_info_and_activity_;
  }

  // empty methods that need to be implemented but are of no use for this
  // case.
  void GetStatusAsync(policy::StatusCollectorCallback callback) override {}
  void OnSubmittedSuccessfully() override {}

 private:
  bool report_activity_times_;
  bool report_nics_;
  bool report_hardware_data_;
  bool report_users_;
  bool report_crash_info_;
  bool report_app_info_and_activity_;
};

class TestDeviceCloudPolicyManagerAsh
    : public policy::DeviceCloudPolicyManagerAsh {
 public:
  TestDeviceCloudPolicyManagerAsh(
      std::unique_ptr<policy::DeviceCloudPolicyStoreAsh> store,
      policy::ServerBackedStateKeysBroker* state_keys_broker)
      : DeviceCloudPolicyManagerAsh(std::move(store),
                                    nullptr,
                                    nullptr,
                                    state_keys_broker) {
    set_component_policy_disabled_for_testing(true);
  }
  ~TestDeviceCloudPolicyManagerAsh() override = default;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class TestManagementUIHandler : public ManagementUIHandler {
 public:
  TestManagementUIHandler() = default;
  explicit TestManagementUIHandler(policy::PolicyService* policy_service)
      : policy_service_(policy_service) {}

  ~TestManagementUIHandler() override = default;

  void EnableUpdateRequiredEolInfo(bool enable) {
    update_required_eol_ = enable;
  }

  base::Value::Dict GetContextualManagedDataForTesting(Profile* profile) {
    return GetContextualManagedData(profile);
  }

  base::Value::List GetExtensionReportingInfo(bool can_collect_signals = true) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    EXPECT_CALL(mock_user_permission_service_, CanCollectSignals())
        .WillOnce(
            Return((can_collect_signals)
                       ? device_signals::UserPermission::kGranted
                       : device_signals::UserPermission::kMissingConsent));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    base::Value::List report_sources;
    AddReportingInfo(&report_sources);
    return report_sources;
  }

  base::Value::List GetManagedWebsitesInfo(Profile* profile) {
    return ManagementUIHandler::GetManagedWebsitesInfo(profile);
  }

  base::Value::Dict GetThreatProtectionInfo(Profile* profile) {
    return ManagementUIHandler::GetThreatProtectionInfo(profile);
  }

  policy::PolicyService* GetPolicyService() override { return policy_service_; }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::UserPermissionService* GetUserPermissionService() override {
    return &mock_user_permission_service_;
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(policy::DeviceCloudPolicyManagerAsh*,
              GetDeviceCloudPolicyManager,
              (),
              (const, override));
  bool IsUpdateRequiredEol() const override { return update_required_eol_; }

  const std::string GetDeviceManager() const override { return device_domain; }
  void SetDeviceDomain(const std::string& domain) { device_domain = domain; }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  raw_ptr<policy::PolicyService> policy_service_ = nullptr;
  bool update_required_eol_ = false;
  std::string device_domain = "devicedomain.com";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::MockUserPermissionService mock_user_permission_service_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
};

// We need to use a different base class for ChromeOS and non ChromeOS case.
// TODO(1071436, marcgrimme): refactor so that ChromeOS and non ChromeOS part is
// better separated.
#if BUILDFLAG(IS_CHROMEOS_ASH)
using TestingBaseClass = ash::DeviceSettingsTestBase;
#else
using TestingBaseClass = testing::Test;
#endif
class ManagementUIHandlerTests : public TestingBaseClass {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ManagementUIHandlerTests()
      : TestingBaseClass(),
        device_domain_(u"devicedomain.com"),
        task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        state_keys_broker_(&session_manager_client_),
        handler_(&policy_service_) {
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

  std::u16string device_domain() { return device_domain_; }
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

  std::u16string ExtractPathFromDict(const base::Value::Dict& data,
                                     const std::string path) {
    const std::string* buf = data.FindStringByDottedPath(path);
    if (!buf) {
      return std::u16string();
    }
    return base::UTF8ToUTF16(*buf);
  }

  void ExtractContextualSourceUpdate(const base::Value::Dict& data) {
    extracted_.extension_reporting_title =
        ExtractPathFromDict(data, "extensionReportingTitle");
    extracted_.managed_websites_title =
        ExtractPathFromDict(data, "managedWebsitesSubtitle");
    extracted_.subtitle = ExtractPathFromDict(data, "pageSubtitle");
#if BUILDFLAG(IS_CHROMEOS_ASH)
    extracted_.management_overview = ExtractPathFromDict(data, "overview");
    extracted_.update_required_eol = ExtractPathFromDict(data, "eolMessage");
    absl::optional<bool> showProxyDisclosure =
        data.FindBool("showMonitoredNetworkPrivacyDisclosure");
    extracted_.show_monitored_network_privacy_disclosure =
        showProxyDisclosure.has_value() && showProxyDisclosure.value();
#else
    extracted_.browser_management_notice =
        ExtractPathFromDict(data, "browserManagementNotice");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    absl::optional<bool> managed = data.FindBool("managed");
    extracted_.managed = managed.has_value() && managed.value();
  }

  /* Structure to organize the different configuration settings for each test
   * into configuration for a test case. */
  struct TestConfig {
    bool report_activity_times;
    bool report_nics;
    bool report_hardware_data;
    bool report_users;
    bool report_crash_info;
    bool report_app_info_and_activity;
    bool report_dlp_events;
    bool report_audio_status;
    bool report_device_peripherals;
    bool device_report_xdr_events;
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
    bool insights_extension_enabled;
    base::Value::List report_app_inventory;
    base::Value::List report_app_usage;
  };

  void ResetTestConfig() { ResetTestConfig(true); }

  void ResetTestConfig(bool default_value) {
    setup_config_.report_activity_times = default_value;
    setup_config_.report_nics = default_value;
    setup_config_.report_hardware_data = default_value;
    setup_config_.report_users = default_value;
    setup_config_.report_crash_info = default_value;
    setup_config_.report_app_info_and_activity = default_value;
    setup_config_.report_dlp_events = default_value;
    setup_config_.report_audio_status = default_value;
    setup_config_.report_device_peripherals = default_value;
    setup_config_.device_report_xdr_events = default_value;
    setup_config_.upload_enabled = default_value;
    setup_config_.printing_send_username_and_filename = default_value;
    setup_config_.crostini_report_usage = default_value;
    setup_config_.cloud_reporting_enabled = default_value;
    setup_config_.profile_name = "";
    setup_config_.override_policy_connector_is_managed = false;
    setup_config_.managed_account = true;
    setup_config_.managed_device = false;
    setup_config_.device_domain = "devicedomain.com";
    setup_config_.insights_extension_enabled = false;
    setup_config_.report_app_inventory = base::Value::List();
    setup_config_.report_app_usage = base::Value::List();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUp() override {
    install_attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>(
        ash::StubInstallAttributes::CreateUnset());
    DeviceSettingsTestBase::SetUp();

    crostini_features_ = std::make_unique<crostini::FakeCrostiniFeatures>();
    SetUpConnectManager();
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    ash::NetworkMetadataStore::RegisterPrefs(user_prefs_.registry());
    stub_resolver_config_reader_ =
        std::make_unique<StubResolverConfigReader>(&local_state_);
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        stub_resolver_config_reader_.get());
    // The |DeviceSettingsTestBase| setup above instantiates
    // |FakeShillManagerClient| with a default environment which will post
    // tasks on the current thread to setup a initial network configuration with
    // a connected default network.
    base::RunLoop().RunUntilIdle();
  }
  void TearDown() override {
    network_handler_test_helper_.reset();
    profile_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    DeviceSettingsTestBase::TearDown();
  }

  void SetUpConnectManager() {
    RegisterLocalState(local_state_.registry());
    std::unique_ptr<policy::DeviceCloudPolicyStoreAsh> store =
        std::make_unique<policy::DeviceCloudPolicyStoreAsh>(
            device_settings_service_.get(), install_attributes_->Get(),
            base::SingleThreadTaskRunner::GetCurrentDefault());
    manager_ = std::make_unique<TestDeviceCloudPolicyManagerAsh>(
        std::move(store), &state_keys_broker_);
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    manager_.get()->Initialize(&local_state_);
  }

  base::Value::List SetUpForReportingInfo() {
    GetTestConfig().override_policy_connector_is_managed = true;
    GetTestConfig().managed_device = true;
    SetUpProfileAndHandler();
    const TestDeviceStatusCollector status_collector(
        &local_state_, GetTestConfig().report_activity_times,
        GetTestConfig().report_nics, GetTestConfig().report_hardware_data,
        GetTestConfig().report_users, GetTestConfig().report_crash_info,
        GetTestConfig().report_app_info_and_activity);
    settings_.device_settings()->SetTrustedStatus(
        ash::CrosSettingsProvider::TRUSTED);
    settings_.device_settings()->SetBoolean(ash::kSystemLogUploadEnabled,
                                            GetTestConfig().upload_enabled);
    settings_.device_settings()->SetBoolean(
        ash::kReportDeviceAudioStatus, GetTestConfig().report_audio_status);
    settings_.device_settings()->SetBoolean(
        ash::kReportDevicePeripherals,
        GetTestConfig().report_device_peripherals);
    settings_.device_settings()->SetBoolean(
        ash::kDeviceReportXDREvents, GetTestConfig().device_report_xdr_events);
    profile_->GetPrefs()->SetBoolean(
        prefs::kPrintingSendUsernameAndFilenameEnabled,
        GetTestConfig().printing_send_username_and_filename);
    profile_->GetPrefs()->SetBoolean(
        crostini::prefs::kReportCrostiniUsageEnabled,
        GetTestConfig().crostini_report_usage);
    local_state_.SetBoolean(enterprise_reporting::kCloudReportingEnabled,
                            GetTestConfig().cloud_reporting_enabled);

    profile_->GetPrefs()->SetFilePath(
        crostini::prefs::kCrostiniAnsiblePlaybookFilePath,
        GetTestConfig().crostini_ansible_playbook_filepath);
    crostini_features()->set_is_allowed_now(true);

    profile_->GetPrefs()->SetBoolean(
        ::prefs::kInsightsExtensionEnabled,
        GetTestConfig().insights_extension_enabled);

    profile_->GetPrefs()->SetList(
        ::ash::reporting::kReportAppInventory,
        std::move(GetTestConfig().report_app_inventory));
    profile_->GetPrefs()->SetList(::ash::reporting::kReportAppUsage,
                                  std::move(GetTestConfig().report_app_usage));

    const policy::SystemLogUploader system_log_uploader(
        /*syslog_delegate=*/nullptr,
        /*task_runner=*/task_runner_);
    ON_CALL(testing::Const(handler_), GetDeviceCloudPolicyManager())
        .WillByDefault(Return(manager_.get()));
    base::Value::List result;
    ManagementUIHandler::AddDeviceReportingInfoForTesting(
        &result, &status_collector, &system_log_uploader, GetProfile());
    if (GetTestConfig().report_dlp_events) {
      ManagementUIHandler::AddDlpDeviceReportingElementForTesting(
          &result, kManagementReportDlpEvents);
    }
    return result;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetUpProfileAndHandler() {
    TestingProfile::Builder builder;
    builder.SetProfileName(GetTestConfig().profile_name);
    if (GetTestConfig().override_policy_connector_is_managed) {
      builder.OverridePolicyConnectorIsManagedForTesting(true);
    }
    profile_ = builder.Build();
    handler_.SetAccountManagedForTesting(GetTestConfig().managed_account);
    handler_.SetDeviceManagedForTesting(GetTestConfig().managed_device);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    handler_.SetDeviceDomain(GetTestConfig().device_domain);
#endif
    base::Value::Dict data =
        handler_.GetContextualManagedDataForTesting(profile_.get());
    ExtractContextualSourceUpdate(data);
  }

  bool GetManaged() const { return extracted_.managed; }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::u16string GetManagementOverview() const {
    return extracted_.management_overview;
  }

  crostini::FakeCrostiniFeatures* crostini_features() {
    return crostini_features_.get();
  }

  std::u16string GetUpdateRequiredEolMessage() const {
    return extracted_.update_required_eol;
  }

  bool GetShowMonitoredNetworkPrivacyDisclosure() const {
    return extracted_.show_monitored_network_privacy_disclosure;
  }
#else

  std::u16string GetBrowserManagementNotice() const {
    return extracted_.browser_management_notice;
  }

#endif

  std::u16string GetExtensionReportingTitle() const {
    return extracted_.extension_reporting_title;
  }

  std::u16string GetManagedWebsitesTitle() const {
    return extracted_.managed_websites_title;
  }

  std::u16string GetPageSubtitle() const { return extracted_.subtitle; }

  TestingProfile* GetProfile() const { return profile_.get(); }

  TestConfig& GetTestConfig() { return setup_config_; }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnFatalError() { DCHECK(false); }

  std::unique_ptr<policy::UserCloudPolicyManagerAsh> BuildCloudPolicyManager() {
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

    return std::make_unique<policy::UserCloudPolicyManagerAsh>(
        managed_user.get(), std::move(store), std::move(data_manager),
        base::FilePath() /* component_policy_cache_path */,
        policy::UserCloudPolicyManagerAsh::PolicyEnforcement::kPolicyRequired,
        &local_state_, base::Minutes(1) /* policy_refresh_timeout */,
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
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
  TestConfig setup_config_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
  std::u16string device_domain_;
  ContextualManagementSourceUpdate extracted_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> install_attributes_;
  std::unique_ptr<crostini::FakeCrostiniFeatures> crostini_features_;
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple user_prefs_;
  std::unique_ptr<StubResolverConfigReader> stub_resolver_config_reader_;
  std::unique_ptr<TestDeviceCloudPolicyManagerAsh> manager_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  policy::ServerBackedStateKeysBroker state_keys_broker_;
  ash::ScopedTestingCrosSettings settings_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#else
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  TestManagementUIHandler handler_;
};

AssertionResult MessagesToBeEQ(const char* infolist_expr,
                               const char* expected_infolist_expr,
                               const base::Value::List& infolist,
                               const std::set<std::string>& expected_messages) {
  std::set<std::string> tmp_expected(expected_messages);
  std::vector<std::string> tmp_info_messages;
  for (const base::Value& tmp_info : infolist) {
    const std::string* message = tmp_info.GetDict().FindString("messageId");
    if (message) {
      if (tmp_expected.erase(*message) != 1u) {
        tmp_info_messages.push_back(*message);
      }
    }
  }
  if (!tmp_expected.empty()) {
    AssertionResult result = AssertionFailure();
    result << "Expected messages from " << expected_infolist_expr
           << " has more contents than " << infolist_expr << std::endl
           << "Messages missing from test: ";
    for (const std::string& message : tmp_expected) {
      result << message << ", ";
    }
    return result;
  }
  if (!tmp_info_messages.empty()) {
    AssertionResult result = AssertionFailure();
    result << "Recieved messages from " << infolist_expr
           << " has more contents than " << expected_infolist_expr << std::endl
           << "Additional messages not expected: ";
    for (const std::string& message : tmp_info_messages) {
      result << message << ", ";
    }
    return result;
  }
  if (infolist.size() != expected_messages.size()) {
    return AssertionFailure()
           << " " << infolist_expr << " and " << expected_infolist_expr
           << " don't have the same size. (info: " << infolist.size()
           << ", expected: " << expected_messages.size() << ")";
  }
  return AssertionSuccess();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
AssertionResult ReportingElementsToBeEQ(
    const char* elements_expr,
    const char* expected_elements_expr,
    const base::Value::List& elements,
    const std::map<std::string, std::string> expected_elements) {
  std::map<std::string, std::string> tmp_expected(expected_elements);
  for (const base::Value& element : elements) {
    const std::string* message_id = element.GetDict().FindString("messageId");
    const std::string* js_reporting_type =
        element.GetDict().FindString("reportingType");
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
             << " couldn't find key messageId or reportingType in " << elements;
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
  if (elements.size() != expected_elements.size()) {
    return AssertionFailure()
           << elements_expr << " and " << expected_elements_expr
           << " don't have the same size. (" << elements.size() << ", "
           << expected_elements.size() << ")";
  }
  return AssertionSuccess();
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  ResetTestConfig();
  SetUpProfileAndHandler();

  EXPECT_EQ(GetExtensionReportingTitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE,
                base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_TRUE(GetManaged());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                       base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), std::u16string());
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(GetManagementOverview(), std::u16string());
  EXPECT_EQ(GetUpdateRequiredEolMessage(), std::u16string());
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 device_domain()));
  EXPECT_EQ(GetManagementOverview(), std::u16string());
  EXPECT_EQ(GetUpdateRequiredEolMessage(), std::u16string());
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 device_domain()));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), std::u16string());
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 device_domain()));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                device_domain(), base::UTF8ToUTF16(domain)));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), std::u16string());
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetManagementOverview(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED));
  EXPECT_EQ(GetUpdateRequiredEolMessage(), std::u16string());
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
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                                 device_domain()));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests, NoDeviceReportingInfo) {
  ResetTestConfig();
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().managed_account = false;
  SetUpProfileAndHandler();

  base::Value::List info =
      ManagementUIHandler::GetDeviceReportingInfo(nullptr, GetProfile());

  EXPECT_EQ(info.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, AllEnabledDeviceReportingInfo) {
  ResetTestConfig(true);
  GetTestConfig().report_users = false;
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportNetworkData, "device"},
      {kManagementReportDeviceAudioStatus, "device"},
      {kManagementReportDevicePeripherals, "peripherals"},
      {kManagementReportHardwareData, "device statistics"},
      {kManagementReportCrashReports, "crash report"},
      {kManagementReportAppInfoAndActivity, "app info and activity"},
      {kManagementLogUploadEnabled, "logs"},
      {kManagementPrinting, "print"},
      {kManagementCrostini, "crostini"},
      {kManagementExtensionReportUsername, "username"},
      {kManagementReportExtensions, "extension"},
      {kManagementReportAndroidApplications, "android application"},
      {kManagementReportDlpEvents, "dlp events"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       AllEnabledCrostiniAnsiblePlaybookDeviceReportingInfo) {
  ResetTestConfig(true);
  GetTestConfig().report_dlp_events = false;
  GetTestConfig().crostini_ansible_playbook_filepath = base::FilePath("/tmp/");
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportNetworkData, "device"},
      {kManagementReportDeviceAudioStatus, "device"},
      {kManagementReportDevicePeripherals, "peripherals"},
      {kManagementReportHardwareData, "device statistics"},
      {kManagementReportCrashReports, "crash report"},
      {kManagementReportAppInfoAndActivity, "app info and activity"},
      {kManagementLogUploadEnabled, "logs"},
      {kManagementPrinting, "print"},
      {kManagementCrostiniContainerConfiguration, "crostini"},
      {kManagementExtensionReportUsername, "username"},
      {kManagementReportExtensions, "extension"},
      {kManagementReportAndroidApplications, "android application"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, OnlyReportDlpEvents) {
  ResetTestConfig(false);
  GetTestConfig().report_dlp_events = true;
  base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportDlpEvents, "dlp events"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, OnlyReportUsersDeviceReportingInfo) {
  ResetTestConfig(false);
  GetTestConfig().report_users = true;
  base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportUsers, "supervised user"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, AllDisabledDeviceReportingInfo) {
  ResetTestConfig(false);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       DeviceReportingInfoWhenInsightsExtensionEnabled) {
  ResetTestConfig(false);
  GetTestConfig().insights_extension_enabled = true;
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportNetworkData, "device"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportDeviceAudioStatusEnabled) {
  ResetTestConfig(false);
  GetTestConfig().report_audio_status = true;
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportDeviceAudioStatus, "device"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportDevicePeripheralsEnabled) {
  ResetTestConfig(false);
  GetTestConfig().report_device_peripherals = true;
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportDevicePeripherals, "peripherals"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportDeviceXdrEventsEnabled) {
  ResetTestConfig(false);
  GetTestConfig().device_report_xdr_events = true;
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportActivityTimes, "device activity"},
      {kManagementReportAppInfoAndActivity, "app info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportAppInventory) {
  ResetTestConfig(false);
  base::Value::List allowed_app_types;
  allowed_app_types.Append(::ash::reporting::kAppCategoryAndroidApps);
  GetTestConfig().report_app_inventory = std::move(allowed_app_types);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportAppInfoAndActivity, "app info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportAppUsage) {
  ResetTestConfig(false);
  base::Value::List allowed_app_types;
  allowed_app_types.Append(::ash::reporting::kAppCategoryAndroidApps);
  GetTestConfig().report_app_usage = std::move(allowed_app_types);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportAppInfoAndActivity, "app info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ShowPrivacyDisclosureForSecureDnsWithIdentifiers) {
  ResetTestConfig();
  local_state_.Set(prefs::kDnsOverHttpsMode,
                   base::Value(SecureDnsConfig::kModeSecure));
  local_state_.Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  local_state_.Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                   base::Value("www.test-dns.com"));

  // Owned by |scoped_user_manager|.
  auto user_manager =
      std::make_unique<user_manager::FakeUserManager>(&local_state_);
  // The DNS templates with identifiers only work is a user is logged in.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(kUser, kGaiaId));
  user_manager->AddUser(account_id);
  user_manager::ScopedUserManager scoper(std::move(user_manager));

  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_TRUE(GetShowMonitoredNetworkPrivacyDisclosure());
}

TEST_F(ManagementUIHandlerTests,
       ShowPrivacyDisclosureForDeviceReportXDREvents) {
  ResetTestConfig();

  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  EXPECT_CALL(policy_service_, GetPolicies(chrome_policies_namespace))
      .WillRepeatedly(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kDeviceReportXDREvents, true, chrome_policies);

  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_TRUE(GetShowMonitoredNetworkPrivacyDisclosure());
}

TEST_F(ManagementUIHandlerTests, ShowPrivacyDisclosureForActiveProxy) {
  ResetTestConfig();
  // Set pref to use a proxy.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  ash::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                     &local_state_);
  user_prefs_.SetUserPref(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateAutoDetect());
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_TRUE(GetShowMonitoredNetworkPrivacyDisclosure());
}

TEST_F(ManagementUIHandlerTests, ProxyServerDisclosureDeviceOffline) {
  ResetTestConfig();
  // Simulate network disconnected state.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  ash::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                     &local_state_);
  ash::NetworkStateHandler::NetworkStateList networks;
  ash::NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      ash::NetworkTypePattern::Default(),
      true,   // configured_only
      false,  // visible_only,
      0,      // no limit to number of results
      &networks);
  ash::ShillServiceClient::TestInterface* service =
      ash::ShillServiceClient::Get()->GetTestInterface();
  for (const auto* const network : networks) {
    service->SetServiceProperty(network->path(), shill::kStateProperty,
                                base::Value(shill::kStateIdle));
  }
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_FALSE(GetShowMonitoredNetworkPrivacyDisclosure());

  ash::NetworkHandler::Get()->NetworkHandler::ShutdownPrefServices();
}

TEST_F(ManagementUIHandlerTests, HideProxyServerDisclosureForDirectProxy) {
  ResetTestConfig();
  // Set pref not to use proxy.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  ash::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                     &local_state_);
  user_prefs_.SetUserPref(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateDirect());
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  SetUpProfileAndHandler();

  EXPECT_FALSE(GetShowMonitoredNetworkPrivacyDisclosure());

  ash::NetworkHandler::Get()->NetworkHandler::ShutdownPrefServices();
}

#endif

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoNoPolicySetNoMessage) {
  auto reporting_info =
      handler_.GetExtensionReportingInfo(/*can_collect_signals=*/false);
  EXPECT_EQ(reporting_info.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, CloudReportingPolicy) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kCloudReportingEnabled, true, chrome_policies);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin};
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected_messages.insert(kManagementDeviceSignalsDisclosure);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  ASSERT_PRED_FORMAT2(MessagesToBeEQ, handler_.GetExtensionReportingInfo(),
                      expected_messages);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(ManagementUIHandlerTests,
       CloudReportingPolicyWithoutDeviceSignalsConsent) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kCloudReportingEnabled, true, chrome_policies);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin};
  ASSERT_PRED_FORMAT2(
      MessagesToBeEQ,
      handler_.GetExtensionReportingInfo(/*can_collect_signals=*/false),
      expected_messages);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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
  policy::PolicyMap chrome_policies;
  EXPECT_CALL(policy_service_,
              GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                  std::string())))
      .WillOnce(ReturnRef(chrome_policies));
  SetPolicyValue(policy::key::kCloudReportingEnabled, true, chrome_policies);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineNameAddress,
      kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportUserBrowsingData,
      kManagementExtensionReportPerfCrash};

  ASSERT_PRED_FORMAT2(
      MessagesToBeEQ,
      handler_.GetExtensionReportingInfo(/*can_collect_signals=*/false),
      expected_messages);
}

TEST_F(ManagementUIHandlerTests, ManagedWebsitiesInfoNoPolicySet) {
  TestingProfile::Builder builder_no_domain;
  auto profile = builder_no_domain.Build();
  auto info = handler_.GetManagedWebsitesInfo(profile.get());
  EXPECT_EQ(info.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, ManagedWebsitiesInfoWebsites) {
  TestingProfile::Builder builder_no_domain;
  auto profile = builder_no_domain.Build();
  base::Value::List managed_websites;
  base::Value::Dict entry;
  entry.Set("origin", "https://example.com");
  managed_websites.Append(std::move(entry));
  profile->GetPrefs()->Set(prefs::kManagedConfigurationPerOrigin,
                           base::Value(std::move(managed_websites)));
  auto info = handler_.GetManagedWebsitesInfo(profile.get());
  EXPECT_EQ(info.size(), 1u);
  EXPECT_EQ(info.begin()->GetString(), "https://example.com");
}

TEST_F(ManagementUIHandlerTests, ThreatReportingInfo) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());

  TestingProfile::Builder builder_no_domain;
  auto profile_no_domain = builder_no_domain.Build();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  handler_.SetDeviceDomain("");
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  EXPECT_CALL(policy_service_, GetPolicies(chrome_policies_namespace))
      .WillRepeatedly(ReturnRef(chrome_policies));

  // When no policies are set, nothing to report.
  auto info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to uninteresting values, nothing to report.
  SetConnectorPolicyValue(policy::key::kOnFileAttachedEnterpriseConnector, "[]",
                          chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnFileDownloadedEnterpriseConnector,
                          "[]", chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnBulkDataEntryEnterpriseConnector,
                          "[]", chrome_policies);
  SetConnectorPolicyValue(policy::key::kOnPrintEnterpriseConnector, "[]",
                          chrome_policies);
#if BUILDFLAG(IS_CHROMEOS)
  SetConnectorPolicyValue(policy::key::kOnFileTransferEnterpriseConnector, "[]",
                          chrome_policies);
#endif
  SetConnectorPolicyValue(policy::key::kOnSecurityEventEnterpriseConnector,
                          "[]", chrome_policies);
  profile_no_domain->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode, 0);

  info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to values that enable the feature without a usable DM
  // token, nothing to report.
  policy::SetDMTokenForTesting(policy::DMToken::CreateInvalidToken());
  enterprise_connectors::test::SetAnalysisConnector(
      profile_no_domain->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_no_domain->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_no_domain->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_no_domain->GetPrefs(), enterprise_connectors::PRINT,
      "[{\"service_provider\":\"google\"}]");
#if BUILDFLAG(IS_CHROMEOS)
  enterprise_connectors::test::SetAnalysisConnector(
      profile_no_domain->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
      "[{\"service_provider\":\"google\"}]");
#endif
  enterprise_connectors::test::SetOnSecurityEventReporting(
      profile_no_domain->GetPrefs(), true);
  profile_no_domain->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode, 1);
  profile_no_domain->GetPrefs()->SetInteger(
      prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to values that enable the feature with a usable DM
  // token, report them.
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));

  info = handler_.GetThreatProtectionInfo(profile_no_domain.get());
#if BUILDFLAG(IS_CHROMEOS)
  const size_t expected_size = 7u;
#else
  const size_t expected_size = 6u;
#endif
  EXPECT_EQ(expected_size, info.FindList("info")->size());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  base::Value::List expected_info;
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnFileAttachedEvent);
    value.Set("permission", kManagementOnFileAttachedVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnFileDownloadedEvent);
    value.Set("permission", kManagementOnFileDownloadedVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnBulkDataEntryEvent);
    value.Set("permission", kManagementOnBulkDataEntryVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnPrintEvent);
    value.Set("permission", kManagementOnPrintVisibleData);
    expected_info.Append(std::move(value));
  }
#if BUILDFLAG(IS_CHROMEOS)
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnFileTransferEvent);
    value.Set("permission", kManagementOnFileTransferVisibleData);
    expected_info.Append(std::move(value));
  }
#endif
  {
    base::Value::Dict value;
    value.Set("title", kManagementEnterpriseReportingEvent);
    value.Set("permission", kManagementEnterpriseReportingVisibleData);
    expected_info.Append(std::move(value));
  }
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnPageVisitedEvent);
    value.Set("permission", kManagementOnPageVisitedVisibleData);
    expected_info.Append(std::move(value));
  }

  EXPECT_EQ(expected_info, *info.FindList("info"));
}
