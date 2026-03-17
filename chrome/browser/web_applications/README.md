# Web Apps on Desktop

See [presentation slides](https://tinyurl.com/dpwa-architecture-public) about the WebAppProvider system architecture.

## Debugging

Use `chrome://web-app-internals` (generated [here](https://source.chromium.org/search?q=WebAppInternalsHandler::BuildDebugInfo)) to inspect internal web app state.
Test failures will print this information out automatically to help with debugging.

The codebase has a number of useful DVLOGs (like in `web_app_command_manager.cc` and `web_app_lock_manager.cc`). Use the normal vmodule command line args to see these (e.g. `--vmodule=web_app*=1`).

For developers wanting to test the behavior of the web app itself, Chrome DevTools Protocol can be used. See [Instruction of using PWA via CDP](docs/cdp-integration.md).

## Documentation Guidelines

- Markdown documentation (files like this):
  - Contains information that can't be documented in class-level documentation.
  - Answers questions like: What is the goal of a group of classes together? How does a group of classes work together?
  - Explains concepts that are used across different files.
  - Should be unlikely to become out-of-date.
    - Any source links should link to a codesearch 'search' page and not the specific line number.
    - Avoid implementation details.
- Class-level documentation (documentation in header files):
  - Answers questions like: Why does this class exist? What is the responsibility of this class? If this class involves a process with stages, what are those stages / steps?
  - Should be updated actively when that given file is changed.
- Documentation inside of methods should only be used to explain the "why" of code if it is not obvious.

## What makes up Chromium's implementation?

The task of turning websites into "apps" in the user's OS environment has many parts to it. Before going into the parts, here is where they live:

![](docs/webappprovider_component_ownership.jpg)

See the drawing source [here](https://docs.google.com/drawings/d/1TqUF2Pqh2S5qPGyA6njQWxOgSgKQBPePKPIH_srGeRk/edit?usp=sharing).

- The `WebAppProvider` core system lives on the `Profile` object.
- The `WebAppUiManagerImpl` also lives on the `Profile` object (to avoid deps issues).
- The `AppBrowserController` (typically `WebAppBrowserController` for our interests) lives on the `Browser` object.
- The `WebAppTabHelper` lives on the `WebContents` object.

While most on-disk storage is done in the [`WebAppSyncBridge`](#webappsyncbridge), the system also sometimes uses the `PrefService`. Most of these prefs live on the `Profile` (`profile->GetPrefs()`), but some prefs are in the global browser prefs (`g_browser_process->local_state()`).

Presentation: [https://tinyurl.com/dpwa-architecture-public](https://tinyurl.com/dpwa-architecture-public)

Older presentation: [https://tinyurl.com/bmo-public](https://tinyurl.com/bmo-public)

## Architecture Philosophy

- Tests (especially browser tests / integration tests) should generally operate on the [public interface](#public-interface) as much as possible. Unit tests can touch internals where convenient to set up initial state, but generally still test the operations via the public interface.
- [External dependencies](#external-dependencies) should be behind fake-able interfaces, allowing unit & browser tests to swap these out. However, internal parts of our system should not be mocked out or faked - this tightly couples the internal implementation to our tests. If it is impossible to trigger a condition with the public interface, then that condition should be removed (or the public interface improved).
  - See [this presentation](https://www.youtube.com/watch?v=EZ05e7EMOLM) about testing that might clarify our approach.

## Usage

The safest way to use the WebAppProvider system is using the `WebAppCommandScheduler` (via `WebAppProvider::scheduler()`), which serves as an entry point for operations on the system for safely reading or writing state. Unsafe state access is available via `WebAppProvider::registrar_unsafe()`, but this in not guaranteed to be consistent as an async operation could be occurring at any time (install, uninstall, update, etc).

For information about creating safe read/write operations on the system, see the [commands README.md](/chrome/browser/web_applications/commands/README.md).

## External Dependencies

The goal is to have all of these behind an abstraction that has a fake to allow easy unit testing of our system. Some of these dependencies are behind a nice fake-able interface, and some are not (yet).

- **Extensions** - Some of our code still talks to the extensions system,
  specifically the `PreinstalledWebAppManager`.
- **`content::WebContents`**: The WebAppProvider system interacts with
  `content::WebContents` for various tasks like loading URLs (via
  `WebAppUrlLoader`), retrieving web app manifest data and icons (via
  `WebAppDataRetriever` and `WebAppIconDownloader` respectively), and observing
  navigations and destruction. The `WebContentsManager` serves as a centralized
  point of dependency for these interactions and acts as a factory for these
  components, allowing for easier management and faking in tests via the `FakeWebContentsManager`.
- **OS Integration**: Each OS integration has fairly custom code on each OS to
  do the operation. The `OsIntegrationManger` and the respective sub-managers own this.
- **Sync system**: There is a tight coupling between our system and the sync
  system through the WebAppSyncBridge. Faking this is easy and is handled by
  the `FakeWebAppProvider`.
- **UI**: There are parts of the system that are coupled to UI, like showing
  dialogs, determining information about app windows, etc. These are put behind
  the `WebAppUiManager`, and faked by the `FakeWebAppUiManager`.
- **Policy**: Our code depends on the policy system setting its policies in
  appropriate prefs for us to read. Because we just look at prefs, we don't
  need a "fake" here.

## Databases / sources of truth

These store data for our system. Some of it is per-web-app, and some of it is global.

- **`WebAppRegistrar`**: This attempts to unify the reading of much of this data, and also holds an in-memory copy of the database data (in WebApp objects).
- **`WebAppDatabase`** / **`WebAppSyncBridge`**: This stores the web_app.proto object in a database, which is the preferred place to store information about a web app.
- **Icons on disk**: These are managed by the `WebAppIconManager` and stored on disk in the user's profile.
- **Prefs**: The `PrefService` is used to store information that is either global, or needs to persist after a web app is uninstalled. Most of these prefs live on the `Profile` (`profile->GetPrefs()`), but some prefs are in the global browser prefs (`g_browser_process->local_state()`). Some users of prefs:
  - AppShimRegistry
  - UserUninstalledPreinstalledWebAppPrefs
- **OS Integration**: Various OS integration requires storing state on the operating system. Sometimes we are able to read this state back, sometimes not.

Accessing any of this information without an applicable 'lock' on the system is considered unsafe.

## Relevant Classes & Managers

The **[`WebAppProvider`](/chrome/browser/web_applications/web_app_provider.h)** is the per-profile object housing most of the various web app subsystems, acting as the "main()" of the web app implementation where everything starts. Unit tests use the `FakeWebAppProvider` version which allows tests to swap out some managers with fakes (and does this by default for a few).

The objects that live on the WebAppProvider, often called 'Managers', are used to encapsulate common responsibilities or in-memory state that needs to be stored. See the respective header files for more detailed information:

- **`WebAppCommandManager` & `WebAppCommandScheduler`**: The entry point for performing asynchronous operations safely via locks.
- **`WebAppRegistrar`**: Provides a queryable in-memory view of all installed web apps.
- **`WebAppSyncBridge`**: Synchronizes the in-memory registrar with the on-disk database and Chrome Sync; faked with an in-memory database and sync disabled by default.
- **`WebAppInstallManager`**: Orchestrates the installation of web apps.
- **`ManifestUpdateManager`**: Monitors and applies updates to web app manifests.
- **`ExternallyManagedAppManager`**: Handles installations from external sources like policies or preinstalled configurations.
- **`WebAppPolicyManager`**: Manages apps installed via enterprise policy.
- **`PreinstalledWebAppManager`**: Manages the installation of default "preinstalled" web apps.
- **`WebAppIconManager`**: Manages the loading and storage of app icons on disk.
- **`OsIntegrationManager`**: Manages all integrations with the host operating system. `FakeOsIntegrationManager` is used by default in unit tests.
- **`WebAppUiManager`**: Interface for performing UI operations like showing dialogs. `FakeWebAppUiManager` is used by default in unit tests.
- **`WebContentsManager`**: Factory for WebContents-based dependencies, wrapping the WebContents / network dependency for the entire system. `FakeWebContentsManager` is used by default in unit tests.
- **`FileUtilsWrapper`**: Utility for file system access. `TestFileUtils` is used by default in unit tests.

Other relevant classes:

- **[`WebApp`](/chrome/browser/web_applications/web_app.h)**: The representation of an installed web app in RAM, reflecting how a site configures its web app manifest plus internal bookkeeping. This does not include information like policy information, so usage of this class is often discouraged over the WebAppRegistrar, which combined multiple sources of truth holistically.
- **[`AppShimRegistry`](/chrome/browser/web_applications/app_shim_registry_mac.h)**: (Mac-only) Stores state in Chrome's "Local State" (global preferences) to reason about installed PWAs across all profiles without loading those profiles into memory.

## Deep Dives

- [Installation Pipeline](docs/installation_pipeline.md)
- [Manifest Representations in Code](docs/manifest_representations.md)
- [Integration Testing Framework](docs/integration-testing-framework.md)
- [OS Integration](docs/os_integration.md)
- [Manifest Update Process](docs/manifest_update_process.md)
- [Isolated Web Apps](docs/isolated_web_apps.md)
- [WebUI Web App](docs/webui_web_app.md)
- [Why is this test failing?](docs/why-is-this-test-failing.md)
- [How to create WebAppIntegration Tests](docs/how-to-create-webapp-integration-tests.md)

## Testing

Please see [testing.md](docs/testing.md).