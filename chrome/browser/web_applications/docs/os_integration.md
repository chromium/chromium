# [Web Apps](../README.md) - Operating System Integration

The WebAppProvider system has to provide a lot of integrations with operating
system surfaces for web apps. This functionality is usually different per
operating system, and is usually invoked through the
[`OsIntegrationManager`](/chrome/browser/web_applications/os_integration/os_integration_manager.h).

The
[`OsIntegrationManager`](/chrome/browser/web_applications/os_integration/os_integration_manager.h)'s
main responsibility is support the following operations:

1. Install operating system integration for a given web app.
1. Update operating system integration for a given web app.
1. Uninstall/remove operating system integration for a given web app.

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
1. **`.desktop` File Generation**: A `.desktop` file string is generated (via
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
1. **Location-Specific Installation**:
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

### File Handlers

Registering file handlers on Linux (see
`web_app_file_handler_registration_linux.cc`) involves updating the system's
MIME info database:

1. An XML string containing the MIME type registrations is generated and written
   to a temporary file.
1. `xdg-mime install` is executed to register the MIME types with the system.
1. Finally, as with shortcut creation, `update-desktop-database` is called to
   refresh the desktop cache so that the new file associations take effect
   immediately in the user's file manager.
