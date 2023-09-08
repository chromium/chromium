// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_full_keyboard_access.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace base {

class CommandLine;

#if BUILDFLAG(IS_MAC)
namespace apple {
class ScopedNSAutoreleasePool;
}  // namespace apple
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
namespace win {
class ScopedCOMInitializer;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class Process;
class Version;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace base

#if defined(TOOLKIT_VIEWS)
namespace views {
class ViewsDelegate;
}
#endif  // defined(TOOLKIT_VIEWS)

namespace display {
class Screen;
}

class Browser;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
class FakeAccountManagerUI;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
class MainThreadStackSamplingProfiler;
class PrefService;
class Profile;
#if BUILDFLAG(IS_MAC)
class ScopedBundleSwizzlerMac;
#endif  // BUILDFLAG(IS_MAC)

// Base class for tests that bring up Browser instances.
// Writing tests with InProcessBrowserTest is slightly different than that of
// other tests. This is necessitated by InProcessBrowserTest running a message
// loop. To use InProcessBrowserTest do the following:
// . Use the macro IN_PROC_BROWSER_TEST_F to define your test.
// . Your test method is invoked on the ui thread. If you need to block until
//   state changes you'll need to run the message loop from your test method.
//   For example, if you need to wait till a find bar has completely been shown
//   you'll need to invoke content::RunMessageLoop(). When the message bar is
//   shown, invoke RunLoop::QuitCurrentWhenIdleDeprecated() to return control
//   back to your test method.
// . If you subclass and override SetUp(), be sure and invoke
//   InProcessBrowserTest::SetUp(). (But see also BrowserTestBase's
//   SetUpOnMainThread(), SetUpInProcessBrowserTestFixture(), and other related
//   methods for a cleaner alternative).
//
// To include the default implementation of RunTestOnMainThread() and TestBody()
// for Gtests, it's also necessary to include the file
// "content/public/test/browser_test.h"
//
// The following hook methods are called in sequence before BrowserMain(), so
// no browser has been created yet. They are mainly for setting up the
// environment for running the browser.
// . SetUpCommandLine()
// . SetUpDefaultCommandLine()
// . SetUpUserDataDirectory()
//
// Default command line switches are added in the default implementation of
// SetUpDefaultCommandLine(). Additional command line switches can be simply
// appended in SetUpCommandLine() without the need to invoke
// InProcessBrowserTest::SetUpCommandLine(). If a test needs to change the
// default command line, it can override SetUpDefaultCommandLine(), where it
// should invoke InProcessBrowserTest::SetUpDefaultCommandLine() to get the
// default switches, and modify them as needed.
//
// SetUpOnMainThread() is called just after creating the default browser object
// and before executing the real test code. It's mainly for setting up things
// related to the browser object and associated window, like opening a new Tab
// with a testing page loaded.
//
// TearDownOnMainThread() is called just after executing the real test code to
// do necessary clean-up before the browser is torn down.
//
// TearDownInProcessBrowserTestFixture() is called after BrowserMain() exits to
// clean up things set up for running the browser.
//
// By default a single Browser is created in BrowserMain(). You can obviously
// create more as needed.

// See ui_test_utils for a handful of methods designed for use with this class.
//
// It's possible to write browser tests that span a restart by splitting each
// run of the browser process into a separate test. Example:
//
// IN_PROC_BROWSER_TEST_F(Foo, PRE_Bar) {
//   do something
// }
//
// IN_PROC_BROWSER_TEST_F(Foo, Bar) {
//   verify something persisted from before
// }
//
//  This is recursive, so PRE_PRE_Bar would run before PRE_BAR.
class InProcessBrowserTest : public content::BrowserTestBase {
 public:
  InProcessBrowserTest();

#if defined(TOOLKIT_VIEWS)
  // |views_delegate| is used for tests that want to use a derived class of
  // ViewsDelegate to observe or modify things like window placement and Widget
  // params.
  explicit InProcessBrowserTest(
      std::unique_ptr<views::ViewsDelegate> views_delegate);
#endif
  InProcessBrowserTest(const InProcessBrowserTest&) = delete;
  InProcessBrowserTest& operator=(const InProcessBrowserTest&) = delete;
  ~InProcessBrowserTest() override;

  // Returns the currently running InProcessBrowserTest.
  static InProcessBrowserTest* GetCurrent();

  // Configures everything for an in process browser test, then invokes
  // BrowserMain(). BrowserMain() ends up invoking RunTestOnMainThreadLoop().
  void SetUp() override;

