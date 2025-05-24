// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"

#include <map>
#include <memory>
#include <optional>
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
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
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
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"
#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/policy/uploading/status_uploader.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/webui/management/management_ui_handler_chromeos.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_signing_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/mock_user_permission_service.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

using testing::_;
using testing::AnyNumber;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;

struct ContextualManagementSourceUpdate {
  std::u16string extension_reporting_subtitle;
  std::u16string managed_websites_title;
  std::u16string subtitle;
#if BUILDFLAG(IS_CHROMEOS)
  std::u16string management_overview;
  std::u16string update_required_eol;
  bool show_monitored_network_privacy_disclosure;
  bool show_windows_notice_for_desk_sync;
  bool show_cookies_notice_for_desk_sync;
#else
  std::u16string browser_management_notice;
#endif  // BUILDFLAG(IS_CHROMEOS)
  bool managed;
};

namespace {
const char kUser[] = "user@domain.com";
#if BUILDFLAG(IS_CHROMEOS)
const GaiaId::Literal kGaiaId("gaia_id");
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
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
                                    state_keys_broker,
                                    crd_delegate_) {
    set_component_policy_disabled_for_testing(true);
  }
  ~TestDeviceCloudPolicyManagerAsh() override = default;

 private:
  policy::FakeStartCrdSessionJobDelegate crd_delegate_;
};

using ManagementUIHandlerBase = ManagementUIHandlerChromeOS;
#else
using ManagementUIHandlerBase = ManagementUIHandler;
#endif  // BUILDFLAG(IS_CHROMEOS)
class TestManagementUIHandler : public ManagementUIHandlerBase {
 public:
  TestManagementUIHandler() : ManagementUIHandlerBase(/*profile=*/nullptr) {}
  TestManagementUIHandler(policy::PolicyService* policy_service,
                          content::WebUI* web_ui)
      : ManagementUIHandlerBase(/*profile=*/nullptr),
        policy_service_(policy_service) {
    set_web_ui(web_ui);
  }

  ~TestManagementUIHandler() override = default;

  void EnableUpdateRequiredEolInfo(bool enable) {
    update_required_eol_ = enable;
  }

  base::Value::Dict GetContextualManagedDataForTesting(Profile* profile) {
    return GetContextualManagedData(profile);
  }

