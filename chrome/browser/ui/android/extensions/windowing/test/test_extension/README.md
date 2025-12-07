# `chrome.windows` Test Extension

This folder contains a test extension for `chrome.windows` APIs.

## Load the extension into Chrome Android

* Build the "desktop Android" APK (set the GN arg `is_desktop_android=true`).

* Install the APK on a tablet.

* Push everything in this folder to your device:
  * Create a folder:

    `$ adb shell mkdir /sdcard/test_extension`

  * Push all files:

    `$ adb push ./chrome/browser/ui/android/extensions/windowing/test/test_extension/* /sdcard/test_extension`


* Open `chrome://extensions`.

* Enable "Developer mode" on the top right corner.

* Click "Load unpacked" on the top left corner.

* Select the `test_extension` folder on your device, and click "Use this folder" at the bottom of the window.

* You should see the test extension under "All Extensions" and its icon on the toolbar.
