// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of BrowserProcess for unit tests that fails for most
// services. By preventing creation of services, we reduce dependencies and
// keep the profile clean. Clients of this class must handle the NULL return
// value, however.

#ifndef CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
#define CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/upgrade_detector/build_state.h"
#endif

class BackgroundModeManager;
class NotificationPlatformBridge;
class NotificationUIManager;
class PrefService;
class SystemNotificationHelper;

namespace extensions {
class ExtensionsBrowserClient;
}

namespace gcm {
class GCMDriver;
}

namespace metrics {
class MetricsService;
}

namespace network {
class TestNetworkConnectionTracker;
class TestNetworkQualityTracker;
}

namespace os_crypt_async {
class OSCryptAsync;
}

namespace policy {
class PolicyService;
}

namespace resource_coordinator {
class ResourceCoordinatorParts;
}

namespace variations {
class VariationsService;
}

class TestingBrowserProcess : public BrowserProcess {
 public:
  // Initializes |g_browser_process| with a new TestingBrowserProcess.
  static void CreateInstance();

  // Cleanly destroys |g_browser_process|, which has special deletion semantics.
  static void DeleteInstance();

  // Convenience method to get g_browser_process as a TestingBrowserProcess*.
  static TestingBrowserProcess* GetGlobal();

  // Convenience method to both teardown and destroy the TestingBrowserProcess
  // instance
  static void TearDownAndDeleteInstance();

  TestingBrowserProcess(const TestingBrowserProcess&) = delete;
  TestingBrowserProcess& operator=(const TestingBrowserProcess&) = delete;

  // BrowserProcess overrides:
  void EndSession() override;
  void FlushLocalStateAndReply(base::OnceClosure reply) override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* metrics_service() override;
  SystemNetworkContextManager* system_network_context_manager() override;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      override;
  network::NetworkQualityTracker* network_quality_tracker() override;
  embedder_support::OriginTrialsSettingsStorage*
  GetOriginTrialsSettingsStorage() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  signin::ActivePrimaryAccountsMetricsRecorder*
  active_primary_accounts_metrics_recorder() override;
  variations::VariationsService* variations_service() override;
  policy::ChromeBrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  BackgroundModeManager* background_mode_manager() override;
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  void set_background_mode_manager_for_test(
      std::unique_ptr<BackgroundModeManager> manager) override;
#endif
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  subresource_filter::RulesetService* subresource_filter_ruleset_service()
      override;
  subresource_filter::RulesetService*
  fingerprinting_protection_ruleset_service() override;
  BrowserProcessPlatformPart* platform_part() override;

  NotificationUIManager* notification_ui_manager() override;
  NotificationPlatformBridge* notification_platform_bridge() override;
#if !BUILDFLAG(IS_ANDROID)
  IntranetRedirectDetector* intranet_redirect_detector() override;
#endif
  void CreateDevToolsProtocolHandler() override;
  void CreateDevToolsAutoOpener() override;
  bool IsShuttingDown() override;
  printing::PrintJobManager* print_job_manager() override;
  printing::PrintPreviewDialogController* print_preview_dialog_controller()
      override;
  printing::BackgroundPrintingManager* background_printing_manager() override;
  const std::string& GetApplicationLocale() override;
  void SetApplicationLocale(const std::string& actual_locale) override;
  DownloadStatusUpdater* download_status_updater() override;
  DownloadRequestLimiter* download_request_limiter() override;
  StartupData* startup_data() override;

// TODO(crbug.com/40118868): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void StartAutoupdateTimer() override {}
#endif

  component_updater::ComponentUpdateService* component_updater() override;
  MediaFileSystemRegistry* media_file_system_registry() override;

  WebRtcLogUploader* webrtc_log_uploader() override;

  network_time::NetworkTimeTracker* network_time_tracker() override;

#if !BUILDFLAG(IS_ANDROID)
  gcm::GCMDriver* gcm_driver() override;
#endif
  resource_coordinator::TabManager* GetTabManager() override;
  resource_coordinator::ResourceCoordinatorParts* resource_coordinator_parts()
      override;
#if !BUILDFLAG(IS_ANDROID)
  SerialPolicyAllowedPorts* serial_policy_allowed_ports() override;
  HidSystemTrayIcon* hid_system_tray_icon() override;
  UsbSystemTrayIcon* usb_system_tray_icon() override;
#endif
  os_crypt_async::OSCryptAsync* os_crypt_async() override;
  void set_additional_os_crypt_async_provider_for_test(
      size_t precedence,
      std::unique_ptr<os_crypt_async::KeyProvider> provider) override;

  BuildState* GetBuildState() override;
  GlobalFeatures* GetFeatures() override;