  base::Value::List GetReportingInfo(bool can_collect_signals = true,
                                     bool is_browser = true) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    EXPECT_CALL(mock_user_permission_service_, CanCollectSignals())
        .WillOnce(
            Return((can_collect_signals)
                       ? device_signals::UserPermission::kGranted
                       : device_signals::UserPermission::kMissingConsent));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    base::Value::List report_sources;
    AddReportingInfo(&report_sources, is_browser);
    return report_sources;
  }

  base::Value::List GetManagedWebsitesInfo(Profile* profile) {
    return ManagementUIHandlerBase::GetManagedWebsitesInfo(profile);
  }

  base::Value::Dict GetThreatProtectionInfo(Profile* profile) {
    return ManagementUIHandlerBase::GetThreatProtectionInfo(profile);
  }

  policy::PolicyService* GetPolicyService() override { return policy_service_; }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::UserPermissionService* GetUserPermissionService() override {
    return &mock_user_permission_service_;
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  std::u16string GetFilesUploadToCloudInfo(Profile* profile) {
    return ManagementUIHandlerBase::GetFilesUploadToCloudInfo(profile);
  }

  MOCK_METHOD(policy::DeviceCloudPolicyManagerAsh*,
              GetDeviceCloudPolicyManager,
              (),
              (const, override));
  bool IsUpdateRequiredEol() const override { return update_required_eol_; }

  const std::string GetDeviceManager() const override { return device_domain; }
  void SetDeviceDomain(const std::string& domain) { device_domain = domain; }

  const ash::SecureDnsManager* GetSecureDnsManager() const override {
    return secure_dns_manager_.get();
  }
  void CreateSecureDnsManagerForTesting(PrefService* local_state,
                                        user_manager::User& user) {
    secure_dns_manager_ = std::make_unique<ash::SecureDnsManager>(
        local_state, user, /*is_profile_managed=*/true);
  }
  void DestroySecureDnsManagerForTesting() { secure_dns_manager_.reset(); }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  raw_ptr<policy::PolicyService> policy_service_ = nullptr;
  bool update_required_eol_ = false;
  std::string device_domain = "devicedomain.com";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::MockUserPermissionService mock_user_permission_service_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::SecureDnsManager> secure_dns_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// We need to use a different base class for ChromeOS and non ChromeOS case.
// TODO(1071436, marcgrimme): refactor so that ChromeOS and non ChromeOS part is
// better separated.
class ManagementUIHandlerTests :
#if BUILDFLAG(IS_CHROMEOS)
    public ash::DeviceSettingsTestBase
#else
    public testing::Test
#endif
{
 public:
#if BUILDFLAG(IS_CHROMEOS)
  ManagementUIHandlerTests()
      : ash::DeviceSettingsTestBase(/*profile_creation_enabled=*/false),
        device_domain_(u"devicedomain.com"),
        task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        state_keys_broker_(&session_manager_client_),
        handler_(&policy_service_, &web_ui_) {
    ON_CALL(policy_service_, GetPolicies(_))
        .WillByDefault(ReturnRef(empty_policy_map_));
  }
#else
  ManagementUIHandlerTests() : handler_(&policy_service_, &web_ui_) {
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
    extracted_.extension_reporting_subtitle =
        ExtractPathFromDict(data, "extensionReportingSubtitle");
    extracted_.managed_websites_title =
        ExtractPathFromDict(data, "managedWebsitesSubtitle");
    extracted_.subtitle = ExtractPathFromDict(data, "pageSubtitle");
#if BUILDFLAG(IS_CHROMEOS)
    extracted_.management_overview = ExtractPathFromDict(data, "overview");
    extracted_.update_required_eol = ExtractPathFromDict(data, "eolMessage");
    extracted_.show_monitored_network_privacy_disclosure =
        data.FindBool("showMonitoredNetworkPrivacyDisclosure").value_or(false);
    extracted_.show_windows_notice_for_desk_sync =
        data.FindBool("showWindowsNoticeForDeskSync").value_or(false);
    extracted_.show_cookies_notice_for_desk_sync =
        data.FindBool("showCookiesNoticeForDeskSync").value_or(false);
#else
    extracted_.browser_management_notice =
        ExtractPathFromDict(data, "browserManagementNotice");
#endif  // BUILDFLAG(IS_CHROMEOS)
    std::optional<bool> managed = data.FindBool("managed");
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
    bool cloud_profile_reporting_enabled;
    std::string profile_name = kUser;
    bool override_policy_connector_is_managed;
    bool managed_account;
    bool managed_browser;
    bool managed_device;
    std::string device_domain;
    base::FilePath crostini_ansible_playbook_filepath;
    bool insights_extension_enabled;
    bool legacy_tech_reporting_enabled;
    bool real_time_url_check_connector_enabled;
    base::Value::List report_app_inventory;
    base::Value::List report_app_usage;
    base::Value::List report_website_telemetry;
    base::Value::List report_website_activity_allowlist;
    base::Value::List report_website_telemetry_allowlist;
    bool sync_windows;
    bool sync_cookies;
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
    setup_config_.cloud_profile_reporting_enabled = default_value;
    setup_config_.profile_name = kUser;
    setup_config_.override_policy_connector_is_managed = false;
    setup_config_.managed_account = true;
    setup_config_.managed_browser = true;
    setup_config_.managed_device = false;
    setup_config_.device_domain = "devicedomain.com";
    setup_config_.insights_extension_enabled = false;
    setup_config_.report_app_inventory = base::Value::List();
    setup_config_.report_app_usage = base::Value::List();
    setup_config_.report_website_telemetry = base::Value::List();
    setup_config_.report_website_activity_allowlist = base::Value::List();
    setup_config_.report_website_telemetry_allowlist = base::Value::List();
    setup_config_.legacy_tech_reporting_enabled = false;
    setup_config_.real_time_url_check_connector_enabled = default_value;
    setup_config_.sync_windows = false;
    setup_config_.sync_cookies = false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());
    fake_user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(local_state_.Get()));

    const AccountId account_id(AccountId::FromUserEmailGaiaId(kUser, kGaiaId));
    fake_user_manager_->AddGaiaUser(account_id,
                                    user_manager::UserType::kRegular);
    user_ = fake_user_manager_->FindUserAndModify(account_id);

    install_attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>(
        ash::StubInstallAttributes::CreateUnset());
    DeviceSettingsTestBase::SetUp();

    crostini_features_ = std::make_unique<crostini::FakeCrostiniFeatures>();
    SetUpConnectManager();
    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    ash::NetworkMetadataStore::RegisterPrefs(user_prefs_.registry());
    stub_resolver_config_reader_ =
        std::make_unique<StubResolverConfigReader>(local_state_.Get());
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        stub_resolver_config_reader_.get());
    // The |DeviceSettingsTestBase| setup above instantiates
    // |FakeShillManagerClient| with a default environment which will post
    // tasks on the current thread to setup a initial network configuration with
    // a connected default network.
    base::RunLoop().RunUntilIdle();
  }
  void TearDown() override {
    handler_.DestroySecureDnsManagerForTesting();
    stub_resolver_config_reader_.reset();
    network_handler_test_helper_.reset();
    manager_.reset();
    DeviceSettingsTestBase::TearDown();
    install_attributes_.reset();
    web_contents_.reset();
    fake_user_manager_->OnUserProfileWillBeDestroyed(
        AccountId::FromUserEmailGaiaId(GetTestConfig().profile_name, kGaiaId));
    profile_ = nullptr;
    profile_manager_.reset();
    user_ = nullptr;
    fake_user_manager_.Reset();
  }

  void SetUpConnectManager() {
    std::unique_ptr<policy::DeviceCloudPolicyStoreAsh> store =
        std::make_unique<policy::DeviceCloudPolicyStoreAsh>(
            device_settings_service_.get(), install_attributes_->Get(),
            base::SingleThreadTaskRunner::GetCurrentDefault());
    manager_ = std::make_unique<TestDeviceCloudPolicyManagerAsh>(
        std::move(store), &state_keys_broker_);
    manager_.get()->Initialize(local_state_.Get());
  }

  base::Value::List SetUpForReportingInfo() {
    GetTestConfig().override_policy_connector_is_managed = true;
    GetTestConfig().managed_device = true;
    if (!SetUpProfileAndHandler()) {
      return {};
    }
    const TestDeviceStatusCollector status_collector(
        local_state_.Get(), GetTestConfig().report_activity_times,
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
    local_state_.Get()->SetBoolean(enterprise_reporting::kCloudReportingEnabled,
                                   GetTestConfig().cloud_reporting_enabled);
    profile_->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
    profile_->GetPrefs()->SetInteger(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
        policy::POLICY_SCOPE_MACHINE);

    if (GetTestConfig().legacy_tech_reporting_enabled) {
      base::Value::List allowlist;
      allowlist.Append("www.example.com");
      profile_->GetTestingPrefService()->SetManagedPref(
          enterprise_reporting::kCloudLegacyTechReportAllowlist,
          std::make_unique<base::Value>(std::move(allowlist)));
    }

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

    profile_->GetPrefs()->SetList(
        ::reporting::kReportWebsiteTelemetry,
        std::move(GetTestConfig().report_website_telemetry));
    profile_->GetPrefs()->SetList(
        ::reporting::kReportWebsiteActivityAllowlist,
        std::move(GetTestConfig().report_website_activity_allowlist));
    profile_->GetPrefs()->SetList(
        ::reporting::kReportWebsiteTelemetryAllowlist,
        std::move(GetTestConfig().report_website_telemetry_allowlist));

    const policy::SystemLogUploader system_log_uploader(
        /*syslog_delegate=*/nullptr,
        /*task_runner=*/task_runner_);
    ON_CALL(testing::Const(handler_), GetDeviceCloudPolicyManager())
        .WillByDefault(Return(manager_.get()));
    base::Value::List result;
    ManagementUIHandlerBase::AddDeviceReportingInfoForTesting(
        &result, &status_collector, &system_log_uploader, GetProfile());
    if (GetTestConfig().report_dlp_events) {
      ManagementUIHandlerBase::AddDlpDeviceReportingElementForTesting(
          &result, kManagementReportDlpEvents);
    }
    return result;
  }
#else
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());
  }
  void TearDown() override {
    web_contents_.reset();
    profile_ = nullptr;
    profile_manager_.reset();
  }

