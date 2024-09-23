# src/chromeos/dbus

This directory contains client libraries for communication with Chrome OS
system service daemons over D-Bus.

For more information, see [Chrome OS D-Bus Usage in Chrome].

## D-Bus Client Best Practices

An example of a relatively simple client using existing patterns can be found
in [src/chromeos/dbus/dlp].

*   Create a subdirectory under `src/chromeos/dbus` for new clients or use an
    existing directory. If a new client is ash-specific, create a subdirectory
    under `src/chromeos/ash/components/dbus`. Do not add new clients to this
    directory. If Lacros wants to talk to D-Bus service, the default
    recommendation is to build a crosapi class in ash proxying requests to the
    D-Bus service, then let lacros talk to ash. If you need direct communication
    from lacros to the D-Bus service, please be consulted by lacros-team@.

*   D-Bus clients are explicitly initialized and shut down. They provide a
    static getter for the single global instance. In Ash Chrome, initialization
    occurs in [ash_dbus_helper.cc]. In Lacros Chrome, initialization occurs in
    [lacros_dbus_helper.cc].

*   Be careful when providing access to multiple processes (e.g. Ash Chrome and
    Lacros Chrome). Not all of the underlying daemons support multiple clients.

*   For new clients, if test methods are required, create a `TestInterface` in
    the base class with a virtual `GetTestInterface()` method and implement it
    only in the fake (return null in the real implementation). See
    [src/chromeos/dbus/dlp] for an example.

    (Many existing clients provide additional test functionality in the fake
    implementation, however this complicates tests and the fake implementation).

*   These clients do not have any dependency on FeatureList, and care should be
    taken regarding initialization order if such dependencies are added (see
    BluezDBusManager for an example of such client).

## Older clients that have been removed:

*   Amplifier (`amplifier_client.cc`)
*   Audio DSP (`audio_dsp_client.cc`)
*   Introspection (`introspectable_client.cc`)
*   NFC (`nfc_manager_client.cc`)
*   peerd (`peer_daemon_manager_client.cc`)
*   privetd (`privet_daemon_manager_client.cc`)
*   Wi-Fi AP manager (`ap_manager_client.cc`)

[Chrome OS D-Bus Usage in Chrome]: https://chromium.googlesource.com/chromiumos/docs/+/main/dbus_in_chrome.md
[src/chromeos/dbus/dlp]: https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/dbus/dlp
[ash_dbus_helper.cc]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/ash/dbus/ash_dbus_helper.cc
[lacros_dbus_helper.cc]: https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/lacros/lacros_dbus_helper.cc
[ash_service.cc]: https://chromium.googlesource.com/chromium/src/+/HEAD/ash/ash_service.cc
