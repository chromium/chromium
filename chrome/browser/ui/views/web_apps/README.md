# Web App Views UI (`chrome/browser/ui/views/web_apps`)

This directory contains the Views-based UI dialogs, modal bubbles, custom frame
toolbars, and test drivers for Progressive Web Apps (PWAs) on Desktop platforms
(Windows, Mac, Linux, ChromeOS).

Following Chromium's
[documentation guidelines](/docs/documentation_guidelines.md), this document
describes **how Views surfaces connect to the browser window hierarchy and PWA
backend**. For per-class API details, see the respective header files.

## How Views UI is Hooked Up

### 1. Dialog Invocation & Backend Callback Loop

```
Backend Command (scheduled by WebAppCommandScheduler)
         │
         ▼
WebAppUiManagerImpl (chrome/browser/ui/web_applications/)
         │
         ▼ (calls web_app_dialog_utils)
Views Dialog Delegate (views::DialogDelegateView / BubbleDialogDelegateView)
         │ (anchored to BrowserView / LocationBarView)
         ▼ (user confirms or dismisses)
Result Callback ──► Resumes Command execution in WebAppCommandManager
```

- **Anchoring & Modality:** Dialogs in this directory subclass
  `views::DialogDelegateView` or `views::BubbleDialogDelegateView`. They are
  typically anchored to either the browser window's location bar (for omnibox
  install prompts) or centered over the `BrowserView` (for modal permission or
  update confirmation dialogs).
- **Callback Loop:** UI dialogs do not mutate backend databases directly.
  Instead, when the user accepts or cancels a prompt (e.g.
  `WebAppInstallDialogDelegate`), the dialog executes a callback that signals
  the backend command (like `FetchManifestAndInstallCommand`) to proceed with
  installation or abort cleanly.

### 2. Custom Titlebar & Frame Toolbar Hierarchy (`frame_toolbar/`)

```
BrowserFrame (Non-Client Frame Area)
         │
         ▼
BrowserView ──► WebAppFrameToolbarView
                    ├── Window Controls Overlay (WCO) elements
                    ├── Origin / Title display
                    ├── Extension action icons & Page Actions
                    └── WebAppMenuButton (3-dot app menu)
```

- When running a standalone PWA window, `BrowserView` embeds
  **`WebAppFrameToolbarView`** into the custom window frame.
- **Window Controls Overlay (WCO):** When an app declares
  `"display_override": ["window-controls-overlay"]`, `WebAppFrameToolbarView`
  coordinates with `WebAppBrowserController` to reserve titlebar space for the
  web content while hosting essential window controls and security origin
  labels.

### 3. Integration Testing Driver Wiring

- **[`WebAppIntegrationTestDriver`](web_app_integration_test_driver.cc)**: Acts
  as the programmatic driver for Chromium's cross-platform PWA integration
  tests. It directly manipulates and asserts state across the Views hierarchy
  (clicking install bubbles, verifying titlebar states, triggering app menu
  items).
- For complete framework architecture, see
  [Integration Testing Framework](/chrome/browser/web_applications/docs/integration-testing-framework.md)
  and
  [How to Create WebApp Integration Tests](/chrome/browser/web_applications/docs/how-to-create-webapp-integration-tests.md).
