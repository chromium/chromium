This directory contains code used by WebView and WebContent[1] of iOS Blink to
inject javascript from the browser to the renderer, as well as a simple message
port style API.

As there is not an iOS builder blocking CQ, it is recommended to manually
trigger an `ios-blink-rel-fyi` trybot job when making changes to detect
breakages.

[1] https://source.chromium.org/chromium/chromium/src/+/main:ios/web/content/js_messaging/
