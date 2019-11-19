// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/remoting/remote_desktop_browsertest.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/remoting/key_code_conv.h"
#include "chrome/test/remoting/page_load_notification_observer.h"
#include "chrome/test/remoting/remote_test_helper.h"
#include "chrome/test/remoting/waiter.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using extensions::Extension;

namespace remoting {

namespace {

// Command line arguments specific to the chromoting browser tests.
const char kOverrideUserDataDir[] = "override-user-data-dir";
const char kNoCleanup[] = "no-cleanup";
const char kNoInstall[] = "no-install";
const char kWebAppCrx[] = "webapp-crx";
const char kWebAppUnpacked[] = "webapp-unpacked";
const char kUserName[] = "username";
const char kUserPassword[] = "password";
const char kAccountsFile[] = "accounts-file";
const char kAccountType[] = "account-type";
const char kMe2MePin[] = "me2me-pin";
const char kRemoteHostName[] = "remote-host-name";
const char kExtensionName[] = "extension-name";
const char kHttpServer[] = "http-server";

}  // namespace

RemoteDesktopBrowserTest::RemoteDesktopBrowserTest()
    : remote_test_helper_(nullptr), extension_(nullptr) {
}

RemoteDesktopBrowserTest::~RemoteDesktopBrowserTest() {}

void RemoteDesktopBrowserTest::SetUp() {
  ParseCommandLine();
  PlatformAppBrowserTest::SetUp();
}

void RemoteDesktopBrowserTest::SetUpOnMainThread() {
  PlatformAppBrowserTest::SetUpOnMainThread();

  // Pushing the initial WebContents instance onto the stack before
  // RunTestOnMainThread() and after |InProcessBrowserTest::browser_|
  // is initialized in InProcessBrowserTest::RunTestOnMainThreadLoop()
  web_contents_stack_.push_back(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Change behavior of the default host resolver to avoid DNS lookup errors,
// so we can make network calls.
void RemoteDesktopBrowserTest::SetUpInProcessBrowserTestFixture() {
  // The resolver object lifetime is managed by sync_test_setup, not here.
  EnableDNSLookupForThisTest(
      new net::RuleBasedHostResolverProc(host_resolver()));
}

void RemoteDesktopBrowserTest::TearDownInProcessBrowserTestFixture() {
  DisableDNSLookupForThisTest();
}

void RemoteDesktopBrowserTest::VerifyInternetAccess() {
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.google.com"), 1);

  EXPECT_EQ(GetCurrentURL().host(), "www.google.com");
}

void RemoteDesktopBrowserTest::OpenClientBrowserPage() {
  // Open the client browser page in a new tab
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(http_server() + "/client.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Save this web content for later reference
  client_web_content_ = browser()->tab_strip_model()->GetActiveWebContents();

  // Go back to the previous tab that has chromoting opened
  browser()->tab_strip_model()->SelectPreviousTab();

  // Create the RemoteTestHelper object to use.
  remote_test_helper_.reset(new RemoteTestHelper(client_web_content_));
}

bool RemoteDesktopBrowserTest::HtmlElementVisible(const std::string& name) {
  _ASSERT_TRUE(HtmlElementExists(name));

  ExecuteScript(
      "function isElementVisible(name) {"
      "  var element = document.getElementById(name);"
      "  /* The existence of the element has already been ASSERTed. */"
      "  do {"
      "    if (element.hidden) {"
      "      return false;"
      "    }"
      "    element = element.parentNode;"
      "  } while (element != null);"
      "  return true;"
      "};");

  return ExecuteScriptAndExtractBool(
      "isElementVisible(\"" + name + "\")");
}

void RemoteDesktopBrowserTest::InstallChromotingAppCrx() {
  ASSERT_TRUE(!is_unpacked());

  base::FilePath install_dir(WebAppCrxPath());
  scoped_refptr<const Extension> extension(InstallExtensionWithUIAutoConfirm(
      install_dir, 1, browser()));

  EXPECT_FALSE(extension.get() == NULL);

  extension_ = extension.get();
}

void RemoteDesktopBrowserTest::InstallChromotingAppUnpacked() {
  ASSERT_TRUE(is_unpacked());

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(extension_service());

  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));

  installer->Load(webapp_unpacked_);

  observer.WaitForExtensionLoaded();
}

void RemoteDesktopBrowserTest::UninstallChromotingApp() {
  UninstallExtension(ChromotingID());
  extension_ = NULL;
}

void RemoteDesktopBrowserTest::VerifyChromotingLoaded(bool expected) {
  bool installed = false;

  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions::ExtensionRegistry::Get(profile())->enabled_extensions()) {
    // Is there a better way to recognize the chromoting extension
    // than name comparison?
    if (extension->name() == extension_name_) {
      if (extension_) {
        EXPECT_EQ(extension.get(), extension_);
      } else {
        extension_ = extension.get();
      }

      installed = true;
      break;
    }
  }

  if (installed) {
    // Either a V1 (TYPE_LEGACY_PACKAGED_APP) or a V2 (TYPE_PLATFORM_APP ) app.
    extensions::Manifest::Type type = extension_->GetType();
    EXPECT_TRUE(type == extensions::Manifest::TYPE_PLATFORM_APP ||
                type == extensions::Manifest::TYPE_LEGACY_PACKAGED_APP);

    EXPECT_TRUE(extension_->ShouldDisplayInAppLauncher());
  }

  ASSERT_EQ(installed, expected);
}

content::WebContents* RemoteDesktopBrowserTest::LaunchChromotingApp(
    bool defer_start,
    WindowOpenDisposition window_open_disposition) {
  _ASSERT_TRUE(extension_);

  GURL chromoting_main = Chromoting_Main_URL();
  // We cannot simply wait for any page load because the first page
  // loaded could be the generated background page. We need to wait
  // till the chromoting main page is loaded.
  PageLoadNotificationObserver observer(chromoting_main);
  observer.set_ignore_url_parameters(true);

  // If the app should be started in deferred mode, ensure that a "source" URL
  // parameter is present; if not, ensure that no such parameter is present.
  // The value of the parameter is determined by the AppLaunchParams ("test",
  // in this case).
  extensions::FeatureSwitch::ScopedOverride override_trace_app_source(
      extensions::FeatureSwitch::trace_app_source(),
      defer_start);

  if (is_platform_app()) {
    window_open_disposition = WindowOpenDisposition::NEW_WINDOW;
  }

  apps::LaunchService::Get(browser()->profile())
      ->OpenApplication(apps::AppLaunchParams(
          extension_->id(),
          is_platform_app() ? apps::mojom::LaunchContainer::kLaunchContainerNone
                            : apps::mojom::LaunchContainer::kLaunchContainerTab,
          window_open_disposition, apps::mojom::AppLaunchSource::kSourceTest));

  observer.Wait();


  // The active WebContents instance should be the source of the LOAD_STOP
  // notification.
  content::NavigationController* controller =
      content::Source<content::NavigationController>(
          observer.matched_source()).ptr();

  content::WebContents* web_contents = controller->GetWebContents();
  _ASSERT_TRUE(web_contents);

  if (web_contents != active_web_contents())
    web_contents_stack_.push_back(web_contents);

  if (is_platform_app()) {
    EXPECT_EQ(GetFirstAppWindowWebContents(), active_web_contents());
  } else {
    // For apps v1 only, the DOMOperationObserver is not ready at the LOAD_STOP
    // event. A half second wait is necessary for the subsequent javascript
    // injection to work.
    // TODO(weitaosu): Find out whether there is a more appropriate notification
    // to wait for so we can get rid of this wait.
    _ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(5)).Wait());
  }

  EXPECT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  return web_contents;
}

