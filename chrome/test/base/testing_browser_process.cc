// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/stub_notification_platform_bridge.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/permissions/chrome_permissions_client.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/metrics/metrics_service.h"
#include "components/network_time/network_time_tracker.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/permissions/permissions_client.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/apps/platform_apps/chrome_apps_browser_api_provider.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/ui/apps/chrome_app_window_client.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#include "chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/hid/hid_pinned_notification.h"
#include "chrome/browser/usb/usb_pinned_notification.h"
#else
#include "chrome/browser/hid/hid_status_icon.h"
#include "chrome/browser/usb/usb_status_icon.h"
#endif  // BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#endif

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
#include "chrome/browser/notifications/notification_ui_manager.h"
#endif

// static
TestingBrowserProcess* TestingBrowserProcess::GetGlobal() {
  return static_cast<TestingBrowserProcess*>(g_browser_process);
}

// static
void TestingBrowserProcess::CreateInstance() {
  DCHECK(!g_browser_process);
  TestingBrowserProcess* process = new TestingBrowserProcess;
  // Set |g_browser_process| before initializing the TestingBrowserProcess
  // because some members may depend on |g_browser_process| (in particular,
  // ChromeExtensionsBrowserClient).
  g_browser_process = process;
  process->Init();

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  if (features::IsOsLevelGeolocationPermissionSupportEnabled()) {
    auto fake_geolocation_system_permission_manager =
        std::make_unique<device::FakeGeolocationSystemPermissionManager>();
    fake_geolocation_system_permission_manager->SetSystemPermission(
        device::LocationSystemPermissionStatus::kAllowed);
    device::GeolocationSystemPermissionManager::SetInstance(
        std::move(fake_geolocation_system_permission_manager));
  }
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
}

// static
void TestingBrowserProcess::DeleteInstance() {
  // g_browser_process must be null during its own destruction.
  BrowserProcess* browser_process = g_browser_process;
  g_browser_process = nullptr;
  delete browser_process;
}

// static
void TestingBrowserProcess::StartTearDown() {
  TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
  if (browser_process) {
    browser_process->ShutdownBrowserPolicyConnector();
  }
}

// static
void TestingBrowserProcess::TearDownAndDeleteInstance() {
  TestingBrowserProcess::StartTearDown();
  TestingBrowserProcess::DeleteInstance();
}

TestingBrowserProcess::TestingBrowserProcess()
    : app_locale_("en"),
      platform_part_(std::make_unique<TestingBrowserProcessPlatformPart>()),
      os_crypt_async_(os_crypt_async::GetTestOSCryptAsyncForTesting()) {
}

TestingBrowserProcess::~TestingBrowserProcess() {
  EXPECT_FALSE(local_state_);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionsBrowserClient::Set(nullptr);
  extensions::AppWindowClient::Set(nullptr);
#endif

  if (test_network_connection_tracker_)
    content::SetNetworkConnectionTrackerForTesting(nullptr);

  // Destructors for some objects owned by TestingBrowserProcess will use
  // g_browser_process if it is not null, so it must be null before proceeding.
  DCHECK_EQ(static_cast<BrowserProcess*>(nullptr), g_browser_process);
}

void TestingBrowserProcess::Init() {
  // See comment in constructor.
  if (!network::TestNetworkConnectionTracker::HasInstance()) {
    test_network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    content::SetNetworkConnectionTrackerForTesting(
        test_network_connection_tracker_.get());
  }

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  extensions_browser_client_ =
      std::make_unique<extensions::DesktopAndroidExtensionsBrowserClient>();
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
#elif BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_browser_client_ =
      std::make_unique<extensions::ChromeExtensionsBrowserClient>();
  extensions_browser_client_->AddAPIProvider(
      std::make_unique<chrome_apps::ChromeAppsBrowserAPIProvider>());
  extensions::AppWindowClient::Set(ChromeAppWindowClient::GetInstance());
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
#endif

  // Make sure permissions client has been set.
  ChromePermissionsClient::GetInstance();

#if !BUILDFLAG(IS_ANDROID)
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(false);
#if BUILDFLAG(IS_CHROMEOS)
  hid_system_tray_icon_ = std::make_unique<HidPinnedNotification>();
  usb_system_tray_icon_ = std::make_unique<UsbPinnedNotification>();
#else
  hid_system_tray_icon_ = std::make_unique<HidStatusIcon>();
  usb_system_tray_icon_ = std::make_unique<UsbStatusIcon>();
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

  features_ = GlobalFeatures::CreateGlobalFeatures();
  features_->Init();
}

