// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"

class BackgroundModeManager;
class IOThread;
class NotificationPlatformBridge;
class NotificationUIManager;
class PrefService;
class WatchDogThread;

namespace content {
class NotificationService;
}

namespace extensions {
class ExtensionsBrowserClient;
}

namespace gcm {
class GCMDriver;
}

namespace network {
class NetworkQualityTracker;
class TestNetworkConnectionTracker;
}

namespace policy {
class PolicyService;
}

namespace resource_coordinator {
class TabLifecycleUnitSource;
}

class TestingBrowserProcess : public BrowserProcess {
 public:
  // Initializes |g_browser_process| with a new TestingBrowserProcess.
  static void CreateInstance();

  // Cleanly destroys |g_browser_process|, which has special deletion semantics.
  static void DeleteInstance();

  // Convenience method to get g_browser_process as a TestingBrowserProcess*.
  static TestingBrowserProcess* GetGlobal();

  // BrowserProcess overrides:
  void ResourceDispatcherHostCreated() override;
  void EndSession() override;
  void FlushLocalStateAndReply(base::OnceClosure reply) override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* metrics_service() override;
  rappor::RapporServiceImpl* rappor_service() override;
  IOThread* io_thread() override;
  SystemNetworkContextManager* system_network_context_manager() override;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      override;
  network::NetworkQualityTracker* network_quality_tracker() override;
  WatchDogThread* watchdog_thread() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  variations::VariationsService* variations_service() override;
  policy::ChromeBrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  BackgroundModeManager* background_mode_manager() override;
  void set_background_mode_manager_for_test(
      std::unique_ptr<BackgroundModeManager> manager) override;
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  safe_browsing::ClientSideDetectionService* safe_browsing_detection_service()
      override;
  subresource_filter::ContentRulesetService*
  subresource_filter_ruleset_service() override;
  optimization_guide::OptimizationGuideService* optimization_guide_service()
      override;
  net::URLRequestContextGetter* system_request_context() override;
  BrowserProcessPlatformPart* platform_part() override;

  extensions::EventRouterForwarder* extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  NotificationPlatformBridge* notification_platform_bridge() override;
  IntranetRedirectDetector* intranet_redirect_detector() override;
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

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  void StartAutoupdateTimer() override {}
#endif

  net_log::ChromeNetLog* net_log() override;
  component_updater::ComponentUpdateService* component_updater() override;
  component_updater::SupervisedUserWhitelistInstaller*
  supervised_user_whitelist_installer() override;
  MediaFileSystemRegistry* media_file_system_registry() override;

  WebRtcLogUploader* webrtc_log_uploader() override;

  network_time::NetworkTimeTracker* network_time_tracker() override;

  gcm::GCMDriver* gcm_driver() override;
  resource_coordinator::TabManager* GetTabManager() override;
  shell_integration::DefaultWebClientState CachedDefaultWebClientState()
      override;
  prefs::InProcessPrefServiceFactory* pref_service_factory() const override;
  data_use_measurement::ChromeDataUseMeasurement* data_use_measurement()
      override;

  // Set the local state for tests. Consumer is responsible for cleaning it up
  // afterwards (using ScopedTestingLocalState, for example).
  void SetLocalState(PrefService* local_state);
  void SetProfileManager(ProfileManager* profile_manager);
  void SetIOThread(IOThread* io_thread);
  void SetSafeBrowsingService(safe_browsing::SafeBrowsingService* sb_service);
  void SetRulesetService(
      std::unique_ptr<subresource_filter::ContentRulesetService>
          ruleset_service);
  void SetOptimizationGuideService(
      std::unique_ptr<optimization_guide::OptimizationGuideService>
          optimization_guide_service);
  void SetSystemRequestContext(net::URLRequestContextGetter* context_getter);
  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);
  void SetNotificationUIManager(
      std::unique_ptr<NotificationUIManager> notification_ui_manager);
  void SetNotificationPlatformBridge(
      std::unique_ptr<NotificationPlatformBridge> notification_platform_bridge);
  void SetRapporServiceImpl(rappor::RapporServiceImpl* rappor_service);
  void SetShuttingDown(bool is_shutting_down);
  void ShutdownBrowserPolicyConnector();

 private:
  // See CreateInstance() and DestoryInstance() above.
  TestingBrowserProcess();
  ~TestingBrowserProcess() override;

  std::unique_ptr<content::NotificationService> notification_service_;
  std::string app_locale_;
  bool is_shutting_down_;

  std::unique_ptr<policy::ChromeBrowserPolicyConnector>
      browser_policy_connector_;
  bool created_browser_policy_connector_ = false;
  std::unique_ptr<network::NetworkQualityTracker> network_quality_tracker_;
  std::unique_ptr<ProfileManager> profile_manager_;
  std::unique_ptr<NotificationUIManager> notification_ui_manager_;
  std::unique_ptr<NotificationPlatformBridge> notification_platform_bridge_;
  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

#if BUILDFLAG(ENABLE_PRINTING)
  std::unique_ptr<printing::PrintJobManager> print_job_manager_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  std::unique_ptr<printing::BackgroundPrintingManager>
      background_printing_manager_;
  scoped_refptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;
#endif

  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  std::unique_ptr<subresource_filter::ContentRulesetService>
      subresource_filter_ruleset_service_;
  std::unique_ptr<optimization_guide::OptimizationGuideService>
      optimization_guide_service_;

  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  // |tab_manager_| is null by default and will be created when
  // GetTabManager() is invoked on supported platforms.
#if !defined(OS_ANDROID)
  std::unique_ptr<resource_coordinator::TabManager> tab_manager_;
  std::unique_ptr<resource_coordinator::TabLifecycleUnitSource>
      tab_lifecycle_unit_source_;
#endif

  // The following objects are not owned by TestingBrowserProcess:
  PrefService* local_state_;
  IOThread* io_thread_;
  net::URLRequestContextGetter* system_request_context_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  rappor::RapporServiceImpl* rappor_service_;

  std::unique_ptr<BrowserProcessPlatformPart> platform_part_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      test_network_connection_tracker_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<MediaFileSystemRegistry> media_file_system_registry_;

  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcess);
};

// RAII (resource acquisition is initialization) for TestingBrowserProcess.
// Allows you to initialize TestingBrowserProcess/NotificationService before
// other member variables.
//
// This can be helpful if you are running a unit test inside the browser_tests
// suite because browser_tests do not make a TestingBrowserProcess for you.
//
// class MyUnitTestRunningAsBrowserTest : public testing::Test {
//  ...stuff...
//  private:
//   TestingBrowserProcessInitializer initializer_;
//   LocalState local_state_;  // Needs a BrowserProcess to initialize.
//   NotificationRegistrar registar_;  // Needs NotificationService.
// };
class TestingBrowserProcessInitializer {
 public:
  TestingBrowserProcessInitializer();
  ~TestingBrowserProcessInitializer();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcessInitializer);
};

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
