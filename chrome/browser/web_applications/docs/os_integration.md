# [Web Apps](../README.md) - Operating System Integration

The WebAppProvider system has to provide a lot of integrations with operating
system surfaces for web apps. This functionality is usually different per
operating system, and is usually invoked through the
[`OsIntegrationManager`](/chrome/browser/web_applications/os_integration/os_integration_manager.h).

The
[`OsIntegrationManager`](/chrome/browser/web_applications/os_integration/os_integration_manager.h)'s
main responsibility is support the following operations:

1. Install operating system integration for a given web app.
2. Update operating system integration for a given web app.
3. Uninstall/remove operating system integration for a given web app.

It owns sub-managers who are responsible for each individual operating system
integration functionality (e.g.
[`web_app_file_handler_manager.h`](/chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h)
which owns the file handling feature). That manager will implement the
non-os-specific logic, and then call into functions that have os-specific
implementations (e.g.
`web_app_file_handler_registration.h/_mac.h/_win.h/_linux.h` files).

Below are sections describing how each OS integration works.

## Protocol Handler

The Protocol Handler component is responsible for handling the registration and
unregistration of custom protocols for web apps
([explainer](https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/URLProtocolHandler/explainer.md)).

### WebAppProtocolHandlerManager

The entrypoint to the component is `web_app_protocol_handler_manager.cc`, which
contains methods for registering and unregistering custom protocol handlers for
web apps, as well as any other logic related to protocol handling that should be
OS-agnostic - such as URL translation (translating from a protocol URL to a web
app URL) and interactions with the `ProtocolHandlerRegistry`, the browser
component that stores information about handlers internally.

`WebAppProtocolHandlerManager` is also responsible for leveraging the
web_app_protocol_handler_registration API to register and unregister protocols
with the underlying OS, so that custom protocols for a web app can be used from
outside the browser.

### web_app_protocol_handler_registration

`web_app_protocol_handler_registration` exposes a simple API that has
OS-specific files / implementations. This API communicates with other components
responsible for directly managing OS integrations. For example, on Windows this
interacts with the `ShellUtil` class, which manages the interactions with the
Windows registry for us.

Since some of these interactions with the OSes can be a bit costly, we try to do
as much as possible off the main thread. For the Windows implementation, we post
a task and wait for confirmation before proceeding with other steps (such as
updating the browser internal registry).

### ProtocolHandlerRegistry

Protocol handlers, like other Web App features, interact both with the OS and
the browser. On the browser, `protocol_handler_registry.cc` stores and manages
all protocol handler registrations, for both web apps and web sites (registered
via the HTML5 `registerProtocolHandler` API). The `WebAppProtocolHandlerManager`
is responsible for keeping both the OS and the browser in sync, by ensuring OS
changes are reflected in the browser registry accordingly.

### Flow of execution for a protocol registration call (Windows)

The flow of execution is similar for both registrations and unregistrations, so
we only describe registrations below.

1. Other components (typically the `OSIntegrationManager`) call into
   WebAppProtocolHandlerManager:

```cpp
protocol_handler_manager_->RegisterOsProtocolHandlers(app_id);
```

2. `WebAppProtocolHandlerManager` forwards that to
   `web_app_protocol_handler_registration`, which is OS specific. That API
   registers protocols with the OS via `ShellUtil` utilities off the main
   thread:

```cpp
base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterProtocolHandlersWithOSInBackground, app_id,
                     base::UTF8ToUTF16(app_name), profile, profile->GetPath(),
                     protocol_handlers, app_name_extension),
      base::BindOnce(&CheckAndUpdateExternalInstallations, profile->GetPath(),
                     app_id));
```

3. Once the protocols are registered successfully, a callback is invoked inside
   `RegisterProtocolHandlersWithOSInBackground` which then completes the
   registration with the `ProtocolHandlerRegistry`.

```cpp
ProtocolHandlerRegistry* registry =
    ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);

registry->RegisterAppProtocolHandlers(app_id, protocol_handlers);
```

4. `CheckAndUpdateExternalInstallations` is then called as the reply to the task
   in 2) and checks if there is an installation of this app in another profile
   that needs to be updated with a profile specific name and executes required
   update.

## Linux OS Integration

On Linux, OS integration primarily relies on creating `.desktop` files and
registering them along with other resources (like icons and MIME types) using
standard `xdg` utilities.

### Shortcuts

When a web app is installed and a shortcut is requested (e.g., on the desktop or
in the applications menu), the `web_app_shortcut_linux.cc` implementation takes
the following steps:

1. **Icon Installation**: The app's icons are saved as temporary `.png` files.
   For each size, `xdg-icon-resource install` is called to register the icon
   globally for the user. This allows the `.desktop` file to reference the icon
   by its registered name.
2. **`.desktop` File Generation**: A `.desktop` file string is generated (via
   `shell_integration_linux::GetDesktopFileContents`). This file contains proper
   instructions for the desktop environment, such as executing the browser with
   the correct app ID, the app's title, the installed icon's name, and any file
   handling MIME types. It also includes:
   - `StartupWMClass`: A generated window manager class name to ensure the
     desktop environment matches the launched application window to the correct
     launcher icon.
   - `Actions`: Defines any app shortcuts (right-click menu items on the app
     icon).
   - `NoDisplay=true`: Included if the shortcut is meant to be hidden from
     user-facing menus.