void TestingBrowserProcess::FlushLocalStateAndReply(base::OnceClosure reply) {
  // This could be implemented the same way as in BrowserProcessImpl but it's
  // not currently expected to be used by TestingBrowserProcess users so we
  // don't bother.
  CHECK(false);
}

void TestingBrowserProcess::EndSession() {
}

metrics_services_manager::MetricsServicesManager*
TestingBrowserProcess::GetMetricsServicesManager() {
  return nullptr;
}

metrics::MetricsService* TestingBrowserProcess::metrics_service() {
  return metrics_service_;
}

embedder_support::OriginTrialsSettingsStorage*
TestingBrowserProcess::GetOriginTrialsSettingsStorage() {
  if (!origin_trials_settings_storage_) {
    origin_trials_settings_storage_ =
        std::make_unique<embedder_support::OriginTrialsSettingsStorage>();
  }
  return origin_trials_settings_storage_.get();
}

SystemNetworkContextManager*
TestingBrowserProcess::system_network_context_manager() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestingBrowserProcess::shared_url_loader_factory() {
  return shared_url_loader_factory_;
}

network::NetworkQualityTracker*
TestingBrowserProcess::network_quality_tracker() {
  if (!test_network_quality_tracker_) {
    test_network_quality_tracker_ =
        std::make_unique<network::TestNetworkQualityTracker>();
  }
  return test_network_quality_tracker_.get();
}

ProfileManager* TestingBrowserProcess::profile_manager() {
  return profile_manager_.get();
}

void TestingBrowserProcess::SetMetricsService(
    metrics::MetricsService* metrics_service) {
  metrics_service_ = metrics_service;
}

void TestingBrowserProcess::SetProfileManager(
    std::unique_ptr<ProfileManager> profile_manager) {
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  // NotificationUIManager can contain references to elements in the current
  // ProfileManager. So when we change the ProfileManager (typically during test
  // shutdown) make sure to reset any objects that might maintain references to
  // it. See SetLocalState() for a description of a similar situation.
  notification_ui_manager_.reset();
#endif
  profile_manager_ = std::move(profile_manager);
}

void TestingBrowserProcess::SetVariationsService(
    variations::VariationsService* variations_service) {
  variations_service_ = variations_service;
}

PrefService* TestingBrowserProcess::local_state() {
  return local_state_;
}

signin::ActivePrimaryAccountsMetricsRecorder*
TestingBrowserProcess::active_primary_accounts_metrics_recorder() {
  return nullptr;
}

variations::VariationsService* TestingBrowserProcess::variations_service() {
  return variations_service_;
}

StartupData* TestingBrowserProcess::startup_data() {
  return nullptr;
}

policy::ChromeBrowserPolicyConnector*
TestingBrowserProcess::browser_policy_connector() {
  if (!browser_policy_connector_) {
    EXPECT_FALSE(created_browser_policy_connector_);
    created_browser_policy_connector_ = true;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
    // Make sure that the machine policy directory does not exist so that
    // machine-wide policies do not affect tests.
    // Note that passing false as last argument to OverrideAndCreateIfNeeded
    // means that the directory will not be created.
    // If a test needs to place a file in this directory in the future, we could
    // create a temporary directory and make its path available to tests.
    base::FilePath local_policy_path("/tmp/non/existing/directory");
    EXPECT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_POLICY_FILES, local_policy_path, true, false));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    browser_policy_connector_ =
        std::make_unique<policy::BrowserPolicyConnectorAsh>();
