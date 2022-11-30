# src/chromeos/ash/components/dbus

This directory contains ash-specific D-Bus client libararies and utility codes.
Most of clients in this directory have been moved from `src/chromeos/dbus`.

## DBusThreadManager

The DBusThreadManager class was originally created to both own the D-Bus
base::Thread, the system dbus::Bus instance, and all of the D-Bus clients.

With the significantly increased number of clients, this model no longer makes
sense.

New clients should not be added to DBusThreadManager but instead should follow
the pattern described in [src/chromeos/dbus/README.md].

[src/chromeos/dbus/README.md]: https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/dbus/README.md