content::WebContents* RemoteDesktopBrowserTest::LaunchChromotingApp(
    bool defer_start) {
  return LaunchChromotingApp(defer_start, WindowOpenDisposition::CURRENT_TAB);
}

void RemoteDesktopBrowserTest::StartChromotingApp() {
  ClickOnControl("browser-test-continue-init");
}

void RemoteDesktopBrowserTest::Authorize() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  // The chromoting main page should be loaded in the current tab
  // and isAuthenticated() should be false (auth dialog visible).
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  ASSERT_FALSE(IsAuthenticated());

  // We cannot simply wait for any page load because the first page
  // loaded will be chrome://chrome-signin in a packaged app. We need to wait
  // for the Google login page to be loaded (inside an embedded iframe).
  GURL google_login("https://accounts.google.com/ServiceLogin");
  PageLoadNotificationObserver observer(google_login);
  observer.set_ignore_url_parameters(true);

  ClickOnControl("auth-button");

  observer.Wait();

  content::NavigationController* controller =
      content::Source<content::NavigationController>(
          observer.matched_source()).ptr();
  content::WebContents* web_contents = controller->GetWebContents();
  _ASSERT_TRUE(web_contents);

  if (web_contents != active_web_contents()) {
    // Pushing the WebContents hosting the Google login page onto the stack.
    // If this is a packaged app the Google login page will be loaded in an
    // iframe embedded in the chrome://chrome-signin page. But we can ignore
    // that WebContents because we never need to interact with it directly.
    LOG(INFO) << "Pushing onto the stack: " << web_contents->GetURL();
    web_contents_stack_.push_back(web_contents);
  }

  // Verify the active tab is at the "Google Accounts" login page.
  EXPECT_EQ("accounts.google.com", GetCurrentURL().host());
}

