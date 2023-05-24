# Hermes' D-Bus Client Library

Hermes' D-Bus client library allows Chrome to make API calls to
[Hermes](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=64;drc=4a50d13fc73268ef4a27cf67dc1eff40ea6f997a),
ChromeOS' eSIM configuration manager.

This document describes the various D-Bus clients that utilize Hermes' D-Bus
interfaces, the **Manager Client**, **EUICC Client**, and **Profile Client**.

### Initialization and Shutdown

The D-Bus clients are initialized in the order of Profile client, EUICC client,
then Manager client when Ash D-Bus clients are initialized after
[Chrome's early initialization](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:chrome/app/chrome_main_delegate.cc;l=755;drc=90b428dcab63b652cc91107b81d2758270e92ac0).

For extensions, the clients are initialized after the Shell Browser's
[main message loop is created](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:extensions/shell/browser/shell_browser_main_parts.cc;l=129;drc=90b428dcab63b652cc91107b81d2758270e92ac0).

The clients are shutdown with the rest of the Ash D-Bus clients after Chrome
[destroys its threads](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1706;drc=90b428dcab63b652cc91107b81d2758270e92ac0).

### Constants

Success and error codes, along with helpful utility functions related to these
codes can be found in [hermes_response_status.h](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_response_status.h;l=14;drc=07384a913575f611a42f063e7e273ac499af9ef1).

There are also [several constants](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/constants.h;l=10;drc=789bec586d89e87ccb30ba132a12da2dd99b42e3)
which are shared between the D-Bus clients to implement a consistent timeout
for D-Bus calls.

### Hermes Manager Client
TODO

### Hermes EUICC Client
TODO

### Hermes Profile Client
TODO
