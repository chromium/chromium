chrome/browser/ui/android/browser_instant_controller
==================

This directory contains Android code to bridge `BrowserInstantController` with
`ChromeAndroidTask`, so that a browser window's New Tab Pages are reloaded when
the default search engine changes (the Android counterpart of
`BrowserWindowFeatures` owning `BrowserInstantController` on desktop).
Note that this feature is only available for WebUI NTPs on Desktop Android.