void RemoteDesktopBrowserTest::Authenticate() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  // The active WebContents should have the "Google Accounts" login page loaded.
  ASSERT_EQ("accounts.google.com", GetCurrentURL().host());

  // Sign-in by injecting JavaScript in the chrome://chrome-signin/ page.
  login_ui_test_utils::ExecuteJsToSigninInSigninFrame(browser(), username_,
                                                      password_);

  // TODO(weitaosu): Is there a better way to verify we are on the
  // "Request for Permission" page?
  // V2 app won't ask for approval here because the chromoting test account
  // has already been granted permissions.
  if (!is_platform_app()) {
    EXPECT_EQ(GetCurrentURL().host(), "accounts.google.com");
    EXPECT_TRUE(HtmlElementExists("submit_approve_access"));
  }
}

void RemoteDesktopBrowserTest::Approve() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  if (is_platform_app()) {
    // Popping the login window off the stack to return to the chromoting
    // window.
    web_contents_stack_.pop_back();

    // There is nothing for the V2 app to approve because the chromoting test
    // account has already been granted permissions.
    //
    // TODO(weitaosu): Revoke the permission at the beginning of the test so
    // that we can test first-time experience here.
    //
    // TODO(jamiewalch): Remove the elapsed time logging once this has run a few
    // times on the bots.
    base::Time start = base::Time::Now();
    ConditionalTimeoutWaiter waiter(
        base::TimeDelta::FromSeconds(10),
        base::TimeDelta::FromSeconds(1),
        base::Bind(
            &RemoteDesktopBrowserTest::IsAuthenticatedInWindow,
            active_web_contents()));
    bool result = waiter.Wait();
    base::TimeDelta elapsed = base::Time::Now() - start;
    LOG(INFO) << "*** IsAuthenticatedInWindow took "
              << elapsed.InSeconds() << "s";
    ASSERT_TRUE(result);
  } else {
    ASSERT_EQ("accounts.google.com", GetCurrentURL().host());

    // Is there a better way to verify we are on the "Request for Permission"
    // page?
    ASSERT_TRUE(HtmlElementExists("submit_approve_access"));

    const GURL chromoting_main = Chromoting_Main_URL();

    // active_web_contents() is still the login window but the observer
    // should be set up to observe the chromoting window because that is
    // where we'll receive the message from the login window and reload the
    // chromoting app.
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
          base::Bind(
              &RemoteDesktopBrowserTest::IsAuthenticatedInWindow,
              browser()->tab_strip_model()->GetActiveWebContents()));

    // Click to Approve the web-app.
    ClickOnControl("submit_approve_access");

    observer.Wait();

    // Popping the login window off the stack to return to the chromoting
    // window.
    web_contents_stack_.pop_back();
  }

  ASSERT_TRUE(GetCurrentURL() == Chromoting_Main_URL());

  EXPECT_TRUE(IsAuthenticated());
}

void RemoteDesktopBrowserTest::ExpandMe2Me() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ(Chromoting_Main_URL(), GetCurrentURL());
  EXPECT_TRUE(IsAuthenticated());

  // The Me2Me host list should be hidden.
  ASSERT_FALSE(HtmlElementVisible("me2me-content"));
  // The Me2Me "Get Start" button should be visible.
  ASSERT_TRUE(HtmlElementVisible("get-started-me2me"));

  // Starting Me2Me.
  ExecuteScript("remoting.showMe2MeUiAndSave();");

  EXPECT_TRUE(HtmlElementVisible("me2me-content"));
  EXPECT_FALSE(HtmlElementVisible("me2me-first-run"));
}

void RemoteDesktopBrowserTest::DisconnectMe2Me() {
  // The chromoting extension should be installed.
  ASSERT_TRUE(extension_);

  ASSERT_TRUE(RemoteDesktopBrowserTest::IsSessionConnected());

  ExecuteScript("remoting.app.getActivity().stop();");

  EXPECT_TRUE(HtmlElementVisible("client-dialog"));
  EXPECT_TRUE(HtmlElementVisible("client-reconnect-button"));
  EXPECT_TRUE(HtmlElementVisible("client-finished-me2me-button"));

  ClickOnControl("client-finished-me2me-button");

  EXPECT_FALSE(HtmlElementVisible("client-dialog"));
}