3. **Location-Specific Installation**:
   - **Desktop**: The `.desktop` file is written directly to the user's desktop
     directory (e.g., `~/Desktop`).
   - **Run on OS Login (Autostart)**: If the app is configured to launch on OS
     login, the `.desktop` file is written directly to the user's autostart
     directory (e.g., `~/.config/autostart`).
   - **Applications Menu**: For the system application menu,
     `xdg-desktop-menu install` is invoked with the `.desktop` file. Following
     this, `update-desktop-database` is executed on the user's applications
     directory. This cache update is necessary for file managers (like Nautilus
     or Nemo) to recognize any file handler associations declared in the
     `.desktop` file.

### DynamicLauncher Portal

Inside a Flatpak sandbox, Chrome can't write `.desktop` files into the host's
applications directory. In that case the applications menu launcher is installed
through the
[XDG DynamicLauncher portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.DynamicLauncher.html)
(`dynamic_launcher_portal.cc`) instead. The portal is used only when
`kWebAppsUseDynamicLauncherPortal` is enabled and `base::nix::GetAppSandbox()`
reports Flatpak (see `ShouldUseDynamicLauncherPortal()`). Host installs keep
using the `xdg` utilities described above. So do snaps, since the portal doesn't
support them
([xdg-desktop-portal#2014](https://github.com/flatpak/xdg-desktop-portal/issues/2014)).

The rest of this section calls that `xdg`-based implementation (see
[Shortcuts](#shortcuts)) the *legacy path*. It is the only path outside a
sandbox, and the fallback inside one whenever no portal token is available.
Inside a sandbox it doesn't produce a usable launcher: the `xdg` utilities and
the XDG data directory are those of the sandbox, not the host, so the `.desktop`
file either isn't written or ends up in the sandbox's private data directory,
and the app doesn't appear in the host's applications menu. The app itself is
still installed and can be launched from Chrome (e.g. `chrome://apps`), since
failing to create OS shortcuts doesn't fail the install.

The portal flow has two steps:

1. **`PrepareInstall` (preflight)**: The portal shows its own confirmation
   dialog with the app's name and icon (Chrome passes `editable_name=false`, so
   the name stays in sync with the app), and returns a token on approval. The
   token has to come from a user-visible dialog, so
   `FinalizeInstallOrUpdateJob::Start()` requests it before anything is
   committed to the web app database (see `ShouldRunPortalPreflight()` and
   `PreparePortalInstallOnUIThread()`). This preflight runs only for user
   installs that get OS integration. If the user cancels, the install fails with
   `kUserInstallDeclined` and nothing is written. If the portal is unavailable,
   the token is empty and the legacy path is used.
2. **`Install`**: The token is passed through `SynchronizeOsOptions` and
   `ShortcutInfo` to `CreateShortcutInApplicationsMenu()`, which calls the
   portal's `Install` with it. The desktop file ID is the shortcut filename
   prefixed with Chrome's own application ID (`GetPortalDesktopFileId()`, e.g.
   `org.chromium.Chromium.chrome-<app_id>-<profile>.desktop`), and the desktop
   entry is stripped of `Name`, `Icon` and `Actions` (the portal takes the name
   and icon from `PrepareInstall` and allows only a single `[Desktop Entry]`
   group). The portal writes the launcher and its icon itself. Without a token,
   the legacy path runs. If the portal's `Install` call fails, the legacy path
   runs too, and `Apps.CreateShortcuts.Linux.Result` records
   `kPortalInstallFailed`.

**Silent updates skip the preflight.** Silent manifest updates (for example
small icon changes, `FinalizeJobOptions::is_silent_update`) can't show a dialog,
and `RequestInstallToken`, which issues tokens without a dialog, is limited to
desktop-environment components and not available to Chrome. So these updates
don't get a token: the rest of the app (file handlers, scope extensions, etc.)
is still updated, but an existing portal-installed launcher is left untouched
rather than being replaced by a legacy launcher, which wouldn't work in the
sandbox. Other installs and updates that don't go through this job, such as
forced migrations, still use the legacy path (https://crbug.com/543925322).

**Uninstall**: `DeleteShortcutInApplicationsMenu()` calls the portal's
`Uninstall`, which doesn't need a token. If the portal is unavailable or the
call fails (for example for launchers installed before the portal was used), the
legacy uninstall runs, which removes both the portal-named and the legacy
`.desktop` files.

### File Handlers

Registering file handlers on Linux (see
`web_app_file_handler_registration_linux.cc`) involves updating the system's
MIME info database:

1. An XML string containing the MIME type registrations is generated and written
   to a temporary file.
2. `xdg-mime install` is executed to register the MIME types with the system.
3. Finally, as with shortcut creation, `update-desktop-database` is called to
   refresh the desktop cache so that the new file associations take effect
   immediately in the user's file manager.

> **Note on FreeDesktop Protocol vs. File Handlers:** Under FreeDesktop
> specifications, URL scheme associations are stored in `.desktop` entries using
> pseudo-MIME types prefixed with `x-scheme-handler/` (e.g.
> `x-scheme-handler/mailto`). These are exclusively protocol handlers and are
> filtered out during file handler registration, so they never appear in
> generated shared MIME-info XML files or file handler `.desktop` shortcuts.
> Legitimate `x-scheme-handler/*` associations are populated solely from
> validated `protocol_handlers`.
