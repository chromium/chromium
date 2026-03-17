## Web Apps - Desktop Concepts

In addition to the [universal web app concepts](/docs/webapps/concepts.md), there are several concepts specific to desktop web apps and the `WebAppProvider` system.

### User Display Mode

In addition to the developer-specified `display` (see [Display Mode](/docs/webapps/concepts.md#display-mode)), the user can specify how they want a WebApp to be displayed, with the only option being whether to "open in a window" or not. Internally, this is expressed in the same display mode enumeration type, but only the `kStandalone` and `kBrowser` values are used to specify "open in a window" and "do not open in a window", respectively.

#### Effective Display Mode

The pseudocode to determine the ACTUAL display mode a WebApp is displayed in is:

```js
if (user_display_mode == kStandalone)
  return developer_specified_display_mode;
else
  return kBrowser; // Open in a tab.
```

#### Open-in-window

This refers to the user specifying that a WebApp should open in the developer specified display mode.

#### Open-in-browser-tab

This refers to the user specifying that a WebApp should NOT open in a window, and thus the WebApp, if launched, will just be opened in a browser tab.

### App Management

Each app has one or more 'management source', specified by the [`WebAppManagement::Type`](https://source.chromium.org/search?q=f:web_app_management_type.h%20WebAppManagement::Type) enumeration. This signifies the system that is 'managing' the install, AKA responsible for installing or uninstalling the app. Internally, the web app system will ensure that the app will only be uninstalled if there are no sources left in the app.

When a user installs an app, the `kSync` management source is specified, because user installs are considered 'managed' by the sync system (and installs will by synced to all devices). See the `WebAppManagement` enumeration for the description of other management sources.

Installation by certain sources can cause the app to no longer be "uninstallable" by the user. The method [`CanUserUninstallWebApp`](https://source.chromium.org/search?q=f:web_app.h%20CanUserUninstallWebApp) function determines if this is the case.

#### Placeholder app

There are some webapps which are managed by external sources - for example, the enterprise policy force-install apps, or the system web apps for ChromeOS. These are generally not installed by user interaction, and the WebAppProvider needs to install something for each of these apps.

Sometimes, the installation of these apps can fail because the install url is not reachable (usually a cert or login needs to occur, and the url is redirected). When this happens, the system [can](https://source.chromium.org/search?q=ExternalInstallOptions::install_placeholder) install a "placeholder" app, which is a fake application that, when launched, navigates to the install url of the application, given by the external app manager.

To resolve placeholder apps back into the intended installation, another external
install is triggered for the same install URL. The
[`ExternalAppResolutionCommand`](https://source.chromium.org/search?q=ExternalAppResolutionCommand) is given the ID of the existing
placeholder app. After the new app is successfully installed, the
placeholder app is uninstalled.

### Installation State

A web app can exist in several different installation states, which determine
its capabilities and how it's presented to the user. These states are
represented internally by the [`InstallState` enum](/chrome/browser/web_applications/proto/web_app_install_state.proto).

*   **Suggested from another device**: The app is installed on another one of
    the user's devices and has been synced to the current device, but it
    hasn't been fully installed yet. These apps appear in `chrome://apps` (often
    grayed out) but don't have OS integrations like shortcuts or protocol
    handlers. They cannot be launched in a standalone window until they are
    fully installed. This state corresponds to
    `InstallState::SUGGESTED_FROM_ANOTHER_DEVICE`.

*   **Installed without OS integration**: The app is installed on the device,
    but without any OS-level integrations. This is common for pre-installed
    apps on non-ChromeOS platforms. Like suggested apps, they cannot be
    launched in a standalone window until OS integration is enabled. This state
    corresponds to `InstallState::INSTALLED_WITHOUT_OS_INTEGRATION`.

*   **Installed with OS integration**: The app is fully installed and
    integrated with the operating system. This includes shortcuts, protocol
    handling, file handling, and the ability to run on OS login. This is the
    state for user-installed apps or apps that have had their OS integration
    explicitly triggered. This state corresponds to
    `InstallState::INSTALLED_WITH_OS_INTEGRATION`.

To query for apps with specific capabilities, which often depend on their
installation state, the [`WebAppFilter`](/chrome/browser/web_applications/web_app_filter.h) class should be used.
For example, `WebAppFilter::IsSuggestedApp()` can be used to find apps that are
suggested from another device.

#### Triggering a full installation

For an app that is only "suggested" or "installed without OS integration", a
full installation with OS integration can be triggered by:

*   The user installing the app again through a normal installation flow (e.g.,
    the omnibox install icon).
*   The user right-clicking the app on `chrome://apps` and selecting "Install".
*   Programmatically, by scheduling an
    [`InstallAppLocallyCommand`](/chrome/browser/web_applications/commands/install_app_locally_command.h).

This was designed this way because on non-ChromeOS devices, it was considered a
bad user experience to fully install all of a user's synced web apps (creating
platform shortcuts, etc.), as this might not be expected by the user on a new
device.