#endif  // BUILDFLAG(IS_CHROMEOS)

  [[nodiscard]] bool SetUpProfileAndHandler() {
    if (profile_) {
      return false;
    }
#if BUILDFLAG(IS_CHROMEOS)
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(GetTestConfig().profile_name, kGaiaId);
    fake_user_manager_->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id));
    fake_user_manager_->SwitchActiveUser(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

    profile_ =
        profile_manager_->CreateTestingProfile(GetTestConfig().profile_name);
    if (GetTestConfig().override_policy_connector_is_managed) {
      profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
    }

#if BUILDFLAG(IS_CHROMEOS)
    // Set Floating Workspace (responsible for syncing windows) pref.
    profile_->GetTestingPrefService()->SetManagedPref(
        ash::prefs::kFloatingWorkspaceV2Enabled,
        std::make_unique<base::Value>(GetTestConfig().sync_windows));
    // Set Floating SSO (responsible for syncing cookies) pref.
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kFloatingSsoEnabled,
        std::make_unique<base::Value>(GetTestConfig().sync_cookies));
    fake_user_manager_->OnUserProfileCreated(account_id, profile_->GetPrefs());
#endif  // BUILDFLAG(IS_CHROMEOS)

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());
    handler_.SetAccountManagedForTesting(GetTestConfig().managed_account);
