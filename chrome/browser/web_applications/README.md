# Web Apps

This directory holds the core of Chromium's web app system. For a quick code
starting point see [WebAppProvider::Start()](web_app_provider.h), this is the
entry point where everything web app related begins.


## What are web apps?

Simply put web apps are sites that the user installs onto their machine
mimicking a native app install on their respective operating system.

### User entry points

Sites that meet our install promotion requirements will have an install prompt
appear in the omnibox on the right.  
 - Example site: https://developers.google.com/  
Users can also install any site they like via `Menu > More tools > Create
shortcut...`.


### Developer interface

Sites customise how their installed site integrates at the OS level using a
[web app manifest](https://www.w3.org/TR/appmanifest/).  
See developer guides for in depth overviews:
 - https://web.dev/progressive-web-apps/
 - https://web.dev/codelab-make-installable/


## What makes up Chromium's implementation?

The task of turning web sites into "apps" in the user's OS environment has
surprisingly many parts to it. Here are some (but not all) of the key ones.


### [`WebAppProvider`](web_app_provider.h)

This is a per-profile object housing all the various web app subsystems. This is
the "main()" of the web app implementation where everything starts.


### [`WebApp`](web_app.h)

This is the representation of an installed web app in RAM. Its member fields
largely reflect all the ways a site can configure their
[web app manifest](https://www.w3.org/TR/appmanifest/) plus miscellaneous
internal bookkeeping and user settings.


### [`WebAppRegistrar`](web_app_registrar.h)

This is where all the [`WebApp`](web_app.h)s live in memory and what many other
subsystems query to look up any given web app's fields. Mutations to the
registry have to go via [`WebAppSyncBridge`](web_app_sync_bridge.h).

Why is it full of `GetAppXYZ()` getters for every field instead of just
returning a `WebApp` reference? Because web apps used to be backed by
`Extension`s and in that mode there were no `WebApp`s; instead everything was
stored on an `Extension`. See `WebAppRegistrar`'s sibling
[`BookmarkAppRegistrar`](extensions/bookmark_app_registrar.h) for that
implementation. Since the backing object was decided based on the feature flag
`kDesktopPWAsWithoutExtensions` the rest of the logic had to go through a
generic interface [`AppRegistrar`](components/app_registrar.h) with a separate
getter for each field.  
Note that the Extensions based implementation is obsolete and needs cleaning up.


### [`WebAppInstallManager`](web_app_install_manager.h)

This is where web apps are created, updated and removed. The install manager
spawns [`WebAppInstallTask`](web_app_install_task.h)s for each "job".

Installation comes in many different forms from a simple "here's all the
[info](components/web_application_info.h) necessary please install it" to
"please install the site currently loaded in this web contents and fetch all the
manifest data it specifies" with a few inbetweens.


### [`ExternallyManagedAppManager`](components/externally_managed_app_manager.h)

This is for all installs that are not initiated by the user. This includes
[preinstalled apps](preinstalled_web_app_manager.h),
[policy installed apps](policy/web_app_policy_manager.h) and
[system web apps](system_web_apps/system_web_app_manager.h).

These all specify a set of [install URLs](components/external_install_options.h)
which the `ExternallyManagedAppManager` synchronises the set of currently
installed web apps with.


### [`WebAppInstallFinalizer`](web_app_install_finalizer.h)

This is the tail end of the installation process where we write all our web app
metadata to [disk](web_app_database.h) and deploy OS integrations (like
[desktop shortcuts](components/web_app_shortcut.h) and
[file handlers](components/file_handler_manager.h)) using the
[`OsIntegrationManager`](components/os_integration_manager.h).


### [`WebAppUiManager`](../ui/web_applications/web_app_ui_manager_impl.h)

Sometimes we need to query window state from chrome/browser/ui land even though
our BUILD.gn targets disallow this as it would be a circular dependency. This
[abstract class](web_app_ui_manager.h) + [impl](web_app_ui_manager_impl.h)
injects the dependency at link time (see
[`WebAppUiManager::Create()`](https://source.chromium.org/search?q=WebAppUiManager::Create)'s
declaration and definition locations).


## Debugging

Use [chrome://internals/web-app](chrome://internals/web-app) to inspect internal
web app state.
