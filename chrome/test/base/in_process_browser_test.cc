// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <map>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_file_util.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/embedder_support/switches.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/google/core/common/google_util.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/test/base/scoped_bundle_swizzler_mac.h"
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "components/version_info/version_info.h"
#include "ui/base/win/atl_module.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_service.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "components/storage_monitor/test_storage_monitor.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/services/device_sync/device_sync_impl.h"
#include "chromeos/ash/services/device_sync/fake_device_sync.h"
#include "components/user_manager/user_names.h"
#include "ui/display/display_switches.h"
#include "ui/events/test/event_generator.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_OZONE)
#include "ui/views/test/test_desktop_screen_ozone.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/test/views/accessibility_checker.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/base_switches.h"
#include "base/environment.h"
#include "base/files/file_path_watcher.h"
#include "base/process/launch.h"
#include "base/uuid.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"  // nogncheck
#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"  // nogncheck
#include "components/variations/variations_switches.h"
#include "content/public/test/network_connection_change_simulator.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class FakeDeviceSyncImplFactory
    : public ash::device_sync::DeviceSyncImpl::Factory {
 public:
  FakeDeviceSyncImplFactory() = default;
  ~FakeDeviceSyncImplFactory() override = default;

  // ash::device_sync::DeviceSyncImpl::Factory:
  std::unique_ptr<ash::device_sync::DeviceSyncBase> CreateInstance(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      PrefService* profile_prefs,
      const ash::device_sync::GcmDeviceInfoProvider* gcm_device_info_provider,
      ash::device_sync::ClientAppMetadataProvider* client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<base::OneShotTimer> timer,
      ash::device_sync::AttestationCertificatesSyncer::
          GetAttestationCertificatesFunction
              get_attestation_certificates_function) override {
    return std::make_unique<ash::device_sync::FakeDeviceSync>();
  }
};

FakeDeviceSyncImplFactory* GetFakeDeviceSyncImplFactory() {
  static base::NoDestructor<FakeDeviceSyncImplFactory> factory;
  return factory.get();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
class ChromeBrowserMainExtraPartsBrowserProcessInjection
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsBrowserProcessInjection() = default;

  // ChromeBrowserMainExtraParts implementation
  void PreCreateMainMessageLoop() override {
    // The real GeolocationManager initializes a CLLocationManager. It has
    // been observed that when thousands of instances of this object are
    // created, as happens when running browser tests, the CoreLocationAgent
    // process uses lots of CPU. This makes test execution slower and causes
    // jobs to time out. We therefore insert a fake.
    auto fake_geolocation_manager =
        std::make_unique<device::FakeGeolocationManager>();
    fake_geolocation_manager->SetSystemPermission(
        device::LocationSystemPermissionStatus::kAllowed);
    device::GeolocationManager::SetInstance(
        std::move(fake_geolocation_manager));
  }

  ChromeBrowserMainExtraPartsBrowserProcessInjection(
      const ChromeBrowserMainExtraPartsBrowserProcessInjection&) = delete;
  ChromeBrowserMainExtraPartsBrowserProcessInjection& operator=(
      const ChromeBrowserMainExtraPartsBrowserProcessInjection&) = delete;
};
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// For browser tests that depend on AccountManager on Lacros - e.g. tests that
// manage accounts by calling methods like `signin::MakePrimaryAccountAvailable`
// from identity_test_utils.
// TODO(https://crbug.com/982233): consider using this class on Ash, and remove
// the initialization from profile_impl.
class IdentityExtraSetUp : public ChromeBrowserMainExtraParts {
 public:
  void PreProfileInit() override {
    // Create and initialize Ash AccountManager.
    scoped_ash_account_manager_ =
        std::make_unique<ScopedAshAccountManagerForTests>(
            std::make_unique<FakeAccountManagerUI>());
    auto* account_manager = MaybeGetAshAccountManagerForTests();
    CHECK(account_manager);
    account_manager->InitializeInEphemeralMode(
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory());

    // Make sure the primary accounts for all profiles are present in the
    // account manager, to prevent profiles from being deleted. This is useful
    // in particular for tests that create profiles in a PRE_ step and expect
    // the profiles to still exist when Chrome is restarted.
    ProfileAttributesStorage* storage =
        &g_browser_process->profile_manager()->GetProfileAttributesStorage();
    for (const ProfileAttributesEntry* entry :
         storage->GetAllProfilesAttributes()) {
      const std::string& gaia_id = entry->GetGAIAId();
      if (!gaia_id.empty()) {
        account_manager->UpsertAccount(
            {gaia_id, account_manager::AccountType::kGaia},
            base::UTF16ToUTF8(entry->GetUserName()),
            "identity_extra_setup_test_token");
      }
    }
  }

