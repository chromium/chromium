// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_file_util.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_util.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/test/base/scoped_bundle_swizzler_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "ui/base/win/atl_module.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#endif

#if !defined(OS_ANDROID)
#include "components/storage_monitor/test_storage_monitor.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/services/device_sync/device_sync_impl.h"
#include "chromeos/services/device_sync/fake_device_sync.h"
#include "components/user_manager/user_names.h"
#include "ui/display/display_switches.h"
#include "ui/events/test/event_generator.h"
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS) && defined(OS_LINUX)
#include "ui/views/test/test_desktop_screen_x11.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/test/views/accessibility_checker.h"
#include "ui/views/views_delegate.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
class FakeDeviceSyncImplFactory
    : public chromeos::device_sync::DeviceSyncImpl::Factory {
 public:
  FakeDeviceSyncImplFactory() = default;
  ~FakeDeviceSyncImplFactory() override = default;

  // chromeos::device_sync::DeviceSyncImpl::Factory:
  std::unique_ptr<chromeos::device_sync::DeviceSyncBase> BuildInstance(
      signin::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      PrefService* profile_prefs,
      const chromeos::device_sync::GcmDeviceInfoProvider*
          gcm_device_info_provider,
      chromeos::device_sync::ClientAppMetadataProvider*
          client_app_metadata_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<base::OneShotTimer> timer) override {
    return std::make_unique<chromeos::device_sync::FakeDeviceSync>();
  }
};

FakeDeviceSyncImplFactory* GetFakeDeviceSyncImplFactory() {
  static base::NoDestructor<FakeDeviceSyncImplFactory> factory;
  return factory.get();
}
#endif  // defined(OS_CHROMEOS)

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

std::unique_ptr<storage::QuotaSettings>
InProcessBrowserTest::CreateQuotaSettings() {
  // By default use hardcoded quota settings to have a consistent testing
  // environment.
  const int kQuota = 5 * 1024 * 1024;
  return std::make_unique<storage::QuotaSettings>(kQuota * 5, kQuota, 0, 0);
}

void InProcessBrowserTest::Initialize() {
  CreateTestServer(GetChromeTestDataDir());
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

  // chrome::DIR_TEST_DATA isn't going to be setup until after we call
  // ContentMain. However that is after tests' constructors or SetUp methods,
  // which sometimes need it. So just override it.
  CHECK(base::PathService::Override(chrome::DIR_TEST_DATA,
                                    src_dir.Append(GetChromeTestDataDir())));

#if defined(OS_MACOSX)
  bundle_swizzler_ = std::make_unique<ScopedBundleSwizzlerMac>();
#endif
}

InProcessBrowserTest::~InProcessBrowserTest() = default;

void InProcessBrowserTest::SetUp() {
  // Browser tests will create their own g_browser_process later.
  DCHECK(!g_browser_process);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Auto-reload breaks many browser tests, which assume error pages won't be
  // reloaded out from under them. Tests that expect or desire this behavior can
  // append switches::kEnableAutoReload, which will override the disable here.
  command_line->AppendSwitch(switches::kDisableAutoReload);

  // Allow subclasses to change the command line before running any tests.
  SetUpCommandLine(command_line);
  // Add command line arguments that are used by all InProcessBrowserTests.
  SetUpDefaultCommandLine(command_line);

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

#if defined(OS_CHROMEOS)
  // No need to redirect log for test.
  command_line->AppendSwitch(switches::kDisableLoggingRedirect);

  // Disable IME extension loading to avoid many browser tests failures.
  chromeos::input_method::DisableExtensionLoading();

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
  // the same as in ChromeBrowserMainPartsChromeos::PreEarlyInitialization
  // but it will be done on device and only for tests.
  if (!command_line->HasSwitch(chromeos::switches::kLoginManager) &&
      !command_line->HasSwitch(chromeos::switches::kLoginUser) &&
      !command_line->HasSwitch(chromeos::switches::kGuestSession)) {
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        cryptohome::Identification(user_manager::StubAccountId()).id());
    if (!command_line->HasSwitch(chromeos::switches::kLoginProfile)) {
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      chrome::kTestUserProfileDir);
    }
  }
