# Test Chrome apps and extensions

The fake web store implemented by
[`FakeCWS`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/chrome/browser/ash/app_mode/fake_cws.h)
can be configured to host Chrome apps and extensions for Kiosk tests.

This directory contains the source code, signing keys, and packed CRX files of
Chrome apps and extensions accessible to `FakeCWS` in automated tests.

### How to add a new test Chrome app or extension?

1.  Pack your app or extension in the chrome://extensions page.
2.  Save `<app_id>.pem` file to the app source directory inside
    `apps_and_extensions`.
3.  Save `<app_id>.crx` file to `webstore/downloads/` directory.
4.  Create `<app_id>.textproto` file in `webstore/itemsnippet/` directory. See
    other files in that directory for more details.
5.  In `webstore/itemsnippet/BUILD.gn`, add `<app_id>.textproto` to `sources` so
    a protobuf string API response can be generated from the `.textproto` file.

TODO(crbug.com/325314721): `webstore/inlineinstall/detail` is deprecated and no
longer used for tests. The directory will be removed when the item snippets API
is fully rolled out.

### How to use test Chrome Apps or extensions?

To use a Chrome app in the test code, you need to know app id. You can find app
id in the app source directory: `<app_id>.pem`.

As an example, see usage of "Kiosk base test app" with id
`epancfbahpnkphlhpeefecinmgclhjlj` in `ChromeAppKioskLacrosTest` browser tests.