void RemoteDesktopBrowserTest::SimulateKeyPressWithCode(
    ui::KeyboardCode keyCode,
    const std::string& codeStr) {
  ui::DomCode code = ui::KeycodeConverter::CodeStringToDomCode(codeStr);
  SimulateKeyPressWithCode(ui::DomKey(), code, keyCode, false, false, false,
                           false);
}

void RemoteDesktopBrowserTest::SimulateKeyPressWithCode(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode keyCode,
    bool control,
    bool shift,
    bool alt,
    bool command) {
  content::SimulateKeyPress(active_web_contents(), key, code, keyCode, control,
                            shift, alt, command);
}

void RemoteDesktopBrowserTest::SimulateCharInput(char c) {
  const char* codeStr;
  ui::KeyboardCode keyboard_code;
  bool shift;
  GetKeyValuesFromChar(c, &codeStr, &keyboard_code, &shift);
  ui::DomKey key = ui::DomKey::FromCharacter(c);
  ASSERT_TRUE(codeStr != NULL);
  ui::DomCode code = ui::KeycodeConverter::CodeStringToDomCode(codeStr);
  SimulateKeyPressWithCode(key, code, keyboard_code, false, shift, false,
                           false);
}

void RemoteDesktopBrowserTest::SimulateStringInput(const std::string& input) {
  for (size_t i = 0; i < input.length(); ++i)
    SimulateCharInput(input[i]);
}

void RemoteDesktopBrowserTest::SimulateMouseLeftClickAt(int x, int y) {
  SimulateMouseClickAt(0, blink::WebMouseEvent::Button::kLeft, x, y);
}

void RemoteDesktopBrowserTest::SimulateMouseClickAt(
    int modifiers, blink::WebMouseEvent::Button button, int x, int y) {
  // TODO(weitaosu): The coordinate translation doesn't work correctly for
  // apps v2.
  ExecuteScript(
      "var clientPluginElement = "
           "document.getElementById('session-client-plugin');"
      "var clientPluginRect = clientPluginElement.getBoundingClientRect();");

  int top = ExecuteScriptAndExtractInt("clientPluginRect.top");
  int left = ExecuteScriptAndExtractInt("clientPluginRect.left");
  int width = ExecuteScriptAndExtractInt("clientPluginRect.width");
  int height = ExecuteScriptAndExtractInt("clientPluginRect.height");

  ASSERT_GT(x, 0);
  ASSERT_LT(x, width);
  ASSERT_GT(y, 0);
  ASSERT_LT(y, height);

  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(),
      modifiers,
      button,
      gfx::Point(left + x, top + y));
}

void RemoteDesktopBrowserTest::Install() {
  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    if (is_unpacked())
      InstallChromotingAppUnpacked();
    else
      InstallChromotingAppCrx();
  }

  VerifyChromotingLoaded(true);
}

void RemoteDesktopBrowserTest::LoadBrowserTestJavaScript(
    content::WebContents* content) {
  LoadScript(content, FILE_PATH_LITERAL("browser_test.js"));
  LoadScript(content, FILE_PATH_LITERAL("mock_client_plugin.js"));
  LoadScript(content, FILE_PATH_LITERAL("mock_host_list_api.js"));
  LoadScript(content, FILE_PATH_LITERAL("mock_identity.js"));
  LoadScript(content, FILE_PATH_LITERAL("mock_oauth2_api.js"));
  LoadScript(content, FILE_PATH_LITERAL("mock_session_connector.js"));
  LoadScript(content, FILE_PATH_LITERAL("mock_signal_strategy.js"));
  LoadScript(content, FILE_PATH_LITERAL("timeout_waiter.js"));
  LoadScript(content, FILE_PATH_LITERAL("sinon.js"));
}

void RemoteDesktopBrowserTest::Cleanup() {
  // TODO(weitaosu): Remove this hack by blocking on the appropriate
  // notification.
  // The browser may still be loading images embedded in the webapp. If we
  // uinstall it now those load will fail.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(2)).Wait());

  if (!NoCleanup()) {
    UninstallChromotingApp();
    VerifyChromotingLoaded(false);
  }

  // TODO(chaitali): Remove this additional timeout after we figure out
  // why this is needed for the v1 app to work.
  // Without this timeout the test fail with a "CloseWebContents called for
  // tab not in our strip" error for the v1 app.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(2)).Wait());
}