#endif

  SetScreenInstance();

  // Use a mocked password storage if OS encryption is used that might block or
  // prompt the user (which is when anything sensitive gets stored, including
  // Cookies). Without this on Mac and Linux, many tests will hang waiting for a
  // user to approve KeyChain/kwallet access. On Windows this is not needed as
  // OS APIs never block.
#if defined(OS_MACOSX) || defined(OS_LINUX)
  OSCryptMocker::SetUp();
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService::set_state_for_testing(
      CaptivePortalService::DISABLED_FOR_TESTING);
#endif

  chrome_browser_net::NetErrorTabHelper::set_state_for_testing(
      chrome_browser_net::NetErrorTabHelper::TESTING_FORCE_DISABLED);

  google_util::SetMockLinkDoctorBaseURLForTesting();

#if defined(OS_CHROMEOS)
  chromeos::device_sync::DeviceSyncImpl::Factory::SetInstanceForTesting(
      GetFakeDeviceSyncImplFactory());

  // On Chrome OS, access to files via file: scheme is restricted. Enable
  // access to all files here since browser_tests and interactive_ui_tests
  // rely on the ability to open any files via file: scheme.
  ChromeNetworkDelegate::EnableAccessToAllFilesForTesting(true);

  // Using a screenshot for clamshell to tablet mode transitions makes the flow
  // async which we want to disable for most tests.
  ash::ShellTestApi::SetTabletControllerUseScreenshotForTest(false);
#endif  // defined(OS_CHROMEOS)

  quota_settings_ = CreateQuotaSettings();
  ChromeContentBrowserClient::SetDefaultQuotaSettingsForTesting(
      quota_settings_.get());

  // Redirect the default download directory to a temporary directory.
  ASSERT_TRUE(default_download_dir_.CreateUniqueTempDir());
  CHECK(base::PathService::Override(chrome::DIR_DEFAULT_DOWNLOADS,
                                    default_download_dir_.GetPath()));

  AfterStartupTaskUtils::DisableScheduleTaskDelayForTesting();

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
}

void InProcessBrowserTest::TearDown() {
  DCHECK(!g_browser_process);
#if defined(OS_WIN)
  com_initializer_.reset();
#endif
  BrowserTestBase::TearDown();
#if defined(OS_MACOSX) || defined(OS_LINUX)
  OSCryptMocker::TearDown();
#endif
  ChromeContentBrowserClient::SetDefaultQuotaSettingsForTesting(nullptr);

#if defined(OS_CHROMEOS)
  chromeos::device_sync::DeviceSyncImpl::Factory::SetInstanceForTesting(
      nullptr);
#endif
}

void InProcessBrowserTest::SelectFirstBrowser() {
  const BrowserList* browser_list = BrowserList::GetInstance();
  if (!browser_list->empty())
    browser_ = browser_list->get(0);
}

void InProcessBrowserTest::CloseBrowserSynchronously(Browser* browser) {
  CloseBrowserAsynchronously(browser);
  ui_test_utils::WaitForBrowserToClose(browser);
}

void InProcessBrowserTest::CloseBrowserAsynchronously(Browser* browser) {
  browser->window()->Close();
#if defined(OS_MACOSX)
  // BrowserWindowController depends on the auto release pool being recycled
  // in the message loop to delete itself.
  AutoreleasePool()->Recycle();
#endif
}

void InProcessBrowserTest::CloseAllBrowsers() {
  chrome::CloseAllBrowsers();
#if defined(OS_MACOSX)
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
void InProcessBrowserTest::AddTabAtIndexToBrowser(
    Browser* browser,
    int index,
    const GURL& url,
    ui::PageTransition transition,
    bool check_navigation_success) {
  NavigateParams params(browser, url, transition);
  params.tabstrip_index = index;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);

  if (check_navigation_success) {
    content::WaitForLoadStop(params.navigated_or_inserted_contents);
  } else {
    content::WaitForLoadStopWithoutSuccessCheck(
        params.navigated_or_inserted_contents);
  }
}

