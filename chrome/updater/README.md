An updater for desktop client software using Chromium code and tools.

* The code lives in //chrome/updater.
* Design Doc: https://bit.ly/chromium-updater
* Please join chrome-updates-dev@chromium.org or
https://chromium.slack.com#updater for topics related to the project.

The updater will be built from a common, platform neutral code base, as part of
the Chrome build. The updater is going to be a drop-in replacement for Google
Update/Omaha/Keystone and could be customized by 3rd party embedders to for
updating non-Google client software, such as Edge.

The desktop platforms include Windows, macOS, Linux.

There are many reasons to start a new code base for this:

* Reducing the development cost and the code duplication among platforms.
* Implementing update algorithms consistently and correctly: checking for
updates, applying updates, gathering metrics, and load shedding.
* Use world-class developer tool chains for build, security, and stability.

The existing Omaha/Keystone design, implementation, and production issues apply
to this project as well.