  // Restores state configured in SetUp().
  void TearDown() override;

  using SetUpBrowserFunction = bool(const Browser*);

  // Sets a function that is called from InProcessBrowserTest::SetUp() with the
  // first browser. This is intended to set up state applicable to all tests
  // in the suite. For example, interactive_ui_tests installs a function that
  // ensures the first browser is in the foreground, active and has focus.
  static void set_global_browser_set_up_function(
      SetUpBrowserFunction* set_up_function) {
    global_browser_set_up_function_ = set_up_function;
  }

  // Counts the number of "PRE_" prefixes in the test name. This is used to
  // differentiate between different PRE tests in browser test constructors
  // and setup functions.
  static size_t GetTestPreCount();

  // Returns the browser created by BrowserMain().
  // If no browser is created in BrowserMain(), this will return nullptr unless
  // another browser instance is created at a later time and
  // SelectFirstBrowser() is called.
  Browser* browser() const { return browser_; }

  // Set |browser_| to the first browser on the browser list.
  // Call this when your test subclass wants to access a non-null browser
  // instance through browser() but browser creation is delayed until after
  // PreRunTestOnMainThread().
  void SelectFirstBrowser();

  // This function is used to record a set of properties for a test case in
  // gtest result and that will be used by resultDB. The map's key value pair
  // are defined by each test case. For use case check this bug:
  // https://crbug.com/1365899
  // The final value of the result is the format of key1=value1;key2=value2.
  void RecordPropertyFromMap(const std::map<std::string, std::string>& tags);

  // Tests can override this to customize the initial local_state.
  virtual void SetUpLocalStatePrefService(PrefService* local_state);

  // Start ash-chrome with specific flags.
  // In general, there is a shared ash chrome started and a lacros chrome
  // started before a test case. But for some tests, you may need a special
  // ash chrome. 2 common use cases:
  //   1. you need to enable a feature in ash chrome then your test can verify
  //      some behavior. In this case you need to call this function to start
  //      a unique ash chrome with the feature enabled.
  //   2. your test case will pollute ash and cause following test cases fail or
  //      flaky. Instead of implementing cleanup in TearDown(), using a
  //      unique ash just for the test is better.
  // Call this function in the test SetUp() function before invoking
  // InProcessBrowserTest::SetUp().
  // This function has negative performance impact:
  //   1. Start additional ash chrome uses more time.
  //   2. Additional ash chrome uses more resources.
  //      The shared ash chrome is still running. By calling this function,
  //      you start another ash chrome.
  // Args:
  //   enabled_features: Additional features to be enabled in ash chrome.
  //   disabled_features: Additional features to be disabled in ash chrome.
  //   additional_cmdline_switches: Additional cmdline switches.
  //       e.g. {"enable-pixel-outputs-in-tests"}
  //   bug_number_and_reason: Not used in code. But please provide information
  //       about why you need unique ash chrome. Hopefully this can help reduce
  //       the usage of unique ash.
  //       e.g. "crbug.com/11. Switch to shared ash when feature XX is default."
  //
  // After you call this function in SetUp(), before the test case test body,
  // a unique ash chrome will be started and a lacros chrome will be connected
  // to it. After the test case finishes, the unique ash chrome will be
  // terminated and the next test case will use the default shared ash chrome.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void StartUniqueAshChrome(
      const std::vector<std::string>& enabled_features,
      const std::vector<std::string>& disabled_features,
      const std::vector<std::string>& additional_cmdline_switches,
      const std::string& bug_number_and_reason);
#endif

 protected:
  // Closes the given browser and waits for it to release all its resources.
  void CloseBrowserSynchronously(Browser* browser);

  // Closes the browser without waiting for it to release all its resources.
  // WARNING: This may leave tasks posted, but not yet run, in the message
  // loops. Prefer CloseBrowserSynchronously() over this method.
  void CloseBrowserAsynchronously(Browser* browser);

  // Closes all browsers. No guarantees are made about the destruction of
  // outstanding resources.
  void CloseAllBrowsers();

  // Runs the main thread message loop until the BrowserProcess indicates
  // we should quit. This will normally be called automatically during test
  // teardown, but may instead be run manually by the test, if necessary.
  void RunUntilBrowserProcessQuits();