 private:
  std::unique_ptr<ScopedAshAccountManagerForTests> scoped_ash_account_manager_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void EnsureBrowserContextKeyedServiceFactoriesForTestingBuilt() {
  NotificationDisplayServiceTester::EnsureFactoryBuilt();
}

InProcessBrowserTest* g_current_test;

}  // namespace

// static
InProcessBrowserTest::SetUpBrowserFunction*
    InProcessBrowserTest::global_browser_set_up_function_ = nullptr;

InProcessBrowserTest::InProcessBrowserTest() {
  Initialize();
#if defined(TOOLKIT_VIEWS)
  views_delegate_ = std::make_unique<AccessibilityChecker>();
#endif
}

#if defined(TOOLKIT_VIEWS)
InProcessBrowserTest::InProcessBrowserTest(
    std::unique_ptr<views::ViewsDelegate> views_delegate) {
  Initialize();
  views_delegate_ = std::move(views_delegate);
}
#endif

void InProcessBrowserTest::RunScheduledLayouts() {
#if defined(TOOLKIT_VIEWS)
  views::Widget::Widgets widgets_to_layout;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // WidgetTest::GetAllWidgets() doesn't work for ChromeOS in a production
  // environment. We must get the Widgets ourself.
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows())
    views::Widget::GetAllChildWidgets(root_window, &widgets_to_layout);
#else
  widgets_to_layout = views::test::WidgetTest::GetAllWidgets();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  for (views::Widget* widget : widgets_to_layout)
    widget->LayoutRootViewIfNecessary();