#if BUILDFLAG(IS_CHROMEOS)
    handler_.SetDeviceManagedForTesting(GetTestConfig().managed_device);
    handler_.SetDeviceDomain(GetTestConfig().device_domain);
    handler_.CreateSecureDnsManagerForTesting(local_state_.Get(), *user_.get());
#else
    handler_.SetBrowserManagedForTesting(GetTestConfig().managed_browser);
#endif
    base::Value::Dict data =
        handler_.GetContextualManagedDataForTesting(profile_);
    ExtractContextualSourceUpdate(data);

    return true;
  }

  [[nodiscard]] bool SetUpNoDomainProfile() {
    if (profile_) {
      return false;
    }
    profile_ =
        profile_manager_->CreateTestingProfile(GetTestConfig().profile_name);
    return true;
  }

  bool GetManaged() const { return extracted_.managed; }

#if BUILDFLAG(IS_CHROMEOS)
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

  bool GetShowWindowsNoticeForDeskSync() const {
    return extracted_.show_windows_notice_for_desk_sync;
  }

  bool GetShowCookiesNoticeForDeskSync() const {
    return extracted_.show_cookies_notice_for_desk_sync;
  }
#else

  std::u16string GetBrowserManagementNotice() const {
    return extracted_.browser_management_notice;
  }

#endif

  std::u16string GetExtensionReportingSubtitle() const {
    return extracted_.extension_reporting_subtitle;
  }

  std::u16string GetManagedWebsitesTitle() const {
    return extracted_.managed_websites_title;
  }

  std::u16string GetPageSubtitle() const { return extracted_.subtitle; }

  TestingProfile* GetProfile() const { return profile_; }

  TestConfig& GetTestConfig() { return setup_config_; }

 protected:
  TestConfig setup_config_;
  policy::MockPolicyService policy_service_;
  policy::PolicyMap empty_policy_map_;
  std::u16string device_domain_;
  ContextualManagementSourceUpdate extracted_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  TestingPrefServiceSimple user_prefs_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
#if BUILDFLAG(IS_CHROMEOS)
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  raw_ptr<user_manager::User> user_ = nullptr;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> install_attributes_;
  std::unique_ptr<crostini::FakeCrostiniFeatures> crostini_features_;
  std::unique_ptr<StubResolverConfigReader> stub_resolver_config_reader_;
  std::unique_ptr<TestDeviceCloudPolicyManagerAsh> manager_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  policy::ServerBackedStateKeysBroker state_keys_broker_;
  ash::ScopedTestingCrosSettings settings_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#else
  content::BrowserTaskEnvironment task_environment_;
#endif
  raw_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;

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

