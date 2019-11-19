# src/chromeos/dbus

This directory contains client libraries for communication with Chrome OS
system service daemons over D-Bus.

For more information, see [Chrome OS D-Bus Usage in Chrome].

## DBusThreadManager

The DBusThreadManager class was originally created to both own the D-Bus
base::Thread, the system dbus::Bus instance, and all of the D-Bus clients.

With the significantly increased number of clients and upcoming [Mash] effort
splitting Chrome and Ash into separate processes, this model no longer makes
sense.

New clients should not be added to DBusThreadManager but instead should follow
the pattern described below. DBusThreadManager will eventually be deprecated.

## D-Bus Client Best Practices

An example of a relatively simple client using existing patterns can be found
in [src/chromeos/dbus/kerberos].

*   Create a subdirectory under `src/chromeos/dbus` for new clients or use an
    existing directory. Do not add new clients to this directory.

*   D-Bus clients should only be accessed by a single process, e.g. Chrome or
    Ash (preferably Ash), not both. Use a mojo interface to communicate between
    Chrome and Ash where necessary.

*   D-Bus clients are explicitly initialized and shut down. They provide a
    static getter for the single global instance. In Chrome, initialization
    occurs in [dbus_helper.cc]. In Ash it occurs in [ash_service.cc].

*   For new clients, if test methods are required, create a `TestInterface` in
    the base class with a virtual `GetTestInterface()` method and implement it
    only in the fake (return null in the real implementation). See
    [src/chromeos/dbus/kerberos] for an example.

    (Many existing clients provide additional test functionality in the fake
    implementation, however this complicates tests and the fake implementation).

*   These clients do not have any dependency on FeatureList, and care should be
    taken regarding initialization order if such dependencies are added (see
    BluezDBusManager for an example of such client).

## Shill clients

Shill clients will eventually only be available to Chrome. As such, the
DBusThreadManager::GetShill*Client() methods have been left intact for now.
However, the clients are no longer owned by DBusThreadManager so that they can
be initialized independently.

New code should prefer Shill*Client::Get() over the DBusThreadManager accessors.

## Older clients that have been removed:

*   Amplifier (`amplifier_client.cc`)
*   Audio DSP (`audio_dsp_client.cc`)
*   Introspection (`introspectable_client.cc`)
*   NFC (`nfc_manager_client.cc`)
*   peerd (`peer_daemon_manager_client.cc`)
*   privetd (`privet_daemon_manager_client.cc`)
*   Wi-Fi AP manager (`ap_manager_client.cc`)

[Chrome OS D-Bus Usage in Chrome]: https://chromium.googlesource.com/chromiumos/docs/+/master/dbus_in_chrome.md
[Mash]: https://chromium.googlesource.com/chromium/src/+/HEAD/ash/README.md
[src/chromeos/dbus/kerberos]: https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/dbus/kerberos
[dbus_helper.cc]: https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/chromeos/dbus/dbus_helper.cc
[ash_service.cc]: https://chromium.googlesource.com/chromium/src/+/HEAD/ash/ash_service.cc
