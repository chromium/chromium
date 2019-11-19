// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "components/network_time/network_time_tracker.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

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

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif

#if !defined(OS_ANDROID)
#include "components/keep_alive_registry/keep_alive_registry.h"
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
}

// static
void TestingBrowserProcess::DeleteInstance() {
  // g_browser_process must be null during its own destruction.
  BrowserProcess* browser_process = g_browser_process;
  g_browser_process = nullptr;
  delete browser_process;
}

TestingBrowserProcess::TestingBrowserProcess()
    : notification_service_(content::NotificationService::Create()),
      app_locale_("en"),
      platform_part_(new TestingBrowserProcessPlatformPart()) {}

TestingBrowserProcess::~TestingBrowserProcess() {
  EXPECT_FALSE(local_state_);
  ShutdownBrowserPolicyConnector();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionsBrowserClient::Set(nullptr);
  extensions::AppWindowClient::Set(nullptr);
#endif

  content::SetNetworkConnectionTrackerForTesting(nullptr);

  // Destructors for some objects owned by TestingBrowserProcess will use
  // g_browser_process if it is not null, so it must be null before proceeding.
  DCHECK_EQ(static_cast<BrowserProcess*>(nullptr), g_browser_process);
}

void TestingBrowserProcess::Init() {
  test_network_connection_tracker_ =
      network::TestNetworkConnectionTracker::CreateInstance();
  content::SetNetworkConnectionTrackerForTesting(
      test_network_connection_tracker_.get());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions_browser_client_ =
      std::make_unique<extensions::ChromeExtensionsBrowserClient>();
  extensions_browser_client_->AddAPIProvider(
      std::make_unique<chrome_apps::ChromeAppsBrowserAPIProvider>());
  extensions::AppWindowClient::Set(ChromeAppWindowClient::GetInstance());
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
#endif

#if !defined(OS_ANDROID)
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(false);
#endif
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
  return nullptr;
}

rappor::RapporServiceImpl* TestingBrowserProcess::rappor_service() {
  return rappor_service_;
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

WatchDogThread* TestingBrowserProcess::watchdog_thread() {
  return nullptr;
}

ProfileManager* TestingBrowserProcess::profile_manager() {
  return profile_manager_.get();
}

void TestingBrowserProcess::SetProfileManager(ProfileManager* profile_manager) {
  // NotificationUIManager can contain references to elements in the current
  // ProfileManager (for example, the MessageCenterSettingsController maintains
  // a pointer to the ProfileInfoCache). So when we change the ProfileManager
  // (typically during test shutdown) make sure to reset any objects that might
  // maintain references to it. See SetLocalState() for a description of a
  // similar situation.
  notification_ui_manager_.reset();
  profile_manager_.reset(profile_manager);
}

PrefService* TestingBrowserProcess::local_state() {
  return local_state_;
}

variations::VariationsService* TestingBrowserProcess::variations_service() {
  return nullptr;
}

StartupData* TestingBrowserProcess::startup_data() {
  return nullptr;
}

policy::ChromeBrowserPolicyConnector*
TestingBrowserProcess::browser_policy_connector() {
  if (!browser_policy_connector_) {
    EXPECT_FALSE(created_browser_policy_connector_);
    created_browser_policy_connector_ = true;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
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

    browser_policy_connector_ = platform_part_->CreateBrowserPolicyConnector();

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

void TestingBrowserProcess::set_background_mode_manager_for_test(
    std::unique_ptr<BackgroundModeManager> manager) {
  NOTREACHED();
}

StatusTray* TestingBrowserProcess::status_tray() {
  return nullptr;
}

safe_browsing::SafeBrowsingService*
TestingBrowserProcess::safe_browsing_service() {
  return sb_service_.get();
}

safe_browsing::ClientSideDetectionService*
TestingBrowserProcess::safe_browsing_detection_service() {
  return nullptr;
}

subresource_filter::RulesetService*
TestingBrowserProcess::subresource_filter_ruleset_service() {
  return subresource_filter_ruleset_service_.get();
}

optimization_guide::OptimizationGuideService*
TestingBrowserProcess::optimization_guide_service() {
  return optimization_guide_service_.get();
}

BrowserProcessPlatformPart* TestingBrowserProcess::platform_part() {
  return platform_part_.get();
}

extensions::EventRouterForwarder*
TestingBrowserProcess::extension_event_router_forwarder() {
  return nullptr;
}

NotificationUIManager* TestingBrowserProcess::notification_ui_manager() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (!notification_ui_manager_.get())
    notification_ui_manager_ = NotificationUIManager::Create();
  return notification_ui_manager_.get();
#else
  return nullptr;
#endif
}

NotificationPlatformBridge*
TestingBrowserProcess::notification_platform_bridge() {
  return notification_platform_bridge_.get();
}

IntranetRedirectDetector* TestingBrowserProcess::intranet_redirect_detector() {
  return nullptr;
}

void TestingBrowserProcess::CreateDevToolsProtocolHandler() {}

void TestingBrowserProcess::CreateDevToolsAutoOpener() {
}

bool TestingBrowserProcess::IsShuttingDown() {
  return is_shutting_down_;
}

printing::PrintJobManager* TestingBrowserProcess::print_job_manager() {
#if BUILDFLAG(ENABLE_PRINTING)
  if (!print_job_manager_.get())
    print_job_manager_.reset(new printing::PrintJobManager());
  return print_job_manager_.get();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

printing::PrintPreviewDialogController*
TestingBrowserProcess::print_preview_dialog_controller() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  if (!print_preview_dialog_controller_.get())
    print_preview_dialog_controller_ =
        new printing::PrintPreviewDialogController();
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
    background_printing_manager_.reset(
        new printing::BackgroundPrintingManager());
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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
component_updater::SupervisedUserWhitelistInstaller*
TestingBrowserProcess::supervised_user_whitelist_installer() {
  return nullptr;
}
#endif

MediaFileSystemRegistry* TestingBrowserProcess::media_file_system_registry() {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return nullptr;
#else
  if (!media_file_system_registry_)
    media_file_system_registry_.reset(new MediaFileSystemRegistry());
  return media_file_system_registry_.get();
#endif
}

WebRtcLogUploader* TestingBrowserProcess::webrtc_log_uploader() {
  return nullptr;
}

network_time::NetworkTimeTracker*
TestingBrowserProcess::network_time_tracker() {
  if (!network_time_tracker_) {
    DCHECK(local_state_);
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        std::unique_ptr<base::Clock>(new base::DefaultClock()),
        std::unique_ptr<base::TickClock>(new base::DefaultTickClock()),
        local_state_, nullptr));
  }
  return network_time_tracker_.get();
}

gcm::GCMDriver* TestingBrowserProcess::gcm_driver() {
  return nullptr;
}

resource_coordinator::ResourceCoordinatorParts*
TestingBrowserProcess::resource_coordinator_parts() {
  if (!resource_coordinator_parts_) {
    resource_coordinator_parts_ =
        std::make_unique<resource_coordinator::ResourceCoordinatorParts>();
  }
  return resource_coordinator_parts_.get();
}

resource_coordinator::TabManager* TestingBrowserProcess::GetTabManager() {
  return resource_coordinator_parts()->tab_manager();
}

void TestingBrowserProcess::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  shared_url_loader_factory_ = shared_url_loader_factory;
}