#if BUILDFLAG(IS_CHROMEOS)
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

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateProfileManagedOnly) {
  ResetTestConfig();
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_PROFILE_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateBrowserManagedOnly) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedNoDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(
      GetBrowserManagementNotice(),
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
          base::EscapeForHTML(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedNoDomain) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedConsumerDomain) {
  ResetTestConfig();
  GetTestConfig().override_policy_connector_is_managed = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedKnownDomain) {
  const std::string domain = "manager.com";
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  EXPECT_EQ(
      GetBrowserManagementNotice(),
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
          base::EscapeForHTML(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_FALSE(GetManaged());
}

TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateUnmanagedCustomerDomain) {
  ResetTestConfig();
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
  EXPECT_EQ(
      GetBrowserManagementNotice(),
      l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_NOT_MANAGED_NOTICE, chrome::kManagedUiLearnMoreUrl,
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
            l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  EXPECT_EQ(
      GetManagedWebsitesTitle(),
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  EXPECT_EQ(GetBrowserManagementNotice(),
            l10n_util::GetStringFUTF16(
                IDS_MANAGEMENT_BROWSER_NOTICE, chrome::kManagedUiLearnMoreUrl,
                base::EscapeForHTML(l10n_util::GetStringUTF16(
                    IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  EXPECT_TRUE(GetManaged());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ManagementUIHandlerTests,
       ManagementContextualSourceUpdateManagedAccountKnownDomain) {
  const std::string domain = "manager.com";
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  ResetTestConfig();
  GetTestConfig().profile_name = "managed@" + domain;
  GetTestConfig().override_policy_connector_is_managed = true;
  GetTestConfig().device_domain = "";
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  GetTestConfig().managed_account = false;
  GetTestConfig().managed_browser = false;
  GetTestConfig().device_domain = "";
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       l10n_util::GetStringUTF16(device_type)));
  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_EQ(
      GetUpdateRequiredEolMessage(),
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_UPDATE_REQUIRED_EOL_MESSAGE,
                                 device_domain(), ui::GetChromeOSDeviceName()));
  EXPECT_EQ(GetPageSubtitle(),
            l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       l10n_util::GetStringUTF16(device_type),
                                       device_domain()));
  EXPECT_EQ(GetExtensionReportingSubtitle(),
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  base::Value::List info = ManagementUIHandlerChromeOS::GetDeviceReportingInfo(
      nullptr, GetProfile());

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
      {kManagementReportDlpEvents, "dlp events"},
      {kManagementReportLoginLogout, "login-logout"},
      {kManagementReportFileEvents, "file events"}};

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
      {kManagementReportAndroidApplications, "android application"},
      {kManagementReportLoginLogout, "login-logout"},
      {kManagementReportFileEvents, "file events"}};

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
      {kManagementReportAppInfoAndActivity, "app info and activity"},
      {kManagementReportLoginLogout, "login-logout"},
      {kManagementReportFileEvents, "file events"}};

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
       ReportAllWebsiteTelemetry_AllowlistedTelemetryType) {
  ResetTestConfig(false);
  base::Value::List allowed_telemetry_types;
  allowed_telemetry_types.Append(::reporting::kWebsiteTelemetryUsageType);
  GetTestConfig().report_website_telemetry = std::move(allowed_telemetry_types);

  base::Value::List allowed_urls;
  allowed_urls.Append(ContentSettingsPattern::Wildcard().ToString());
  GetTestConfig().report_website_telemetry_allowlist = std::move(allowed_urls);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportAllWebsiteInfoAndActivity,
       "website info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ReportAllWebsiteTelemetry_DisallowedTelemetryTypes) {
  ResetTestConfig(false);

  base::Value::List allowed_urls;
  allowed_urls.Append(ContentSettingsPattern::Wildcard().ToString());
  GetTestConfig().report_website_telemetry_allowlist = std::move(allowed_urls);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ReportWebsiteTelemetry_AllowlistedTelemetryType) {
  ResetTestConfig(false);
  base::Value::List allowed_telemetry_types;
  allowed_telemetry_types.Append(::reporting::kWebsiteTelemetryUsageType);
  GetTestConfig().report_website_telemetry = std::move(allowed_telemetry_types);

  base::Value::List allowed_urls;
  allowed_urls.Append("[*.]google.com");
  GetTestConfig().report_website_telemetry_allowlist = std::move(allowed_urls);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportWebsiteInfoAndActivity, "website info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ReportWebsiteTelemetry_DisallowedTelemetryTypes) {
  ResetTestConfig(false);

  base::Value::List allowed_urls;
  allowed_urls.Append("[*.]google.com");
  GetTestConfig().report_website_telemetry_allowlist = std::move(allowed_urls);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ReportNoWebsiteTelemetry_AllowlistedTelemetryType) {
  ResetTestConfig(false);
  base::Value::List allowed_telemetry_types;
  allowed_telemetry_types.Append(::reporting::kWebsiteTelemetryUsageType);
  GetTestConfig().report_website_telemetry = std::move(allowed_telemetry_types);

  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ReportNoWebsiteTelemetry_DisallowedTelemetryTypes) {
  ResetTestConfig(false);

  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportAllWebsiteActivity) {
  ResetTestConfig(false);

  base::Value::List allowed_urls;
  allowed_urls.Append(ContentSettingsPattern::Wildcard().ToString());
  GetTestConfig().report_website_activity_allowlist = std::move(allowed_urls);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportAllWebsiteInfoAndActivity,
       "website info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportWebsiteActivity) {
  ResetTestConfig(false);

  base::Value::List allowed_urls;
  allowed_urls.Append("[*.]google.com");
  GetTestConfig().report_website_activity_allowlist = std::move(allowed_urls);
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementReportWebsiteInfoAndActivity, "website info and activity"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportNoWebsiteActivity) {
  ResetTestConfig(false);

  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests, ReportLegacyTechReport) {
  ResetTestConfig(false);
  GetTestConfig().legacy_tech_reporting_enabled = true;
  const base::Value::List info = SetUpForReportingInfo();
  const std::map<std::string, std::string> expected_elements = {
      {kManagementLegacyTechReport, "legacy-tech"}};

  ASSERT_PRED_FORMAT2(ReportingElementsToBeEQ, info, expected_elements);
}

TEST_F(ManagementUIHandlerTests,
       ShowPrivacyDisclosureForSecureDnsWithIdentifiers) {
  ResetTestConfig();
  local_state_.Get()->SetManagedPref(prefs::kDnsOverHttpsMode,
                                     base::Value(SecureDnsConfig::kModeSecure));
  local_state_.Get()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  local_state_.Get()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                          base::Value("www.test-dns.com"));

  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_TRUE(GetShowMonitoredNetworkPrivacyDisclosure());
}

