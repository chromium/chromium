This directory contains hard coded preinstalled web app configs to be default
installed in Chrome branded builds.

### Configs

Preinstalled web apps are configured in two ways:
- A hard coded set of apps in
  [`GetPreinstalledWebApps()`](preinstalled_web_apps.h).
- JSON configs on device:
  - Command line arg `--preinstalled-web-apps-dir`
    - Only used for testing.
    - Works on all platforms.
  - [`chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS`](https://source.chromium.org/search?q=DIR_STANDALONE_EXTERNAL_EXTENSIONS)/web_apps
    - Being removed in feature PreinstalledWebAppsCoreOnly (b/341824938).
    - Chrome OS only (ozone included).
    - `/usr/share/google-chrome/extensions/web_apps` for branded builds.
    - `/usr/share/chromium/extensions/web_apps` for unbranded builds.
    - Configs come from:
      https://chrome-internal.googlesource.com/chromeos/overlays/chromeos-overlay/+/main/chromeos-base/chromeos-default-apps
  - Command line arg `--extra-web-apps-dir`
    - Used as subdirectory of above directory.
    - Chrome OS only.
    - Used by specific board images to add board specific default web apps.

### Icons

The icon bitmaps bundled with the configs are not suitable to include in an open
source repository and are stored in an internal repo:
https://chrome-internal.googlesource.com/chrome/components/default_apps.git

This internal repo only gets checked out for internal Chromium checkouts.

Icon bitmaps get checked out at:
`chrome/browser/resources/preinstalled_web_apps/internal`

Icons are packaged into the build via:
[`chrome/browser/resources/preinstalled_web_apps/resources.grd`](../../resources/preinstalled_web_apps/resources.grd)

### ChromeOS PreinstalledWebAppsCoreOnly

Feature `PreinstalledWebAppsCoreOnly` is part of work to switch ChromeOS from
using PreinstalledWebAppManager to AppPreloadService. The feature changes
ChromeOS to only install the same core apps which are configured for all
platforms. Once the feature is fully rolled out, it is expected that any
ChromeOS-specific code can be removed.