content::WebContents* RemoteDesktopBrowserTest::SetUpTest() {
  LOG(INFO) << "Starting Test Setup.";
  VerifyInternetAccess();
  Install();
  content::WebContents* app_web_content = LaunchChromotingApp(false);
  Auth();
  LoadBrowserTestJavaScript(app_web_content);
  ExpandMe2Me();
  // The call to EnsureRemoteConnectionEnabled() does a PIN reset.
  // This causes the test to fail because of a recent bug:
  // crbug.com/430676
  // TODO(anandc): Reactivate this call after above bug is fixed.
  // EnsureRemoteConnectionEnabled(app_web_content);
  return app_web_content;
}

void RemoteDesktopBrowserTest::Auth() {
  // For this test, we must be given the user-name and password.
  ASSERT_TRUE(!username_.empty() && !password_.empty());

  Authorize();
  Authenticate();
  Approve();
}

void RemoteDesktopBrowserTest::EnsureRemoteConnectionEnabled(
    content::WebContents* app_web_contents) {
  // browser_test.ensureRemoteConnectionEnabled is defined in
  // browser_test.js, which must be loaded before calling this function.
  // TODO(kelvinp): This function currently only works on linux when the user is
  // already part of the chrome-remote-desktop group.  Extend this functionality
  // to Mac (https://crbug.com/397576) and Windows (https://crbug.com/397575).
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      app_web_contents,
      "browserTest.ensureRemoteConnectionEnabled(" + me2me_pin() + ")",
      &result));
  EXPECT_TRUE(result) << "Cannot start the host with Pin:" << me2me_pin();
}

void RemoteDesktopBrowserTest::ConnectToLocalHost(bool remember_pin) {
  // Wait for local-host to be ready.
  ConditionalTimeoutWaiter waiter(
        base::TimeDelta::FromSeconds(5),
        base::TimeDelta::FromMilliseconds(500),
        base::Bind(&RemoteDesktopBrowserTest::IsLocalHostReady,
                   base::Unretained(this)));
  EXPECT_TRUE(waiter.Wait());

  // Verify that the local host is online.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.hostList.localHostSection_.host_.hostName && "
      "remoting.hostList.localHostSection_.host_.hostId && "
      "remoting.hostList.localHostSection_.host_.status && "
      "remoting.hostList.localHostSection_.host_.status == 'ONLINE'"));

  // Connect.
  ClickOnControl("local-host-connect-button");

  // Enter the pin # passed in from the command line.
  EnterPin(me2me_pin(), remember_pin);

  WaitForConnection();
}

void RemoteDesktopBrowserTest::ConnectToRemoteHost(
    const std::string& host_name, bool remember_pin) {

  // Wait for hosts list to be fetched.
  // This test typically runs with a clean user-profile, with no host-list
  // cached. Waiting for the host-list to be null is sufficient to proceed.
  ConditionalTimeoutWaiter waiter(
        base::TimeDelta::FromSeconds(5),
        base::TimeDelta::FromMilliseconds(500),
        base::Bind(&RemoteDesktopBrowserTest::IsHostListReady,
                   base::Unretained(this)));
  EXPECT_TRUE(waiter.Wait());

  std::string host_id = ExecuteScriptAndExtractString(
      "remoting.hostList.getHostIdForName('" + host_name + "')");

  EXPECT_FALSE(host_id.empty());
  std::string element_id = "host_" + host_id;

  // Wait for the hosts to be online. Try 3 times each spanning 20 seconds
  // successively for 60 seconds.
  ConditionalTimeoutWaiter hostOnlineWaiter(
      base::TimeDelta::FromSeconds(60), base::TimeDelta::FromSeconds(20),
      base::Bind(&RemoteDesktopBrowserTest::IsHostOnline,
                 base::Unretained(this), host_id));
  EXPECT_TRUE(hostOnlineWaiter.Wait());

  ClickOnControl(element_id);

  // Enter the pin # passed in from the command line.
  EnterPin(me2me_pin(), remember_pin);

  WaitForConnection();
}

