An updater for desktop client software using Chromium code and tools.

*   The code lives in //chrome/updater.
*   The documentation lives in //docs/updater.
    *   Deprecated Design Doc: https://bit.ly/chromium-updater
*   Please join [chrome-updates-dev@chromium.org](https://groups.google.com/a/chromium.org/g/chrome-updates-dev) or
https://chromium.slack.com#updater for topics related to the project.

The mission of the updater is to keep Chrome (and other software) up to date.

The updater is built from a common, platform neutral code base, as part of
the Chrome build. The updater is a drop-in replacement for Google
Update/Omaha/Keystone and can be customized by 3rd party embedders to
update non-Google client software, such as Edge.

The desktop platforms include Windows, macOS, Linux.