void TestingBrowserProcess::SetNotificationUIManager(
    std::unique_ptr<NotificationUIManager> notification_ui_manager) {
  notification_ui_manager_.swap(notification_ui_manager);
}

void TestingBrowserProcess::SetNotificationPlatformBridge(
    std::unique_ptr<NotificationPlatformBridge> notification_platform_bridge) {
  notification_platform_bridge_.swap(notification_platform_bridge);
}

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
    notification_ui_manager_.reset();
    ShutdownBrowserPolicyConnector();
    created_browser_policy_connector_ = false;
  }
  local_state_ = local_state;
}

void TestingBrowserProcess::ShutdownBrowserPolicyConnector() {
  if (browser_policy_connector_)
    browser_policy_connector_->Shutdown();
  browser_policy_connector_.reset();
}

void TestingBrowserProcess::SetSafeBrowsingService(
    safe_browsing::SafeBrowsingService* sb_service) {
  sb_service_ = sb_service;
}

void TestingBrowserProcess::SetRulesetService(
    std::unique_ptr<subresource_filter::RulesetService> ruleset_service) {
  subresource_filter_ruleset_service_.swap(ruleset_service);
}

void TestingBrowserProcess::SetOptimizationGuideService(
    std::unique_ptr<optimization_guide::OptimizationGuideService>
        optimization_guide_service) {
  optimization_guide_service_.swap(optimization_guide_service);
}

void TestingBrowserProcess::SetRapporServiceImpl(
    rappor::RapporServiceImpl* rappor_service) {
  rappor_service_ = rappor_service;
}

void TestingBrowserProcess::SetShuttingDown(bool is_shutting_down) {
  is_shutting_down_ = is_shutting_down;
}

///////////////////////////////////////////////////////////////////////////////

TestingBrowserProcessInitializer::TestingBrowserProcessInitializer() {
  TestingBrowserProcess::CreateInstance();
}

TestingBrowserProcessInitializer::~TestingBrowserProcessInitializer() {
  TestingBrowserProcess::DeleteInstance();
}