#else
    browser_policy_connector_ =
        std::make_unique<policy::ChromeBrowserPolicyConnector>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Note: creating the ChromeBrowserPolicyConnector invokes BrowserThread::
    // GetTaskRunnerForThread(), which initializes a base::LazyInstance of
    // BrowserThreadTaskRunners. However, the threads that these task runners
    // would run tasks on are *also* created lazily and might not exist yet.
    // Creating them requires a MessageLoop, which a test can optionally create
    // and manage itself, so don't do it here.
  }
  return browser_policy_connector_.get();
}

policy::PolicyService* TestingBrowserProcess::policy_service() {
  return browser_policy_connector()->GetPolicyService();
}

IconManager* TestingBrowserProcess::icon_manager() {
  return nullptr;
}

GpuModeManager* TestingBrowserProcess::gpu_mode_manager() {
  return nullptr;
}

BackgroundModeManager* TestingBrowserProcess::background_mode_manager() {
  return nullptr;
}

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
void TestingBrowserProcess::set_background_mode_manager_for_test(
    std::unique_ptr<BackgroundModeManager> manager) {
  NOTREACHED_IN_MIGRATION();
}
#endif

StatusTray* TestingBrowserProcess::status_tray() {
  return status_tray_.get();
}

safe_browsing::SafeBrowsingService*
TestingBrowserProcess::safe_browsing_service() {
  return sb_service_.get();
}

WebRtcLogUploader* TestingBrowserProcess::webrtc_log_uploader() {
  return webrtc_log_uploader_.get();
}

subresource_filter::RulesetService*
TestingBrowserProcess::subresource_filter_ruleset_service() {
  return subresource_filter_ruleset_service_.get();
}

subresource_filter::RulesetService*
TestingBrowserProcess::fingerprinting_protection_ruleset_service() {
  return fingerprinting_protection_ruleset_service_.get();
}

BrowserProcessPlatformPart* TestingBrowserProcess::platform_part() {
  return platform_part_.get();
}

NotificationUIManager* TestingBrowserProcess::notification_ui_manager() {
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  if (!notification_ui_manager_.get())
    notification_ui_manager_ = NotificationUIManager::Create();
  return notification_ui_manager_.get();
#else
  return nullptr;
#endif
}

NotificationPlatformBridge*
TestingBrowserProcess::notification_platform_bridge() {
  if (!notification_platform_bridge_.get()) {
    notification_platform_bridge_ =
        std::make_unique<StubNotificationPlatformBridge>();
  }
  return notification_platform_bridge_.get();
}

#if !BUILDFLAG(IS_ANDROID)
IntranetRedirectDetector* TestingBrowserProcess::intranet_redirect_detector() {
  return nullptr;
}
#endif

void TestingBrowserProcess::CreateDevToolsProtocolHandler() {}

void TestingBrowserProcess::CreateDevToolsAutoOpener() {
}

bool TestingBrowserProcess::IsShuttingDown() {
  return is_shutting_down_;
}

printing::PrintJobManager* TestingBrowserProcess::print_job_manager() {
#if BUILDFLAG(ENABLE_PRINTING)
  if (!print_job_manager_.get())
    print_job_manager_ = std::make_unique<printing::PrintJobManager>();
  return print_job_manager_.get();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

printing::PrintPreviewDialogController*
TestingBrowserProcess::print_preview_dialog_controller() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_dialog_controller_) {
    print_preview_dialog_controller_ =
        std::make_unique<printing::PrintPreviewDialogController>();
  }
  return print_preview_dialog_controller_.get();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

