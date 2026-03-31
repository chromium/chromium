---
name: create-web-applications-browsertest
description: Instructions for creating a browser test in the chrome/browser/web_applications and chrome/browser/ui/web_applications directories.
---

# Creating Web Applications Browsertests

This skill guides the process of creating a browsertest in the
`chrome/browser/web_applications` directory. Read
[the testing documentation](../../testing.md) for information about testing
infrastructure in the [WebAppProvider](../../../README.md) system.

## 1. Test Base Class & Setup

For simple tests that don't need extra setup or flags, simply use
`using MyWebAppBrowserTest = WebAppBrowserTestBase`. If more complex fixture
logic is needed, like holding a `base::HistogramTester` or enabling a feature
flag, inherit from `WebAppBrowserTestBase`.

[`WebAppBrowserTestBase`](/chrome/browser/ui/web_applications/web_app_browsertest_base.h)
automatically ensures the WebAppProvider system is started correctly in the
`SetUpOnMainThread()` method, OS integration features are 'faked' out using the
`OsIntegrationTestOverride` system, and the `embedded_https_test_server()` is
started. See the header file for more information about capabilities it
provides.

## 2. Test Execution Environment Setup

In your test, use the embedded test server to serve URLs, and testing helpers
from `WebAppBrowserTestBase` to set up initial state.

Browser tests usually cause commands to execute via the high level operations
done by the user, like navigating the browser, interacting with a web app window
or dialog, etc. Due to this, waiting methods outlined in
[the testing documentation](../../testing.md#common-issue-waiting) are often
essential to reliably wait for, say, commands to be scheduled, completed, and
state updated etc.

```cpp
IN_PROC_BROWSER_TEST_F(MyWebAppBrowserTest, InstallAndLaunch) {
  GURL app_url = embedded_https_test_server()->GetURL("/web_apps/simple/index.html");

  // 1. Setup Phase
  Browser* app_browser = InstallWebAppFromPageGetBrowser(browser(), app_url);

  // 2. Execution Phase
  // Example 1) Cause an update by navigating browser() to a (not yet created)
  // URL like "/web_apps/simple/update_theme_color.html" which links to a
  // manifest with a new theme color.
  //
  // Example 2) Interact with the toolbar of app_browser() to, say click the
  // uninstall button from the 3-dot menu.

  ...
}
```

## 3. Waiting & Stability

Browsertests run a real browser, so you must carefully wait for UI and network
events to complete. Wait for commands in the `WebAppProvider` to settle if
you've triggered background work.

```cpp
// Wait for web content's manifest to make it to the WebAppTabHelper for
// operations like update:
test::WaitForLoadCompleteAndMaybeManifestSeen(
    browser()->tab_strip_model()->GetActiveWebContents());

// Wait for commands to finish.
provider().command_manager().AwaitAllCommandsCompleteForTesting();

// Potentially use observers if needed, etc.
```

## 4. Validating State

Tests can use internal state like `provider().registrar_unsafe()` or
`provider().icon_manager()` to verify assumptions, but in general browser tests
are intended to check 'user visible' / 'developer visible' / other end-to-end
state, so checking the results via something like web contents state, browser
state, ui state, and os integration state (via
`WebAppBrowserTestBase::os_integration_override()`) is most appropriate.