#endif  // defined(TOOLKIT_VIEWS)
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
FakeAccountManagerUI* InProcessBrowserTest::GetFakeAccountManagerUI() const {
  return static_cast<FakeAccountManagerUI*>(
      MaybeGetAshAccountManagerUIForTests());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void InProcessBrowserTest::Initialize() {
  g_current_test = this;
  CreateTestServer(GetChromeTestDataDir());
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

  // chrome::DIR_TEST_DATA isn't going to be setup until after we call
  // ContentMain. However that is after tests' constructors or SetUp methods,
  // which sometimes need it. So just override it.
  CHECK(base::PathService::Override(chrome::DIR_TEST_DATA,
                                    src_dir.Append(GetChromeTestDataDir())));

#if BUILDFLAG(IS_MAC)
  bundle_swizzler_ = std::make_unique<ScopedBundleSwizzlerMac>();
#endif

  std::vector<base::test::FeatureRef> disabled_features;

  // Preconnecting can cause non-deterministic test behavior especially with
  // various test fixtures that mock servers.
  disabled_features.push_back(features::kPreconnectToSearch);

  // If the network service fails to start sandboxed then this should cause
  // tests to fail.
  disabled_features.push_back(
      features::kRestartNetworkServiceUnsandboxedForFailedLaunch);

  // In-product help can conflict with tests' expected window activation and
  // focus. Individual tests can re-enable IPH.
  block_all_iph_feature_list_.InitWithNoFeaturesAllowed();

  scoped_feature_list_.InitWithFeatures({}, disabled_features);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  launch_browser_for_testing_ =
      std::make_unique<ash::full_restore::ScopedLaunchBrowserForTesting>();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  CertDbInitializerFactory::GetInstance()
      ->SetCreateWithBrowserContextForTesting(/*should_create=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

InProcessBrowserTest::~InProcessBrowserTest() {
  g_current_test = nullptr;
}

InProcessBrowserTest* InProcessBrowserTest::GetCurrent() {
  return g_current_test;
}

void InProcessBrowserTest::SetUp() {
  // Browser tests will create their own g_browser_process later.
  DCHECK(!g_browser_process);

  ui_controls::ResetUIControlsIfEnabled();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Auto-reload breaks many browser tests, which assume error pages won't be
  // reloaded out from under them. Tests that expect or desire this behavior can
  // append embedder_support::kEnableAutoReload, which will override the disable
  // here.
  command_line->AppendSwitch(embedder_support::kDisableAutoReload);

  // Allow subclasses to change the command line before running any tests.
  SetUpCommandLine(command_line);
  // Add command line arguments that are used by all InProcessBrowserTests.
  SetUpDefaultCommandLine(command_line);

  // PoissonAllocationSampler's TLS slots need to be set up before
  // MainThreadStackSamplingProfiler, which can allocate TLS slots of its own.
  // On some platforms pthreads can malloc internally to access higher-numbered
  // TLS slots, which can cause reentry in the heap profiler. (See the comment
  // on ReentryGuard::InitTLSSlot().)
  // TODO(https://crbug.com/1411454): Clean up other paths that call this Init()
  // function, which are now redundant.
  base::PoissonAllocationSampler::Init();

  // Initialize sampling profiler in browser tests. This mimics the behavior
  // in standalone Chrome, where this is done in chrome/app/chrome_main.cc,
  // which does not get called by browser tests.
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();

  // Create a temporary user data directory if required.
  ASSERT_TRUE(test_launcher_utils::CreateUserDataDir(&temp_user_data_dir_))
      << "Could not create user data directory.";

  // Allow subclasses the opportunity to make changes to the default user data
  // dir before running any tests.
  ASSERT_TRUE(SetUpUserDataDirectory())
      << "Could not set up user data directory.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // No need to redirect log for test.
  command_line->AppendSwitch(switches::kDisableLoggingRedirect);

  // Disable IME extension loading to avoid many browser tests failures.
  ash::input_method::DisableExtensionLoading();

  if (!command_line->HasSwitch(switches::kHostWindowBounds) &&
      !base::SysInfo::IsRunningOnChromeOS()) {
    // Adjusting window location & size so that the ash desktop window fits
    // inside the Xvfb's default resolution. Only do that when not running
    // on device. Otherwise, device display is not properly configured.
    command_line->AppendSwitchASCII(switches::kHostWindowBounds,
                                    "0+0-1280x800");
  }

  // Default to run in a signed in session of stub user if tests do not run
  // in the login screen (--login-manager), or logged in user session
  // (--login-user), or the guest session (--bwsi). This is essentially
  // the same as in `ChromeBrowserMainPartsAsh::PreEarlyInitialization`
  // but it will be done on device and only for tests.
  if (!command_line->HasSwitch(ash::switches::kLoginManager) &&
      !command_line->HasSwitch(ash::switches::kLoginUser) &&
      !command_line->HasSwitch(ash::switches::kGuestSession)) {
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        cryptohome::Identification(user_manager::StubAccountId()).id());
    if (!command_line->HasSwitch(ash::switches::kLoginProfile)) {
      command_line->AppendSwitchASCII(
          ash::switches::kLoginProfile,
          ash::BrowserContextHelper::kTestUserBrowserContextDirName);
    }
  }
#endif

  SetScreenInstance();

  // Use a mocked password storage if OS encryption is used that might block or
  // prompt the user (which is when anything sensitive gets stored, including
  // Cookies). Without this on Mac and Linux, many tests will hang waiting for a
  // user to approve KeyChain/kwallet access. On Windows this is not needed as
  // OS APIs never block.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  OSCryptMocker::SetUp();
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalService::set_state_for_testing(
      captive_portal::CaptivePortalService::DISABLED_FOR_TESTING);
#endif

  chrome_browser_net::NetErrorTabHelper::set_state_for_testing(
      chrome_browser_net::NetErrorTabHelper::TESTING_FORCE_DISABLED);

#if BUILDFLAG(IS_CHROMEOS)
  // On Chrome OS, access to files via file: scheme is restricted. Enable
  // access to all files here since browser_tests and interactive_ui_tests
  // rely on the ability to open any files via file: scheme.
  ChromeNetworkDelegate::EnableAccessToAllFilesForTesting(true);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Device sync (for multidevice "Better Together") is ash specific.
  ash::device_sync::DeviceSyncImpl::Factory::SetCustomFactory(
      GetFakeDeviceSyncImplFactory());

  // Using a screenshot for clamshell to tablet mode transitions makes the flow
  // async which we want to disable for most tests.
  ash::ShellTestApi::SetTabletControllerUseScreenshotForTest(false);

  // Disable the notification delay timer used to prevent non system
  // notifications from showing up right after login.
  ash::ShellTestApi::SetUseLoginNotificationDelayForTest(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Redirect the default download directory to a temporary directory.
  ASSERT_TRUE(default_download_dir_.CreateUniqueTempDir());
  CHECK(base::PathService::Override(chrome::DIR_DEFAULT_DOWNLOADS,
                                    default_download_dir_.GetPath()));

#if defined(TOOLKIT_VIEWS)
  // Prevent hover cards from appearing when the mouse is over the tab. Tests
  // don't typically account for this possibly, so it can cause unrelated tests
  // to fail. See crbug.com/1050012.
  Tab::SetShowHoverCardOnMouseHoverForTesting(false);
#endif  // defined(TOOLKIT_VIEWS)

  // Auto-redirect to the NTP, which can happen if remote content is enabled on
  // What's New for tests that simulate first run, is unexpected by most tests.
  whats_new::DisableRemoteContentForTests();

  // The Privacy Sandbox service may attempt to show a modal prompt to the
  // profile on browser start, which is unexpected by mosts tests. Tests which
  // expect this can allow the prompt as desired.
  PrivacySandboxService::SetPromptDisabledForTests(true);

  EnsureBrowserContextKeyedServiceFactoriesForTestingBuilt();

  BrowserTestBase::SetUp();
}

void InProcessBrowserTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);
  test_launcher_utils::PrepareBrowserCommandLineForBrowserTests(
      command_line, open_about_blank_on_browser_launch_);

  // TODO(pkotwicz): Investigate if we can remove this switch.
  if (exit_when_last_browser_closes_)
    command_line->AppendSwitch(switches::kDisableZeroBrowsersOpenForTests);
#if BUILDFLAG(IS_CHROMEOS)
  // Do not automaximize in browser tests.
  command_line->AppendSwitch(switches::kDisableAutoMaximizeForTests);
#endif
}

void InProcessBrowserTest::TearDown() {
  DCHECK(!g_browser_process);
#if BUILDFLAG(IS_WIN)
  com_initializer_.reset();
#endif
  BrowserTestBase::TearDown();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  OSCryptMocker::TearDown();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::device_sync::DeviceSyncImpl::Factory::SetCustomFactory(nullptr);
  launch_browser_for_testing_ = nullptr;
#endif
}

// static
size_t InProcessBrowserTest::GetTestPreCount() {
  constexpr base::StringPiece kPreTestPrefix = "PRE_";
  base::StringPiece test_name =
      testing::UnitTest::GetInstance()->current_test_info()->name();
  size_t count = 0;
  while (base::StartsWith(test_name, kPreTestPrefix)) {
    ++count;
    test_name = test_name.substr(kPreTestPrefix.size());
  }
  return count;
}

void InProcessBrowserTest::CreatedBrowserMainParts(
    content::BrowserMainParts* parts) {
  BrowserTestBase::CreatedBrowserMainParts(parts);
#if BUILDFLAG(IS_MAC)
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsBrowserProcessInjection>());
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<IdentityExtraSetUp>());
#endif
}

void InProcessBrowserTest::SelectFirstBrowser() {
  const BrowserList* browser_list = BrowserList::GetInstance();
  if (!browser_list->empty())
    browser_ = browser_list->get(0);
}

void InProcessBrowserTest::RecordPropertyFromMap(
    const std::map<std::string, std::string>& tags) {
  std::string result = "";
  for (auto const& tag_pair : tags) {
    // Make sure the key value pair does not contain  ; and = characters.
    DCHECK(tag_pair.first.find(";") == std::string::npos &&
           tag_pair.first.find("=") == std::string::npos);
    DCHECK(tag_pair.second.find(";") == std::string::npos &&
           tag_pair.second.find("=") == std::string::npos);
    if (!result.empty())
      result = base::StrCat({result, ";"});
    result = base::StrCat({result, tag_pair.first, "=", tag_pair.second});
  }
  if (!result.empty())
    RecordProperty("gtest_tag", result);
}

void InProcessBrowserTest::SetUpLocalStatePrefService(
    PrefService* local_state) {
#if BUILDFLAG(IS_WIN)
  // Put the current build version number in the prefs, so that pinned taskbar
  // icons aren't migrated.
  local_state->SetString(prefs::kShortcutMigrationVersion,
                         std::string(version_info::GetVersionNumber()));
#endif  // BUILDFLAG(IS_WIN);
}

void InProcessBrowserTest::CloseBrowserSynchronously(Browser* browser) {
  CloseBrowserAsynchronously(browser);
  ui_test_utils::WaitForBrowserToClose(browser);
}

void InProcessBrowserTest::CloseBrowserAsynchronously(Browser* browser) {
  browser->window()->Close();
#if BUILDFLAG(IS_MAC)
  // BrowserWindowController depends on the auto release pool being recycled
  // in the message loop to delete itself.
  AutoreleasePool()->Recycle();
#endif
}

void InProcessBrowserTest::CloseAllBrowsers() {
  chrome::CloseAllBrowsers();
#if BUILDFLAG(IS_MAC)
  // BrowserWindowController depends on the auto release pool being recycled
  // in the message loop to delete itself.
  AutoreleasePool()->Recycle();
#endif
}

void InProcessBrowserTest::RunUntilBrowserProcessQuits() {
  std::exchange(run_loop_, nullptr)->Run();
}

// TODO(alexmos): This function should expose success of the underlying
// navigation to tests, which should make sure navigations succeed when
// appropriate. See https://crbug.com/425335
bool InProcessBrowserTest::AddTabAtIndexToBrowser(
    Browser* browser,
    int index,
    const GURL& url,
    ui::PageTransition transition,
    bool check_navigation_success) {
  return AddTabAtIndexToBrowser(browser, index, url, transition);
}

bool InProcessBrowserTest::AddTabAtIndexToBrowser(
    Browser* browser,
    int index,
    const GURL& url,
    ui::PageTransition transition) {
  NavigateParams params(browser, url, transition);
  params.tabstrip_index = index;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);

  return content::WaitForLoadStop(params.navigated_or_inserted_contents);
}

bool InProcessBrowserTest::AddTabAtIndex(int index,
                                         const GURL& url,
                                         ui::PageTransition transition) {
  return AddTabAtIndexToBrowser(browser(), index, url, transition, true);
}

bool InProcessBrowserTest::SetUpUserDataDirectory() {
  return true;
}

void InProcessBrowserTest::SetScreenInstance() {
  // TODO(crbug.com/1317416): On wayland platform, we need to check if the
  // wayland-ozone platform is initialized at this point due to the async
  // initialization of the display. Investigate if we can eliminate
  // IsOzoneInitialized.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!display::Screen::HasScreen() &&
      views::test::TestDesktopScreenOzone::IsOzoneInitialized()) {
    // This is necessary for interactive UI tests.
    // It is enabled in interactive_ui_tests_main.cc
    // (or through GPUMain)
    screen_ = views::test::TestDesktopScreenOzone::Create();
  }
#endif
}

#if !BUILDFLAG(IS_MAC)
void InProcessBrowserTest::OpenDevToolsWindow(
    content::WebContents* web_contents) {
  ASSERT_FALSE(content::DevToolsAgentHost::HasFor(web_contents));
  DevToolsWindow::OpenDevToolsWindow(web_contents);
  ASSERT_TRUE(content::DevToolsAgentHost::HasFor(web_contents));
}

Browser* InProcessBrowserTest::OpenURLOffTheRecord(Profile* profile,
                                                   const GURL& url) {
  chrome::OpenURLOffTheRecord(profile, url);
  Browser* browser = chrome::FindTabbedBrowser(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true), false);
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  observer.Wait();
  return browser;
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateIncognitoBrowser(Profile* profile) {
  // Use active profile if default nullptr was passed.
  if (!profile)
    profile = browser()->profile();
  // Create a new browser with using the incognito profile.
  Browser* incognito = Browser::Create(Browser::CreateParams(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true), true));
  AddBlankTabAndShow(incognito);
  return incognito;
}

Browser* InProcessBrowserTest::CreateBrowserForPopup(Profile* profile) {
  Browser* browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, profile, true));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateBrowserForApp(const std::string& app_name,
                                                   Profile* profile) {
  Browser* browser = Browser::Create(Browser::CreateParams::CreateForApp(
      app_name, false /* trusted_source */, gfx::Rect(), profile, true));
  AddBlankTabAndShow(browser);
  return browser;
}
#endif  // !BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
Browser* InProcessBrowserTest::CreateGuestBrowser() {
  // Get Guest profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath guest_path = profile_manager->GetGuestProfilePath();

  Profile& guest_profile =
      profiles::testing::CreateProfileSync(profile_manager, guest_path);
  Profile* guest_profile_otr =
      guest_profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // Create browser and add tab.
  Browser* browser =
      Browser::Create(Browser::CreateParams(guest_profile_otr, true));
  AddBlankTabAndShow(browser);
  return browser;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

void InProcessBrowserTest::AddBlankTabAndShow(Browser* browser) {
  content::WebContents* blank_tab = chrome::AddSelectedTabWithURL(
      browser, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  content::TestNavigationObserver observer(blank_tab);
  observer.Wait();

  browser->window()->Show();
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS)
base::CommandLine InProcessBrowserTest::GetCommandLineForRelaunch() {
  base::CommandLine new_command_line(
      base::CommandLine::ForCurrentProcess()->GetProgram());
  base::CommandLine::SwitchMap switches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();
  switches.erase(switches::kUserDataDir);
  switches.erase(switches::kSingleProcessTests);
  switches.erase(switches::kSingleProcess);
  new_command_line.AppendSwitch(switches::kLaunchAsBrowser);

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  new_command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  for (base::CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }
  return new_command_line;
}
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS)

base::FilePath InProcessBrowserTest::GetChromeTestDataDir() const {
  return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
}

void InProcessBrowserTest::PreRunTestOnMainThread() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  content::NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.InitializeChromeosConnectionType();
#endif

  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  // Take the ChromeBrowserMainParts' RunLoop to run ourself, when we
  // want to wait for the browser to exit.
  run_loop_ = ChromeBrowserMainParts::TakeRunLoopForTest();

  // Pump startup related events.
  content::RunAllPendingInMessageLoop();

  SelectFirstBrowser();
  if (browser_ && !browser_->tab_strip_model()->empty()) {
    base::WeakPtr<content::WebContents> tab =
        browser_->tab_strip_model()->GetActiveWebContents()->GetWeakPtr();
    content::WaitForLoadStop(tab.get());
    if (tab) {
      SetInitialWebContents(tab.get());
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  // Do not use the real StorageMonitor for tests, which introduces another
  // source of variability and potential slowness.
  ASSERT_TRUE(storage_monitor::TestStorageMonitor::CreateForBrowserTests());
#endif

#if BUILDFLAG(IS_MAC)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  autorelease_pool_ = new base::mac::ScopedNSAutoreleasePool;
#endif

  // Pump any pending events that were created as a result of creating a
  // browser.
  content::RunAllPendingInMessageLoop();

  if (browser_ && global_browser_set_up_function_)
    ASSERT_TRUE(global_browser_set_up_function_(browser_));

#if BUILDFLAG(IS_MAC)
  autorelease_pool_->Recycle();
#endif
}

void InProcessBrowserTest::PostRunTestOnMainThread() {
#if BUILDFLAG(IS_MAC)
  autorelease_pool_->Recycle();
#endif

  // Sometimes tests leave Quit tasks in the MessageLoop (for shame), so let's
  // run all pending messages here to avoid preempting the QuitBrowsers tasks.
  // TODO(https://crbug.com/922118): Remove this once it is no longer possible
  // to post QuitCurrent* tasks.
  content::RunAllPendingInMessageLoop();

  QuitBrowsers();

  // BrowserList should be empty at this point.
  CHECK(BrowserList::GetInstance()->empty());
}

void InProcessBrowserTest::QuitBrowsers() {
  if (chrome::GetTotalBrowserCount() == 0) {
    browser_shutdown::NotifyAppTerminating();

    // Post OnAppExiting call as a task because the code path CHECKs a RunLoop
    // runs at the current thread.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::OnAppExiting));
    // Spin the message loop to ensure OnAppExitting finishes so that proper
    // clean up happens before returning.
    content::RunAllPendingInMessageLoop();
    return;
  }

  // Invoke AttemptExit on a running message loop.
  // AttemptExit exits the message loop after everything has been
  // shut down properly.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  RunUntilBrowserProcessQuits();

#if BUILDFLAG(IS_MAC)
  // chrome::AttemptExit() will attempt to close all browsers by deleting
  // their tab contents. The last tab contents being removed triggers closing of
  // the browser window.
  //
  // On the Mac, this eventually reaches
  // -[BrowserWindowController windowWillClose:], which will post a deferred
  // -autorelease on itself to ultimately destroy the Browser object. The line
  // below is necessary to pump these pending messages to ensure all Browsers
  // get deleted.
  content::RunAllPendingInMessageLoop();
  delete autorelease_pool_;
  autorelease_pool_ = nullptr;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void InProcessBrowserTest::StartUniqueAshChrome(
    const std::vector<std::string>& enabled_features,
    const std::vector<std::string>& disabled_features,
    const std::vector<std::string>& additional_cmdline_switches,
    const std::string& bug_number_and_reason) {
  DCHECK(!bug_number_and_reason.empty());
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  CHECK(!cmdline->GetSwitchValuePath("lacros-mojo-socket-for-testing").empty())
      << "You can only start unique ash chrome when crosapi is enabled. "
      << "It should not be necessary otherwise.";
  base::FilePath ash_dir_holder = cmdline->GetSwitchValuePath("unique-ash-dir");
  CHECK(!ash_dir_holder.empty());
  CHECK(unique_ash_user_data_dir_.CreateUniqueTempDirUnderPath(ash_dir_holder));
  base::FilePath socket_file =
      unique_ash_user_data_dir_.GetPath().Append("lacros.sock");

  // Reset the current test runner connecting to the unique ash chrome.
  cmdline->RemoveSwitch("lacros-mojo-socket-for-testing");
  cmdline->AppendSwitchPath("lacros-mojo-socket-for-testing", socket_file);
  // Need unique socket name for wayland globally. So for each ash and lacros
  // pair, they have a unique socket to communicate.
  base::Environment::Create()->SetVar(
      "WAYLAND_DISPLAY",
      base::JoinString({"unique_wayland",
                        base::Uuid::GenerateRandomV4().AsLowercaseString()},
                       "_"));

  base::FilePath ash_chrome_path =
      cmdline->GetSwitchValuePath("ash-chrome-path");
  CHECK(!ash_chrome_path.empty());
  base::CommandLine ash_cmdline(ash_chrome_path);
  ash_cmdline.AppendSwitchPath(switches::kUserDataDir,
                               unique_ash_user_data_dir_.GetPath());
  ash_cmdline.AppendSwitch("enable-wayland-server");
  ash_cmdline.AppendSwitch(switches::kNoStartupWindow);
  ash_cmdline.AppendSwitch("disable-lacros-keep-alive");
  ash_cmdline.AppendSwitch("disable-login-lacros-opening");
  ash_cmdline.AppendSwitch(
      variations::switches::kEnableFieldTrialTestingConfig);
  for (const std::string& cmdline_switch : additional_cmdline_switches) {
    size_t pos = cmdline_switch.find("=");
    if (pos == std::string::npos) {
      ash_cmdline.AppendSwitch(cmdline_switch);
    } else {
      CHECK_GT(pos, 0u);
      ash_cmdline.AppendSwitchASCII(cmdline_switch.substr(0, pos),
                                    cmdline_switch.substr(pos + 1));
    }
  }

  std::vector<std::string> all_enabled_features = {
      "LacrosSupport", "LacrosPrimary", "LacrosOnly"};
  all_enabled_features.insert(all_enabled_features.end(),
                              enabled_features.begin(),
                              enabled_features.end());
  ash_cmdline.AppendSwitchASCII(switches::kEnableFeatures,
                                base::JoinString(all_enabled_features, ","));
  ash_cmdline.AppendSwitchASCII(switches::kDisableFeatures,
                                base::JoinString(disabled_features, ","));

  ash_cmdline.AppendSwitchPath("lacros-mojo-socket-for-testing", socket_file);
  std::string wayland_socket;
  CHECK(
      base::Environment::Create()->GetVar("WAYLAND_DISPLAY", &wayland_socket));
  DCHECK(!wayland_socket.empty());
  ash_cmdline.AppendSwitchASCII("wayland-server-socket", wayland_socket);
  const base::FilePath ash_ready_file =
      unique_ash_user_data_dir_.GetPath().AppendASCII("ash_ready.txt");
  ash_cmdline.AppendSwitchPath("ash-ready-file-path", ash_ready_file);

  // Need this for RunLoop. See
  // //docs/threading_and_tasks_testing.md#basetestsinglethreadtaskenvironment
  base::test::SingleThreadTaskEnvironment task_environment;
  base::FilePathWatcher watcher;
  base::RunLoop run_loop;
  CHECK(watcher.Watch(base::FilePath(ash_ready_file),
                      base::FilePathWatcher::Type::kNonRecursive,
                      base::BindLambdaForTesting(
                          [&](const base::FilePath& filepath, bool error) {
                            CHECK(!error);
                            run_loop.Quit();
                          })));
  base::LaunchOptions option;
  ash_process_ = base::LaunchProcess(ash_cmdline, option);
  CHECK(ash_process_.IsValid());
  run_loop.Run();
  // When ash is ready and crosapi was enabled, we expect mojo socket is
  // also ready.
  CHECK(base::PathExists(socket_file));
  LOG(INFO) << "Successfully started a unique ash chrome.";
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
