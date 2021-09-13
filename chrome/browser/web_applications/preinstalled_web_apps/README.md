This directory contains hard coded preinstalled web app configs to be default
installed in Chrome branded builds.

### Icons

The icon bitmaps bundled with the configs are not suitable to include in an open
source repository and are stored in an internal repo:
https://chrome-internal.googlesource.com/chrome/components/default_apps.git

This internal repo only gets checked out for internal Chromium checkouts.

Icon bitmaps get checked out at:
`chrome/browser/resources/preinstalled_web_apps/internal`

Icons are packaged into the build via:
[`chrome/browser/resources/preinstalled_web_apps/resources.grd`](../../resources/preinstalled_web_apps/resources.grd)