  // Convenience methods for adding tabs to a Browser. Returns true if the
  // navigation succeeded. |check_navigation_success| is ignored and will be
  // removed as part of check_navigation_success http://crbug.com/1014186.
  // Do not add new usages of the version with |check_navigation_success|.
  [[nodiscard]] bool AddTabAtIndexToBrowser(Browser* browser,
                                            int index,
                                            const GURL& url,
                                            ui::PageTransition transition,
                                            bool check_navigation_success);
  [[nodiscard]] bool AddTabAtIndexToBrowser(Browser* browser,
                                            int index,
                                            const GURL& url,
                                            ui::PageTransition transition);
  [[nodiscard]] bool AddTabAtIndex(int index,
                                   const GURL& url,
                                   ui::PageTransition transition);

  // Sets up default command line that will be used to launch the child browser
  // process with an in-process test. Called by SetUp() after SetUpCommandLine()
  // to add default commandline switches. A default implementation is provided
  // in this class. If a test does not want to use the default implementation,
  // it should override this method.
  virtual void SetUpDefaultCommandLine(base::CommandLine* command_line);

  // Initializes the contents of the user data directory. Called by SetUp()
  // after creating the user data directory, but before any browser is launched.
  // If a test wishes to set up some initial non-empty state in the user data
  // directory before the browser starts up, it can do so here. Returns true if
  // successful. To set initial prefs, see SetUpLocalStatePrefService.
  [[nodiscard]] virtual bool SetUpUserDataDirectory();

  // Initializes the display::Screen instance.
  virtual void SetScreenInstance();

  // BrowserTestBase:
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override;

  // Ensures that no devtools are open, and then opens the devtools.
  void OpenDevToolsWindow(content::WebContents* web_contents);

  // Opens |url| in an incognito browser window with the incognito profile of
  // |profile|, blocking until the navigation finishes. This will create a new
  // browser if a browser with the incognito profile does not exist. Returns the
  // incognito window Browser.
  Browser* OpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Creates a browser with a single tab (about:blank), waits for the tab to
  // finish loading and shows the browser.
  Browser* CreateBrowser(Profile* profile);

  // Similar to |CreateBrowser|, but creates an incognito browser. If |profile|
  // is omitted, the currently active profile will be used.
  Browser* CreateIncognitoBrowser(Profile* profile = nullptr);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Similar to |CreateBrowser|, but creates a Guest browser.
  // To create a ChromeOS Guest user session, you need to add proper switches to
  // commandline while setting up the test. For an example see
  // AppListClientGuestModeBrowserTest::SetUpCommandLine.
  Browser* CreateGuestBrowser();
#endif

  // Creates a browser for a popup window with a single tab (about:blank), waits
  // for the tab to finish loading, and shows the browser.
  Browser* CreateBrowserForPopup(Profile* profile);

  // Creates a browser for an application and waits for it to load and shows
  // the browser.
  Browser* CreateBrowserForApp(const std::string& app_name, Profile* profile);

  // Called from the various CreateBrowser methods to add a blank tab, wait for
  // the navigation to complete, and show the browser's window.
  void AddBlankTabAndShow(Browser* browser);

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Return a CommandLine object that is used to relaunch the browser_test
  // binary as a browser process. This function is deliberately not defined on
  // the Mac because re-using an existing browser process when launching from
  // the command line isn't a concept that we support on the Mac; AppleEvents
  // are the Mac solution for the same need. Any test based on these functions
  // doesn't apply to the Mac. Likewise, Lacros is always launched by ash, and
  // not by the the process restarting itself.
  base::CommandLine GetCommandLineForRelaunch();
#endif

#if BUILDFLAG(IS_MAC)
  // Returns the autorelease pool in use inside RunTestOnMainThreadLoop().
  base::apple::ScopedNSAutoreleasePool* AutoreleasePool() const {
    return autorelease_pool_;
  }
#endif  // BUILDFLAG(IS_MAC)

  // Returns the test data path used by the embedded test server.
  base::FilePath GetChromeTestDataDir() const;

  void set_exit_when_last_browser_closes(bool value) {
    exit_when_last_browser_closes_ = value;
  }