printing::BackgroundPrintingManager*
TestingBrowserProcess::background_printing_manager() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!background_printing_manager_.get()) {
    background_printing_manager_ =
        std::make_unique<printing::BackgroundPrintingManager>();
  }
  return background_printing_manager_.get();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

const std::string& TestingBrowserProcess::GetApplicationLocale() {
  return app_locale_;
}

void TestingBrowserProcess::SetApplicationLocale(
    const std::string& actual_locale) {
  app_locale_ = actual_locale;
}

DownloadStatusUpdater* TestingBrowserProcess::download_status_updater() {
  return nullptr;
}

DownloadRequestLimiter* TestingBrowserProcess::download_request_limiter() {
  if (!download_request_limiter_)
    download_request_limiter_ = base::MakeRefCounted<DownloadRequestLimiter>();
  return download_request_limiter_.get();
}

component_updater::ComponentUpdateService*
TestingBrowserProcess::component_updater() {
  return nullptr;
}

MediaFileSystemRegistry* TestingBrowserProcess::media_file_system_registry() {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
  return nullptr;
#else
  if (!media_file_system_registry_)
    media_file_system_registry_ = std::make_unique<MediaFileSystemRegistry>();
  return media_file_system_registry_.get();
#endif
}

network_time::NetworkTimeTracker*
TestingBrowserProcess::network_time_tracker() {
  if (!network_time_tracker_) {
    if (!local_state_)
      return nullptr;

    network_time_tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(new base::DefaultClock()),
        std::unique_ptr<base::TickClock>(new base::DefaultTickClock()),
        local_state_, nullptr, std::nullopt);
  }
  return network_time_tracker_.get();
}

#if !BUILDFLAG(IS_ANDROID)
gcm::GCMDriver* TestingBrowserProcess::gcm_driver() {
  return nullptr;
}
#endif

resource_coordinator::ResourceCoordinatorParts*
TestingBrowserProcess::resource_coordinator_parts() {
  if (!resource_coordinator_parts_) {
    resource_coordinator_parts_ =
        std::make_unique<resource_coordinator::ResourceCoordinatorParts>();
  }
  return resource_coordinator_parts_.get();
}

#if !BUILDFLAG(IS_ANDROID)
SerialPolicyAllowedPorts* TestingBrowserProcess::serial_policy_allowed_ports() {
  if (!serial_policy_allowed_ports_) {
    serial_policy_allowed_ports_ =
        std::make_unique<SerialPolicyAllowedPorts>(local_state());
  }
  return serial_policy_allowed_ports_.get();
}

HidSystemTrayIcon* TestingBrowserProcess::hid_system_tray_icon() {
  return hid_system_tray_icon_.get();
}

UsbSystemTrayIcon* TestingBrowserProcess::usb_system_tray_icon() {
  return usb_system_tray_icon_.get();
}
#endif

os_crypt_async::OSCryptAsync* TestingBrowserProcess::os_crypt_async() {
  return os_crypt_async_.get();
}

void TestingBrowserProcess::set_additional_os_crypt_async_provider_for_test(
    size_t precedence,
    std::unique_ptr<os_crypt_async::KeyProvider> provider) {
  // Not implemented.
  CHECK(false);
}

BuildState* TestingBrowserProcess::GetBuildState() {
#if !BUILDFLAG(IS_ANDROID)
  return &build_state_;
#else
  return nullptr;
#endif
}

GlobalFeatures* TestingBrowserProcess::GetFeatures() {
  return features_.get();
}

resource_coordinator::TabManager* TestingBrowserProcess::GetTabManager() {
  return resource_coordinator_parts()->tab_manager();
}

void TestingBrowserProcess::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  shared_url_loader_factory_ = shared_url_loader_factory;
}

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
void TestingBrowserProcess::SetNotificationUIManager(
    std::unique_ptr<NotificationUIManager> notification_ui_manager) {
  notification_ui_manager_.swap(notification_ui_manager);
}
#endif