void RemoteDesktopBrowserTest::EnableDNSLookupForThisTest(
    net::RuleBasedHostResolverProc* host_resolver) {
  // mock_host_resolver_override_ takes ownership of the resolver.
  scoped_refptr<net::RuleBasedHostResolverProc> resolver =
      new net::RuleBasedHostResolverProc(host_resolver);
  resolver->AllowDirectLookup("*.google.com");
  // On Linux, we use Chromium's NSS implementation which uses the following
  // hosts for certificate verification. Without these overrides, running the
  // integration tests on Linux causes errors as we make external DNS lookups.
  resolver->AllowDirectLookup("*.thawte.com");
  resolver->AllowDirectLookup("*.geotrust.com");
  resolver->AllowDirectLookup("*.gstatic.com");
  resolver->AllowDirectLookup("*.googleapis.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver.get()));
}

void RemoteDesktopBrowserTest::DisableDNSLookupForThisTest() {
  mock_host_resolver_override_.reset();
}

void RemoteDesktopBrowserTest::ParseCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // The test framework overrides any command line user-data-dir
  // argument with a /tmp/.org.chromium.Chromium.XXXXXX directory.
  // That happens in the ChromeTestLauncherDelegate, and affects
  // all unit tests (no opt out available). It intentionally erases
  // any --user-data-dir switch if present and appends a new one.
  // Re-override the default data dir if override-user-data-dir
  // is specified.
  if (command_line->HasSwitch(kOverrideUserDataDir)) {
    const base::FilePath& override_user_data_dir =
        command_line->GetSwitchValuePath(kOverrideUserDataDir);

    ASSERT_FALSE(override_user_data_dir.empty());

    command_line->AppendSwitchPath(switches::kUserDataDir,
                                   override_user_data_dir);
  }

  base::CommandLine::StringType accounts_file =
      command_line->GetSwitchValueNative(kAccountsFile);
  std::string account_type = command_line->GetSwitchValueASCII(kAccountType);
  if (!accounts_file.empty()) {
    // We've been passed in a file containing accounts information.
    // In this case, we'll obtain the user-name and password information from
    // the specified file, even if user-name and password have been specified
    // on the command-line.
    base::FilePath accounts_file_path((base::FilePath(accounts_file)));
    ASSERT_FALSE(account_type.empty());
    ASSERT_TRUE(base::PathExists((base::FilePath(accounts_file))));
    SetUserNameAndPassword((base::FilePath(accounts_file)), account_type);
  } else {
    // No file for accounts specified. Read user-name and password from command
    // line.
    username_ = command_line->GetSwitchValueASCII(kUserName);
    password_ = command_line->GetSwitchValueASCII(kUserPassword);
  }

  me2me_pin_ = command_line->GetSwitchValueASCII(kMe2MePin);
  remote_host_name_ = command_line->GetSwitchValueASCII(kRemoteHostName);
  extension_name_ = command_line->GetSwitchValueASCII(kExtensionName);
  http_server_ = command_line->GetSwitchValueASCII(kHttpServer);

  no_cleanup_ = command_line->HasSwitch(kNoCleanup);
  no_install_ = command_line->HasSwitch(kNoInstall);

  if (!no_install_) {
    webapp_crx_ = command_line->GetSwitchValuePath(kWebAppCrx);
    webapp_unpacked_ = command_line->GetSwitchValuePath(kWebAppUnpacked);
    // One and only one of these two arguments should be provided.
    ASSERT_NE(webapp_crx_.empty(), webapp_unpacked_.empty());
  }

  // Enable experimental extensions; this is to allow adding the LG extensions
  command_line->AppendSwitch(
    extensions::switches::kEnableExperimentalExtensionApis);
}

void RemoteDesktopBrowserTest::ExecuteScript(const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(active_web_contents(), script));
}

void RemoteDesktopBrowserTest::ExecuteScriptAndWaitForAnyPageLoad(
    const std::string& script) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_web_contents()->
              GetController()));

  ExecuteScript(script);

  observer.Wait();
}

bool RemoteDesktopBrowserTest::ExecuteScriptAndExtractBool(
    const std::string& script) {
  return RemoteTestHelper::ExecuteScriptAndExtractBool(active_web_contents(),
                                                       script);
}

// Helper to execute a JavaScript code snippet in the active WebContents
// and extract the int result.
int RemoteDesktopBrowserTest::ExecuteScriptAndExtractInt(
    const std::string& script) {
  return RemoteTestHelper::ExecuteScriptAndExtractInt(active_web_contents(),
                                                      script);
}

// Helper to execute a JavaScript code snippet in the active WebContents
// and extract the string result.
std::string RemoteDesktopBrowserTest::ExecuteScriptAndExtractString(
    const std::string& script) {
  return RemoteTestHelper::ExecuteScriptAndExtractString(active_web_contents(),
                                                         script);
}

// static
bool RemoteDesktopBrowserTest::LoadScript(
    content::WebContents* web_contents,
    const base::FilePath::StringType& path) {
  std::string script;
  base::FilePath src_dir;
  _ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &src_dir));
  base::FilePath script_path =
      src_dir.Append(FILE_PATH_LITERAL("remoting/browser_test_resources/"));
  script_path = script_path.Append(path);

  if (!base::ReadFileToString(script_path, &script)) {
    LOG(ERROR) << "Failed to load script " << script_path.value();
    return false;
  }

  return content::ExecuteScript(web_contents, script);
}

