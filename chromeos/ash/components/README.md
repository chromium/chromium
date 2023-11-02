# About //chromeos/ash/components

This directory contains components that are used by ash-chrome only.
For C++ code, think of //chromeos/ash/components like top-level //components.
Specifically, each component should have its own DEPS to be isolated from
other components.

For example, //chromeos/ash/components/account_manager manages the user's GAIA
accounts, but only on behalf of ChromeOS code.
//components/account_manager_core contains cross-platform support for
accounts.

Some subdirectories contain low-level utility code. For example,
//chromeos/ash/components/disks has utilities for mounting and unmounting disk
volumes.

Much of this code used to live in //chromeos/components. The
[Lacros project](/docs/lacros.md) is extracting browser functionality into a
separate binary. As part of this migration, code used only by the ash-chrome
binary moved into "ash" directories. See the
[Chrome OS source directory migration](https://docs.google.com/document/d/1g-98HpzA8XcoGBWUv1gQNr4rbnD5yfvbtYZyPDDbkaE/edit)
design doc for details.