  // Set the local state for tests. Consumer is responsible for cleaning it up
  // afterwards (using ScopedTestingLocalState, for example).
  void SetLocalState(PrefService* local_state);
  void SetMetricsService(metrics::MetricsService* metrics_service);
  void SetProfileManager(std::unique_ptr<ProfileManager> profile_manager);
  void SetSafeBrowsingService(safe_browsing::SafeBrowsingService* sb_service);
  void SetVariationsService(variations::VariationsService* variations_service);
  void SetWebRtcLogUploader(std::unique_ptr<WebRtcLogUploader> uploader);
  void SetRulesetService(
      std::unique_ptr<subresource_filter::RulesetService> ruleset_service);
  void SetFingerprintingProtectionRulesetService(
      std::unique_ptr<subresource_filter::RulesetService> ruleset_service);
  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  void SetNotificationUIManager(
      std::unique_ptr<NotificationUIManager> notification_ui_manager);
#endif
  void SetSystemNotificationHelper(
      std::unique_ptr<SystemNotificationHelper> system_notification_helper);
  void SetShuttingDown(bool is_shutting_down);
  void ShutdownBrowserPolicyConnector();
  TestingBrowserProcessPlatformPart* GetTestPlatformPart();
  void SetStatusTray(std::unique_ptr<StatusTray> status_tray);
#if !BUILDFLAG(IS_ANDROID)
  void SetHidSystemTrayIcon(
      std::unique_ptr<HidSystemTrayIcon> hid_system_tray_icon);
  void SetUsbSystemTrayIcon(
      std::unique_ptr<UsbSystemTrayIcon> usb_system_tray_icon);
#endif

 private:
  // Perform necessary cleanup prior to destruction of |g_browser_process|
  static void StartTearDown();

  // See CreateInstance() and DestoryInstance() above.
  TestingBrowserProcess();
  ~TestingBrowserProcess() override;

  void Init();

  std::string app_locale_;
  bool is_shutting_down_ = false;

  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
      browser_policy_connector_;
  bool created_browser_policy_connector_ = false;
  std::unique_ptr<network::TestNetworkQualityTracker>
      test_network_quality_tracker_;
  raw_ptr<metrics::MetricsService> metrics_service_ = nullptr;
  raw_ptr<variations::VariationsService> variations_service_ = nullptr;
  std::unique_ptr<ProfileManager> profile_manager_;

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  std::unique_ptr<NotificationUIManager> notification_ui_manager_;
#endif

  std::unique_ptr<NotificationPlatformBridge> notification_platform_bridge_;
  std::unique_ptr<SystemNotificationHelper> system_notification_helper_;
  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  std::unique_ptr<embedder_support::OriginTrialsSettingsStorage>
      origin_trials_settings_storage_;

#if BUILDFLAG(ENABLE_PRINTING)
  std::unique_ptr<printing::PrintJobManager> print_job_manager_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  std::unique_ptr<printing::BackgroundPrintingManager>
      background_printing_manager_;
  std::unique_ptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;
#endif

  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  std::unique_ptr<subresource_filter::RulesetService>
      subresource_filter_ruleset_service_;
  std::unique_ptr<subresource_filter::RulesetService>
      fingerprinting_protection_ruleset_service_;
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader_;

  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  // The following objects are not owned by TestingBrowserProcess:
  raw_ptr<PrefService> local_state_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<TestingBrowserProcessPlatformPart> platform_part_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      test_network_connection_tracker_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<MediaFileSystemRegistry> media_file_system_registry_;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;
#endif

  std::unique_ptr<resource_coordinator::ResourceCoordinatorParts>
      resource_coordinator_parts_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SerialPolicyAllowedPorts> serial_policy_allowed_ports_;
  std::unique_ptr<HidSystemTrayIcon> hid_system_tray_icon_;
  std::unique_ptr<UsbSystemTrayIcon> usb_system_tray_icon_;
  BuildState build_state_;
#endif

  std::unique_ptr<StatusTray> status_tray_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  std::unique_ptr<GlobalFeatures> features_;
};

// RAII (resource acquisition is initialization) for TestingBrowserProcess.
// Allows you to initialize TestingBrowserProcess before other member variables.
//
// This can be helpful if you are running a unit test inside the browser_tests
// suite because browser_tests do not make a TestingBrowserProcess for you.
//
// class MyUnitTestRunningAsBrowserTest : public testing::Test {
//  ...stuff...
//  private:
//   TestingBrowserProcessInitializer initializer_;
//   LocalState local_state_;  // Needs a BrowserProcess to initialize.
// };
class TestingBrowserProcessInitializer {
 public:
  TestingBrowserProcessInitializer();
  TestingBrowserProcessInitializer(const TestingBrowserProcessInitializer&) =
      delete;
  TestingBrowserProcessInitializer& operator=(
      const TestingBrowserProcessInitializer&) = delete;
  ~TestingBrowserProcessInitializer();
};

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
