# Web App UI & Browser Integration (`chrome/browser/ui/web_applications`)

This directory contains the desktop UI controllers, browser window integration,
and navigation orchestration for Progressive Web Apps (PWAs) in Chromium.

Following Chromium's
[documentation guidelines](/docs/documentation_guidelines.md), this document
focuses on **how classes interact, how subsystems are hooked up, and
architectural layering**. For per-class API and method-level details, refer to
the respective header files.

## Architectural Layering & The UI Bridge

To prevent circular dependencies between Chrome UI and the core PWA backend, the
subsystem is split across distinct layers:

```
[Level 1: Core PWA Backend]      chrome/browser/web_applications/
                                 └── Defines abstract interface: WebAppUiManager
                                              ▲
                                              │ (implements)
[Level 2: UI Controller Bridge]  chrome/browser/ui/web_applications/
                                 └── WebAppUiManagerImpl, WebAppBrowserController
                                              │
                                              ▼ (creates & triggers)
[Level 3: Native Views Surfaces] chrome/browser/ui/views/web_apps/
                                 └── Dialogs, Bubbles, Frame Toolbars
```

- **Backend Independence:** Core PWA logic in `chrome/browser/web_applications/`
  (such as commands, sync, and databases) cannot depend directly on UI code. It
  communicates with the UI solely through the
  [`WebAppUiManager`](/chrome/browser/web_applications/web_app_ui_manager.h)
  interface.
- **The UI Bridge:** [`WebAppUiManagerImpl`](web_app_ui_manager_impl.h)
  implements this interface, translating backend requests (e.g. "show install
  dialog", "launch app window", "trigger update review") into concrete UI window
  operations and Views dialogs.

## How Subsystems Are Hooked Up

### 1. App Window Lifecycle & Controllers

```
Browser (is_type_app) ──► AppBrowserController (WebAppBrowserController)
                                   │
                                   ├──► Observes WebAppRegistrar (theme color, title, manifest updates)
                                   ├──► Observes WebAppInstallManager (install/uninstall events)
                                   └──► Controls BrowserView / Titlebar Frame
```

- When a `Browser` instance is created for an installed web app
  (`browser->is_type_app()` or `is_type_app_popup()`), an
  **[`AppBrowserController`](app_browser_controller.h)** (concretely
  **[`WebAppBrowserController`](web_app_browser_controller.h)**) is attached to
  the `Browser`.
- **Dynamic State Synchronization:** `WebAppBrowserController` observes the
  [`WebAppRegistrar`](/chrome/browser/web_applications/web_app_registrar.h) and
  [`WebAppInstallManager`](/chrome/browser/web_applications/web_app_install_manager.h).
  When manifest updates or metadata changes occur, it automatically updates the
  window title, frame theme colors, and custom titlebar elements without
  recreating the window.
- **Tab Association:** The tab's `content::WebContents` hosts a
  [`WebAppTabHelper`](/chrome/browser/web_applications/web_app_tab_helper.h),
  which tracks the active `AppId`, handles badging/audio focus, and links the
  tab to the owning `WebAppBrowserController`.

### 2. Navigation Capturing & Launch Pipeline

```
Navigation Request ──► BrowserNavigator::Navigate()
                             │
                             ├──► NavigationCapturingProcess::MaybeHandleAppNavigation()
                             │         │ (checks WebAppRegistrar for scope match & launch params)
                             │         ▼
                             │    web_app_launch_utils (focuses app window or opens pinned tab)
                             ▼
              NavigationHandle ──► NavigationCapturingRedirectionThrottle
                                       │ (handles cross-scope HTTP redirects)
                                       ▼
                                   NavigationCapturingProcess::HandleRedirect()
```

- **Interception & Initiation:** When a user initiates a navigation to an
  in-scope URL,
  [`BrowserNavigator`](/chrome/browser/ui/navigator/browser_navigator.h) invokes
  **[`NavigationCapturingProcess`](navigation_capturing_process.h)** before
  starting the navigation.
- **Routing & Dispatch:** `NavigationCapturingProcess` queries the
  `WebAppRegistrar` to verify the target app's user display mode and launch
  settings. It uses **[`web_app_launch_utils`](web_app_launch_utils.h)** to
  either focus an existing app window, navigate a pinned tab, or spawn a new app
  `Browser`.
- **Redirects:** If an ongoing navigation redirects to an in-scope or
  out-of-scope URL,
  [`NavigationCapturingRedirectionThrottle`](navigation_capturing_redirection_throttle.h)
  intercepts the redirect and notifies
  `NavigationCapturingProcess::HandleRedirect()`.

### 3. Omnibox Promotion & Install Triggering

```
AppBannerManager (components/webapps) ──► PwaInstallPageAction (Omnibox)
                                                    │ (user clicks)
                                                    ▼
                                           web_app_dialog_utils
                                                    │
                                                    ▼
                                     Views Install Dialog Delegate
```

- [`AppBannerManager`](/components/webapps/browser/banners/app_banner_manager.h)
  evaluates site installability and notifies the omnibox controller.
- **[`PwaInstallPageAction`](pwa_install_page_action.h)** renders the install
  icon button in the omnibox (LocationBarView). When clicked, it calls
  [`web_app_dialog_utils`](web_app_dialog_utils.h) to instantiate the
  appropriate Views install dialog in `chrome/browser/ui/views/web_apps/`.

## Testing Wiring & Fakes

- **Integration Browser Tests:** End-to-end UI flows (window controls, link
  capturing, install dialogs) use `WebAppBrowserTestBase` and utilities in
  `web_app_browsertest_util.h`.
- **Unit Test Fakes:** In unit tests where native Views surfaces should not be
  instantiated, `FakeWebAppProvider` injects `FakeWebAppUiManager` to intercept
  and mock UI dialog responses.
