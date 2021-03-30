This directory holds files for components that implement features for
Progressive Web Apps on the browser side.

# Components

## Protocol Handler

The Protocol Handler component is responsible for handling the registration and
unregistration of custom protocols for web apps ([explainer](https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/URLProtocolHandler/explainer.md)).

### ProtocolHandlerManager

The entrypoint to the component is `protocol_handler_manager.cc`, which contains
methods for registering and unregistering custom protocol handlers for web apps,
as well as any other logic related to protocol handling that should be
OS-agnostic - such as URL translation (translating from a protocol URL to a web
app URL) and interactions with the `ProtocolHandlerRegistry`, the browser
component that stores information about handlers internally.

`ProtocolHandlerManager` is also responsible for leveraging the
web_app_protocol_handler_registration API to register and unregister protocols
with the underlying OS, so that custom protocols for a web app can be used
from outside the browser.

### web_app_protocol_handler_registration
`web_app_protocol_handler_registration` exposes a simple API that has
OS-specific files / implementations. This API communicates with other components
responsible for directly managing OS integrations. For example, on Windows this
interacts with the `ShellUtil` class, which manages the interactions with the
Windows registry for us.

Since some of these interactions with the OSes can be a bit costly, we try to do
as much as possible off the main thread. For the Windows implementation, we
post a task and wait for confirmation before proceeding with other steps (such
as updating the browser internal registry).

### ProtocolHandlerRegistry
Protocol handlers, like other Web App features, interact both with the OS and
the browser. On the browser, `protocol_handler_registry.cc` stores and manages
all protocol handler registrations, for both web apps and web sites (registered
via the HTML5 `registerProtocolHandler` API). The `ProtocolHandlerManager` is
responsible for keeping both the OS and the browser in sync, by ensuring OS
changes are reflected in the browser registry accordingly.

### Flow of execution for a protocol registration call (Windows)

The flow of execution is similar for both registrations and unregistrations, so
we only describe registrations below.

1) Other components (typically the `OSIntegrationManager`) call into
ProtocolHandlerManager:

```cpp
protocol_handler_manager_->RegisterOsProtocolHandlers(app_id);
```

2) `ProtocolHandlerManager` forwards that to
`web_app_protocol_handler_registration`, which is OS specific. That API registers
protocols with the OS via `ShellUtil` utilities off the main thread:

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

3) Once the protocols are registered successfully, a callback is invoked inside
`RegisterProtocolHandlersWithOSInBackground` which then completes the
registration with the `ProtocolHandlerRegistry`.

```cpp
ProtocolHandlerRegistry* registry =
    ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);

registry->RegisterAppProtocolHandlers(app_id, protocol_handlers);
```

4) `CheckAndUpdateExternalInstallations` is then called as the reply to the task
in 2) and checks if there is an installation of this app in another profile that
needs to be updated with a profile specific name and executes required update.