TEST_F(ManagementUIHandlerTests, ShowPrivacyDisclosureForActiveProxy) {
  ResetTestConfig();
  // Set pref to use a proxy.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  ash::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                     local_state_.Get());
  user_prefs_.SetUserPref(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateAutoDetect());
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_TRUE(GetShowMonitoredNetworkPrivacyDisclosure());
}

TEST_F(ManagementUIHandlerTests, ProxyServerDisclosureDeviceOffline) {
  ResetTestConfig();
  // Simulate network disconnected state.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  ash::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                     local_state_.Get());
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
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_FALSE(GetShowMonitoredNetworkPrivacyDisclosure());

  ash::NetworkHandler::Get()->NetworkHandler::ShutdownPrefServices();
}

TEST_F(ManagementUIHandlerTests, HideProxyServerDisclosureForDirectProxy) {
  ResetTestConfig();
  // Set pref not to use proxy.
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
  ash::NetworkHandler::Get()->InitializePrefServices(&user_prefs_,
                                                     local_state_.Get());
  user_prefs_.SetUserPref(proxy_config::prefs::kProxy,
                          ProxyConfigDictionary::CreateDirect());
  base::RunLoop().RunUntilIdle();

  GetTestConfig().managed_device = true;
  ASSERT_TRUE(SetUpProfileAndHandler());

  EXPECT_FALSE(GetShowMonitoredNetworkPrivacyDisclosure());

  ash::NetworkHandler::Get()->NetworkHandler::ShutdownPrefServices();
}

TEST_F(ManagementUIHandlerTests, ShowDeskSyncWindowsNotice) {
  ResetTestConfig();
  GetTestConfig().managed_device = true;
  GetTestConfig().sync_windows = true;
  ASSERT_TRUE(SetUpProfileAndHandler());
  EXPECT_TRUE(GetShowWindowsNoticeForDeskSync());
}