void InProcessBrowserTest::AddTabAtIndex(int index,
                                         const GURL& url,
                                         ui::PageTransition transition) {
  AddTabAtIndexToBrowser(browser(), index, url, transition, true);
}

bool InProcessBrowserTest::SetUpUserDataDirectory() {
  return true;
}

void InProcessBrowserTest::SetScreenInstance() {
#if defined(USE_X11) && !defined(OS_CHROMEOS)
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(
      views::test::TestDesktopScreenX11::GetInstance());
#endif
}

#if !defined(OS_MACOSX)
void InProcessBrowserTest::OpenDevToolsWindow(
    content::WebContents* web_contents) {
  ASSERT_FALSE(content::DevToolsAgentHost::HasFor(web_contents));
  DevToolsWindow::OpenDevToolsWindow(web_contents);
  ASSERT_TRUE(content::DevToolsAgentHost::HasFor(web_contents));
}

Browser* InProcessBrowserTest::OpenURLOffTheRecord(Profile* profile,
                                                   const GURL& url) {
  chrome::OpenURLOffTheRecord(profile, url);
  Browser* browser =
      chrome::FindTabbedBrowser(profile->GetOffTheRecordProfile(), false);
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  observer.Wait();
  return browser;
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile, true));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateIncognitoBrowser(Profile* profile) {
  // Use active profile if default nullptr was passed.
  if (!profile)
    profile = browser()->profile();
  // Create a new browser with using the incognito profile.
  Browser* incognito = new Browser(
      Browser::CreateParams(profile->GetOffTheRecordProfile(), true));
  AddBlankTabAndShow(incognito);
  return incognito;
}

Browser* InProcessBrowserTest::CreateBrowserForPopup(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(Browser::TYPE_POPUP, profile, true));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateBrowserForApp(const std::string& app_name,
                                                   Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams::CreateForApp(
      app_name, false /* trusted_source */, gfx::Rect(), profile, true));
  AddBlankTabAndShow(browser);
  return browser;
}
#endif  // !defined(OS_MACOSX)

void InProcessBrowserTest::AddBlankTabAndShow(Browser* browser) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser, GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  observer.Wait();

  browser->window()->Show();
}

#if !defined(OS_MACOSX)
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
#endif

base::FilePath InProcessBrowserTest::GetChromeTestDataDir() const {
  return base::FilePath(FILE_PATH_LITERAL("chrome/test/data"));
}

void InProcessBrowserTest::PreRunTestOnMainThread() {
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  // Take the ChromeBrowserMainParts' RunLoop to run ourself, when we
  // want to wait for the browser to exit.
  run_loop_ = ChromeBrowserMainParts::TakeRunLoopForTest();

  // Pump startup related events.
  content::RunAllPendingInMessageLoop();

  SelectFirstBrowser();
  if (browser_) {
#if defined(OS_CHROMEOS)
    // There are cases where windows get created maximized by default.
    if (browser_->window()->IsMaximized())
      browser_->window()->Restore();
#endif
    auto* tab = browser_->tab_strip_model()->GetActiveWebContents();
    content::WaitForLoadStop(tab);
    SetInitialWebContents(tab);
  }

#if !defined(OS_ANDROID)
  // Do not use the real StorageMonitor for tests, which introduces another
  // source of variability and potential slowness.
  ASSERT_TRUE(storage_monitor::TestStorageMonitor::CreateForBrowserTests());
#endif

#if defined(OS_MACOSX)
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

#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif
}

void InProcessBrowserTest::PostRunTestOnMainThread() {
#if defined(OS_MACOSX)
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&chrome::OnAppExiting));
    // Spin the message loop to ensure OnAppExitting finishes so that proper
    // clean up happens before returning.
    content::RunAllPendingInMessageLoop();
    return;
  }

  // Invoke AttemptExit on a running message loop.
  // AttemptExit exits the message loop after everything has been
  // shut down properly.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  RunUntilBrowserProcessQuits();

#if defined(OS_MACOSX)
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
  autorelease_pool_ = NULL;
#endif
}