// static
void RemoteDesktopBrowserTest::RunJavaScriptTest(
    content::WebContents* web_contents,
    const std::string& testName,
    const std::string& testData) {
  std::string result;
  std::string script = "browserTest.runTest(browserTest." + testName + ", " +
                       testData + ");";

  DVLOG(1) << "Executing " << script;

  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(web_contents, script, &result));

  // Read in the JSON
  base::Optional<base::Value> value =
      base::JSONReader::Read(result, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(value);

  // Convert to dictionary
  base::DictionaryValue* dict_value = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  bool succeeded;
  std::string error_message;
  std::string stack_trace;

  // Extract the fields
  ASSERT_TRUE(dict_value->GetBoolean("succeeded", &succeeded));
  ASSERT_TRUE(dict_value->GetString("error_message", &error_message));
  ASSERT_TRUE(dict_value->GetString("stack_trace", &stack_trace));

  EXPECT_TRUE(succeeded) << error_message << "\n" << stack_trace;
}

void RemoteDesktopBrowserTest::ClickOnControl(const std::string& name) {
  ASSERT_TRUE(HtmlElementVisible(name));

  std::string has_disabled_attribute =
    "document.getElementById('" + name + "').hasAttribute('disabled')";

  if (RemoteTestHelper::ExecuteScriptAndExtractBool(active_web_contents(),
                                                    has_disabled_attribute)) {
    // This element has a disabled attribute. Wait for it become enabled.
    ConditionalTimeoutWaiter waiter(
          base::TimeDelta::FromSeconds(5),
          base::TimeDelta::FromMilliseconds(500),
          base::Bind(&RemoteDesktopBrowserTest::IsEnabled,
            active_web_contents(), name));
    ASSERT_TRUE(waiter.Wait());
  }

  ExecuteScript("document.getElementById(\"" + name + "\").click();");
}

void RemoteDesktopBrowserTest::EnterPin(const std::string& pin,
                                        bool remember_pin) {
  // Wait for the pin-form to be displayed. This can take a while.
  // We also need to dismiss the host-needs-update dialog if it comes up.
  // TODO(weitaosu) 1: Instead of polling, can we register a callback to be
  // called when the pin-form is ready?
  // TODO(weitaosu) 2: Instead of blindly dismiss the host-needs-update dialog,
  // we should verify that it only pops up at the right circumstance. That
  // probably belongs in a separate test case though.
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(30),
      base::TimeDelta::FromSeconds(1),
      base::Bind(&RemoteDesktopBrowserTest::IsPinFormVisible,
                 base::Unretained(this)));
  EXPECT_TRUE(waiter.Wait());

  ExecuteScript(
      "document.getElementById(\"pin-entry\").value = \"" + pin + "\";");

  if (remember_pin) {
    EXPECT_TRUE(HtmlElementVisible("remember-pin"));
    EXPECT_FALSE(ExecuteScriptAndExtractBool(
        "document.getElementById('remember-pin-checkbox').checked"));
    ClickOnControl("remember-pin");
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        "document.getElementById('remember-pin-checkbox').checked"));
  }

  ClickOnControl("pin-connect-button");
}

void RemoteDesktopBrowserTest::WaitForConnection() {
  // Wait until the client has connected to the server.
  // This can take a while.
  // TODO(weitaosu): Instead of polling, can we register a callback to
  // remoting.clientSession.onStageChange_?
  ConditionalTimeoutWaiter waiter(
      base::TimeDelta::FromSeconds(30),
      base::TimeDelta::FromSeconds(1),
      base::Bind(&RemoteDesktopBrowserTest::IsSessionConnected,
                 base::Unretained(this)));
  EXPECT_TRUE(waiter.Wait());

  // The client is not yet ready to take input when the session state becomes
  // CONNECTED. Wait for 2 seconds for the client to become ready.
  // TODO(weitaosu): Find a way to detect when the client is truly ready.
  TimeoutWaiter(base::TimeDelta::FromSeconds(2)).Wait();
}

bool RemoteDesktopBrowserTest::IsHostOnline(const std::string& host_id) {
  ExecuteScript("remoting.hostList.refreshAndDisplay()");

  // Verify the host is online.
  std::string element_id = "host_" + host_id;
  std::string host_div_class = ExecuteScriptAndExtractString(
      "document.getElementById('" + element_id + "').parentNode.className");

  return (std::string::npos != host_div_class.find("host-online"));
}

bool RemoteDesktopBrowserTest::IsLocalHostReady() {
  // TODO(weitaosu): Instead of polling, can we register a callback to
  // remoting.hostList.setLocalHost_?
  return ExecuteScriptAndExtractBool(
      "remoting.hostList.localHostSection_.host_ != null");
}