TEST_F(ManagementUIHandlerTests, HideDeskSyncWindowsNotice) {
  ResetTestConfig();
  GetTestConfig().managed_device = true;
  GetTestConfig().sync_windows = false;
  ASSERT_TRUE(SetUpProfileAndHandler());
  EXPECT_FALSE(GetShowWindowsNoticeForDeskSync());
}

TEST_F(ManagementUIHandlerTests, ShowDeskSyncCookiesNotice) {
  ResetTestConfig();
  GetTestConfig().managed_device = true;
  GetTestConfig().sync_cookies = true;
  ASSERT_TRUE(SetUpProfileAndHandler());
  EXPECT_TRUE(GetShowCookiesNoticeForDeskSync());
}

TEST_F(ManagementUIHandlerTests, HideDeskSyncCookiesNotice) {
  ResetTestConfig();
  GetTestConfig().managed_device = true;
  GetTestConfig().sync_cookies = false;
  ASSERT_TRUE(SetUpProfileAndHandler());
  EXPECT_FALSE(GetShowCookiesNoticeForDeskSync());
}

#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoNoPolicySetNoMessage) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  auto reporting_info =
      handler_.GetReportingInfo(/*can_collect_signals=*/false);
  EXPECT_EQ(reporting_info.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, CloudReportingPolicy) {
  ResetTestConfig();
  policy::PolicyMap policies;
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policies));
  local_state_.Get()->SetBoolean(enterprise_reporting::kCloudReportingEnabled,
                                 true);
  ASSERT_TRUE(SetUpProfileAndHandler());

  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportVisitedUrl};
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected_messages.insert(kManagementDeviceSignalsDisclosure);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  ASSERT_PRED_FORMAT2(MessagesToBeEQ, handler_.GetReportingInfo(),
                      expected_messages);
}

TEST_F(ManagementUIHandlerTests, CloudProfileReportingPolicy) {
  ResetTestConfig(false);
  ASSERT_TRUE(SetUpProfileAndHandler());
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudProfileReportingEnabled,
      std::make_unique<base::Value>(true));

  std::set<std::string> expected_messages = {
      kProfileReportingOverview, kProfileReportingUsername,
      kProfileReportingBrowser,  kProfileReportingExtension,
      kProfileReportingPolicy,   kProfileReportingLearnMore};

  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false,
                                                /*is_browser=*/true),
                      {});
  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false,
                                                /*is_browser=*/false),
                      expected_messages);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
TEST_F(ManagementUIHandlerTests,
       CloudReportingPolicyWithoutDeviceSignalsConsent) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  policy::PolicyMap policies;
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policies));
  local_state_.Get()->SetBoolean(enterprise_reporting::kCloudReportingEnabled,
                                 true);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineName, kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportVisitedUrl};

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false),
                      expected_messages);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

TEST_F(ManagementUIHandlerTests, LegacyTechReport) {
  ResetTestConfig();
  policy::PolicyMap policies;
  EXPECT_CALL(policy_service_, GetPolicies(_))
      .WillRepeatedly(ReturnRef(policies));
  ASSERT_TRUE(SetUpProfileAndHandler());

  base::Value::List allowlist;
  allowlist.Append(base::Value("www.example.com"));
  profile_->GetTestingPrefService()->SetManagedPref(
      enterprise_reporting::kCloudLegacyTechReportAllowlist,
      std::make_unique<base::Value>(std::move(allowlist)));

  std::set<std::string> expected_messages = {kManagementLegacyTechReport};
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  expected_messages.insert(kManagementDeviceSignalsDisclosure);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  ASSERT_PRED_FORMAT2(MessagesToBeEQ, handler_.GetReportingInfo(),
                      expected_messages);
}

TEST_F(ManagementUIHandlerTests, ExtensionReportingInfoPoliciesMerge) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
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
  local_state_.Get()->SetBoolean(enterprise_reporting::kCloudReportingEnabled,
                                 true);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  std::set<std::string> expected_messages = {
      kManagementExtensionReportMachineNameAddress,
      kManagementExtensionReportUsername,
      kManagementExtensionReportVersion,
      kManagementExtensionReportExtensionsPlugin,
      kManagementExtensionReportUserBrowsingData,
      kManagementExtensionReportPerfCrash,
      kManagementLegacyTechReport,
      kManagementExtensionReportVisitedUrl};

  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));
  ASSERT_PRED_FORMAT2(MessagesToBeEQ,
                      handler_.GetReportingInfo(/*can_collect_signals=*/false),
                      expected_messages);
}