void TestingBrowserProcess::SetSystemNotificationHelper(
    std::unique_ptr<SystemNotificationHelper> system_notification_helper) {
  system_notification_helper_ = std::move(system_notification_helper);
}

void TestingBrowserProcess::SetLocalState(PrefService* local_state) {
  if (!local_state) {
    // The local_state_ PrefService is owned outside of TestingBrowserProcess,
    // but some of the members of TestingBrowserProcess hold references to it
    // (for example, via PrefNotifier members). But given our test
    // infrastructure which tears down individual tests before freeing the
    // TestingBrowserProcess, there's not a good way to make local_state outlive
    // these dependencies. As a workaround, whenever local_state_ is cleared
    // (assumedly as part of exiting the test and freeing TestingBrowserProcess)
    // any components owned by TestingBrowserProcess that depend on local_state
    // are also freed.
    network_time_tracker_.reset();
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
    notification_ui_manager_.reset();
#endif
#if !BUILDFLAG(IS_ANDROID)
    serial_policy_allowed_ports_.reset();
#endif
    ShutdownBrowserPolicyConnector();
    created_browser_policy_connector_ = false;
  }
  local_state_ = local_state;
}

void TestingBrowserProcess::ShutdownBrowserPolicyConnector() {
  if (browser_policy_connector_) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
    // Initial cleanup for ChromeBrowserCloudManagement, shutdown components
    // that depend on profile and notification system. For example,
    // ProfileManager observer and KeyServices observer need to be removed
    // before profiles.
    auto* cloud_management_controller =
        browser_policy_connector_->chrome_browser_cloud_management_controller();
    if (cloud_management_controller) {
      cloud_management_controller->ShutDown();
    }
#endif
    browser_policy_connector_->Shutdown();
  }
  browser_policy_connector_.reset();
}

TestingBrowserProcessPlatformPart*
TestingBrowserProcess::GetTestPlatformPart() {
  return platform_part_.get();
}

void TestingBrowserProcess::SetSafeBrowsingService(
    safe_browsing::SafeBrowsingService* sb_service) {
  sb_service_ = sb_service;
}

void TestingBrowserProcess::SetWebRtcLogUploader(
    std::unique_ptr<WebRtcLogUploader> uploader) {
  webrtc_log_uploader_ = std::move(uploader);
}

void TestingBrowserProcess::SetRulesetService(
    std::unique_ptr<subresource_filter::RulesetService> ruleset_service) {
  subresource_filter_ruleset_service_.swap(ruleset_service);
}

void TestingBrowserProcess::SetFingerprintingProtectionRulesetService(
    std::unique_ptr<subresource_filter::RulesetService> ruleset_service) {
  fingerprinting_protection_ruleset_service_.swap(ruleset_service);
}

void TestingBrowserProcess::SetShuttingDown(bool is_shutting_down) {
  is_shutting_down_ = is_shutting_down;
}

void TestingBrowserProcess::SetStatusTray(
    std::unique_ptr<StatusTray> status_tray) {
  status_tray_ = std::move(status_tray);
}

#if !BUILDFLAG(IS_ANDROID)
void TestingBrowserProcess::SetHidSystemTrayIcon(
    std::unique_ptr<HidSystemTrayIcon> hid_system_tray_icon) {
  hid_system_tray_icon_ = std::move(hid_system_tray_icon);
}

void TestingBrowserProcess::SetUsbSystemTrayIcon(
    std::unique_ptr<UsbSystemTrayIcon> usb_system_tray_icon) {
  usb_system_tray_icon_ = std::move(usb_system_tray_icon);
}
#endif

///////////////////////////////////////////////////////////////////////////////

TestingBrowserProcessInitializer::TestingBrowserProcessInitializer() {
  TestingBrowserProcess::CreateInstance();
}

TestingBrowserProcessInitializer::~TestingBrowserProcessInitializer() {
  TestingBrowserProcess::TearDownAndDeleteInstance();
}