bool RemoteDesktopBrowserTest::IsHostListReady() {
  // Wait until hostList is not null.
  // The connect-to-host tests are run on the waterfall using a new profile-dir.
  // No hosts will be cached.
  return ExecuteScriptAndExtractBool(
    "remoting.hostList != null && remoting.hostList.hosts_ != null");
}

bool RemoteDesktopBrowserTest::IsSessionConnected() {
  // If some form of PINless authentication is enabled, the host version
  // warning may appear while waiting for the session to connect.
  DismissHostVersionWarningIfVisible();

  return ExecuteScriptAndExtractBool(
      "remoting.currentMode === remoting.AppMode.IN_SESSION");
}

bool RemoteDesktopBrowserTest::IsPinFormVisible() {
  DismissHostVersionWarningIfVisible();
  return HtmlElementVisible("pin-form");
}

void RemoteDesktopBrowserTest::DismissHostVersionWarningIfVisible() {
  if (HtmlElementVisible("host-needs-update-connect-button"))
    ClickOnControl("host-needs-update-connect-button");
}

void RemoteDesktopBrowserTest::SetUserNameAndPassword(
    const base::FilePath& accounts_file_path,
    const std::string& account_type) {
  // Read contents of accounts file, using its absolute path.
  base::FilePath absolute_path = base::MakeAbsoluteFilePath(accounts_file_path);
  std::string accounts_info;
  ASSERT_TRUE(base::ReadFileToString(absolute_path, &accounts_info));

  // Get the root dictionary from the input json file contents.
  base::Optional<base::Value> root =
      base::JSONReader::Read(accounts_info, base::JSON_ALLOW_TRAILING_COMMAS);

  const base::DictionaryValue* root_dict = NULL;
  ASSERT_TRUE(root);
  ASSERT_TRUE(root->GetAsDictionary(&root_dict));

  // Now get the dictionary for the specified account type.
  const base::DictionaryValue* account_dict = NULL;
  ASSERT_TRUE(root_dict->GetDictionary(account_type, &account_dict));
  ASSERT_TRUE(account_dict->GetString(kUserName, &username_));
  ASSERT_TRUE(account_dict->GetString(kUserPassword, &password_));
}

// static
bool RemoteDesktopBrowserTest::IsAuthenticatedInWindow(
    content::WebContents* web_contents) {
  return RemoteTestHelper::ExecuteScriptAndExtractBool(
      web_contents, "remoting.identity.isAuthenticated()");
}

// static
bool RemoteDesktopBrowserTest::IsHostActionComplete(
    content::WebContents* client_web_content,
    std::string host_action_var) {
  return RemoteTestHelper::ExecuteScriptAndExtractBool(
      client_web_content,
      host_action_var);
}

// static
bool RemoteDesktopBrowserTest::IsEnabled(
    content::WebContents* client_web_content,
    const std::string& element_name) {
  return !RemoteTestHelper::ExecuteScriptAndExtractBool(
    client_web_content,
    "document.getElementById(\"" + element_name + "\").disabled");
}

bool RemoteDesktopBrowserTest::IsAppModeEqualTo(const std::string& mode) {
  return ExecuteScriptAndExtractBool(
      "remoting.currentMode == " + mode);
}

void RemoteDesktopBrowserTest::DisableRemoteConnection() {
  ConditionalTimeoutWaiter hostReadyWaiter(
        base::TimeDelta::FromSeconds(5),
        base::TimeDelta::FromMilliseconds(500),
        base::Bind(&RemoteDesktopBrowserTest::IsLocalHostReady,
                   base::Unretained(this)));
  EXPECT_TRUE(hostReadyWaiter.Wait());

  ClickOnControl("stop-daemon");

  ConditionalTimeoutWaiter setupDoneWaiter(
          base::TimeDelta::FromSeconds(30),
          base::TimeDelta::FromMilliseconds(500),
          base::Bind(&RemoteDesktopBrowserTest::IsAppModeEqualTo,
                     base::Unretained(this),
                     "remoting.AppMode.HOST_SETUP_DONE"));
  EXPECT_TRUE(setupDoneWaiter.Wait());

  ClickOnControl("host-config-done-dismiss");

  ConditionalTimeoutWaiter homeWaiter(
          base::TimeDelta::FromSeconds(5),
          base::TimeDelta::FromMilliseconds(500),
          base::Bind(&RemoteDesktopBrowserTest::IsAppModeEqualTo,
                     base::Unretained(this),
                     "remoting.AppMode.HOME"));
  EXPECT_TRUE(homeWaiter.Wait());
}

}  // namespace remoting