TEST_F(ManagementUIHandlerTests, ManagedWebsitiesInfoNoPolicySet) {
  ASSERT_TRUE(SetUpNoDomainProfile());
  auto info = handler_.GetManagedWebsitesInfo(profile_);
  EXPECT_EQ(info.size(), 0u);
}

TEST_F(ManagementUIHandlerTests, ManagedWebsitiesInfoWebsites) {
  ASSERT_TRUE(SetUpNoDomainProfile());
  base::Value::List managed_websites;
  base::Value::Dict entry;
  entry.Set("origin", "https://example.com");
  managed_websites.Append(std::move(entry));
  profile_->GetPrefs()->Set(prefs::kManagedConfigurationPerOrigin,
                            base::Value(std::move(managed_websites)));
  auto info = handler_.GetManagedWebsitesInfo(profile_);
  EXPECT_EQ(info.size(), 1u);
  EXPECT_EQ(info.begin()->GetString(), "https://example.com");
}

TEST_F(ManagementUIHandlerTests, ThreatReportingInfo) {
  policy::PolicyMap chrome_policies;
  const policy::PolicyNamespace chrome_policies_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());

  ASSERT_TRUE(SetUpNoDomainProfile());

#if BUILDFLAG(IS_CHROMEOS)
  handler_.SetDeviceDomain("");
#endif  // BUILDFLAG(IS_CHROMEOS)

  EXPECT_CALL(policy_service_, GetPolicies(chrome_policies_namespace))
      .WillRepeatedly(ReturnRef(chrome_policies));

  // When no policies are set, nothing to report.
  auto info = handler_.GetThreatProtectionInfo(profile_);
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
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 0);

  info = handler_.GetThreatProtectionInfo(profile_);
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to values that enable the feature without a usable DM
  // token, nothing to report.
  policy::SetDMTokenForTesting(policy::DMToken::CreateInvalidToken());
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_DOWNLOADED,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      "[{\"service_provider\":\"google\"}]");
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::PRINT,
      "[{\"service_provider\":\"google\"}]");
#if BUILDFLAG(IS_CHROMEOS)
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
      "[{\"service_provider\":\"google\"}]");
#endif
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/profile_->GetPrefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"extensionTelemetryEvent", {"*"}}});
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, 1);
  profile_->GetPrefs()->SetInteger(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckScope,
      policy::POLICY_SCOPE_MACHINE);

  info = handler_.GetThreatProtectionInfo(profile_);
  EXPECT_TRUE(info.FindList("info")->empty());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION),
      base::UTF8ToUTF16(*info.FindString("description")));

  // When policies are set to values that enable the feature with a usable DM
  // token, report them.
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("fake-token"));

  info = handler_.GetThreatProtectionInfo(profile_);
#if BUILDFLAG(IS_CHROMEOS)
  const size_t expected_size = 8u;
#else
  const size_t expected_size = 7u;
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
  {
    base::Value::Dict value;
    value.Set("title", kManagementOnExtensionTelemetryEvent);
    value.Set("permission", kManagementOnExtensionTelemetryVisibleData);
    expected_info.Append(std::move(value));
  }

  EXPECT_EQ(expected_info, *info.FindList("info"));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ManagementUIHandlerTests, GetFilesUploadToCloud) {
  ResetTestConfig();
  ASSERT_TRUE(SetUpProfileAndHandler());
  EXPECT_TRUE(handler_.GetFilesUploadToCloudInfo(profile_).empty());
  profile_->GetTestingPrefService()->SetManagedPref(
      ash::prefs::kCaptureModePolicySavePath,
      std::make_unique<base::Value>(
          policy::local_user_files::kOneDrivePolicyVariableName));

  EXPECT_FALSE(handler_.GetFilesUploadToCloudInfo(profile_).empty());
}
#endif  // BUILDFLAG(IS_CHROMEOS)