  void set_open_about_blank_on_browser_launch(bool value) {
    open_about_blank_on_browser_launch_ = value;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void set_launch_browser_for_testing(
      std::unique_ptr<ash::full_restore::ScopedLaunchBrowserForTesting>
          launch_browser_for_testing) {
    launch_browser_for_testing_ = std::move(launch_browser_for_testing);
  }
#endif

  // Runs scheduled layouts on all Widgets using
  // Widget::LayoutRootViewIfNecessary(). No-op outside of Views.
  void RunScheduledLayouts();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  FakeAccountManagerUI* GetFakeAccountManagerUI() const;

  // Return the Ash chrome version hooked with Lacros. This API does not
  // depend on crosapi. So it is safe to call when crosapi is disabled and
  // as early as in test SetUp().
  static base::Version GetAshChromeVersion();

  // The following are the helper functions to manage Ash browser based windows
  // from Lacros browser tests. When running with Ash, Lacros browser tests can
  // create some Ash browser based UIs, such as SWA, Web UI, etc. These UIs
  // must be cleaned up when the test tears down, so that they won't pollute
  // the tests running after, since Lacros browser tests are running with the
  // shared Ash instance by default.
  void VerifyNoAshBrowserWindowOpenRightNow();
  void CloseAllAshBrowserWindows();
  void WaitUntilAtLeastOneAshBrowserWindowOpen();
  // Returns true if CloseAllAshBrowserWindows and
  // WaitUntilAtLeaseOneAshBrowserWindowOpen is supported.
  // TODO(crbug.com/1473375): Remove the following function once Ash stable
  // channel supports the Ash Browser Window APIs in
  // crosapi::mojom::TestController needed by the above functions.
  bool IsCloseAndWaitAshBrowserWindowApisSupported() const;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<display::Screen> screen_;
#endif

 private:
  friend class StartUniqueAshBrowserTest;

  void Initialize();

  // Quits all open browsers and waits until there are no more browsers.
  void QuitBrowsers();

  static SetUpBrowserFunction* global_browser_set_up_function_;

  // Usually references the browser created in BrowserMain().
  // If no browser is created in BrowserMain(), then |browser_| will remain
  // nullptr unless SelectFirstBrowser() is called after the creation of the
  // first browser instance at a later time.
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_ = nullptr;

  // Used to run the process until the BrowserProcess signals the test to quit.
  std::unique_ptr<base::RunLoop> run_loop_;

  // Temporary user data directory. Used only when a user data directory is not
  // specified in the command line.
  base::ScopedTempDir temp_user_data_dir_;

  // True if we should exit the tests after the last browser instance closes.
  bool exit_when_last_browser_closes_ = true;

  // True if the about:blank tab should be opened when the browser is launched.
  bool open_about_blank_on_browser_launch_ = true;

  // Use a default download directory to make sure downloads don't end up in the
  // system default location.
  base::ScopedTempDir default_download_dir_;

  base::test::ScopedFeatureList scoped_feature_list_;

  // In-product help can conflict with tests' expected window activation and
  // focus. This disables all IPH by default.
  //
  // This was previously done by disabling all IPH features, but that destroyed
  // all field trials that included an IPH because overriding any feature
  // touched by a field trial disables the field trial (see crbug.com/1381669).
  //
  // Individual tests can re-enable IPH using another ScopedIphFeatureList.
  feature_engagement::test::ScopedIphFeatureList block_all_iph_feature_list_;

#if BUILDFLAG(IS_MAC)
  raw_ptr<base::apple::ScopedNSAutoreleasePool, DanglingUntriaged>
      autorelease_pool_ = nullptr;
  std::unique_ptr<ScopedBundleSwizzlerMac> bundle_swizzler_;

  // Enable fake full keyboard access by default, so that tests don't depend on
  // system setting of the test machine. Also, this helps to make tests on Mac
  // more consistent with other platforms, where most views are focusable by
  // default.
  ui::test::ScopedFakeFullKeyboardAccess faked_full_keyboard_access_;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

#if defined(TOOLKIT_VIEWS)
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
#endif

  std::unique_ptr<MainThreadStackSamplingProfiler> sampling_profiler_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS does not create a browser by default when the full restore feature
  // is enabled. However almost all existing browser tests assume a browser is
  // created. Add ScopedLaunchBrowserForTesting to force creating a browser for
  // testing, when the full restore feature is enabled.
  std::unique_ptr<ash::full_restore::ScopedLaunchBrowserForTesting>
      launch_browser_for_testing_;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::ScopedTempDir unique_ash_user_data_dir_;
  base::Process ash_process_;
#endif
};

// When including either in_process_browser_test.h or android_browser_test.h
// depending on the platform, use this type alias as the test base class.
using PlatformBrowserTest = InProcessBrowserTest;

#endif  // CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_H_
