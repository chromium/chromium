# Chrome OS

This directory contains low-level support for Chrome running on Chrome OS. Many
subdirectories contain Chrome-style C++ wrappers around operating system
components.

For example, //chromeos/dbus contains wrappers around the D-Bus interfaces to
system daemons like the network configuration manager (shill). Most other
directories contain low-level utility code. For example, //chromeos/disks has
utilities for mounting and unmounting disk volumes.

There are two exceptions:

- //chromeos/services contains mojo services that were not considered
  sufficiently general to live in top-level //services. For example
  //chromeos/services/secure_channel bootstraps a secure communications channel
  to an Android phone over Bluetooth, enabling multi-device features like
  instant tethering.

- //chromeos/components contains C++ components that were not considered
  sufficiently general to live in top-level //components. For example,
  //chromeos/components/account_manager manages the user's GAIA accounts, but
  is used as the backend for UI that only exists on Chrome OS devices.

Note, //chromeos does not contain any user-facing UI code, and hence it has
"-ui" in its DEPS. The contents of //chromeos should also not depend on
//chrome or //content.
