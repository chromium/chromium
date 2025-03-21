# Test Chrome apps and extensions

The fake web store implemented by
[`FakeCWS`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/chrome/browser/ash/app_mode/fake_cws.h)
can be configured to host Chrome apps and extensions for Kiosk tests.

This directory contains the source code, signing keys, and packed CRX files of
Chrome apps and extensions accessible to `FakeCWS` in automated tests.

### How to add a new test Chrome app or extension?

1.  Implement your app or extension under
    `apps_and_extensions/<app_name>/v<number>`.

2.  Pack your CRX.

    *   You can navigate to chrome://extensions and click "Pack extension".
    *   Or you can use Chrome on the command line to pack and generate a new key
        as follows:

    ```
    testing/xvfb.py out/Release/chrome \
      --pack-extension=</absolute/path/to/app>
    ```

    *   Or if you already have a key:

    ```
    testing/xvfb.py out/Release/chrome \
      --pack-extension=</absolute/path/to/app> \
      --pack-extension-key=</absolute/path/to/key.pem>
    ```

3.  Find the ID of your app.

    *   You can navigate to chrome://extensions, drag and drop the CRX onto the
        page, then find the ID in the entry for your app.
    *   Or you can use the command line as follows:

    ```
    openssl rsa -in <path/to/key.pem> -pubout -outform DER \
      | shasum -a 256 \
      | head -c32 \
      | tr 0-9a-f a-p
    ```

4.  Save the key file at `apps_and_extensions/<app_name>/<app_id>.pem`.

5.  Save the CRX file at `webstore/downloads/<app_id>.crx`.

6.  Create the `webstore/itemsnippet/<app_id>.textproto` file. See other files
    in that directory for reference.

7.  In `webstore/itemsnippet/BUILD.gn`, add `<app_id>.textproto` to `sources` so
    a protobuf string API response can be generated from the `.textproto` file.

### How to use test Chrome Apps or extensions?

It's recommended to use these apps and extensions with `KioskMixin` via the
helpers defined in `//chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h`.
`KioskMixin` configures `FakeCWS` to serve the test apps, which in turn will
read the files in `webstore/downloads`.

As an example, see usages of the "Kiosk App With Local Data" app with ID
`ckgconpclkocfoolbepdpgmgaicpegnp` in `KioskChromeAppDataUpdateTest` browser
tests.
