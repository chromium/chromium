# Chrome's Installer for Windows

[TOC]

## Overview

Generally speaking, we talk about `setup.exe` as "the installer", although
`mini_installer.exe` is the thing that is delivered to devices to install
Chrome. The installer's primary concern is putting the browser on-disk in the
proper location and registering it with Windows as a browser. It is also used to
uninstall and remove Chrome and to perform various one-off tasks.

### Operational environment

Chrome can be installed for all users on a machine or for a single user. The
former is called a "system-level" install and the latter is a "user-level"
install. setup.exe defaults to performaing a user-level install (note: the
Chrome download page defaults to system-level). System-level can be chosen via
any of:

* `--system-level` on the command line,
* `{"distribution":{"system_level":true}}` in an "initial_preferences" file
  passed via the `--installerdata=<PATH>` command line switch, or
* `GoogleUpdateIsMachine=1` in the process environment block.

A user-level install is always expected to be run within the context of an
interactive user. A system-level install may run in the context of an
interactive user (e.g., one who has downloaded Chrome's installer and passed a
UAC prompt) or in the context of a machine account such as SYSTEM (e.g.,
installation via msiexec.exe or an update driven by a system-level install of
Google Update).

### Use of the Windows Registry

Chrome and its installer use portions of the Windows registry established by
Google Update to maintain installation-related state (not user state). This
state is saved in `...\Software\Google\Update\Clients` and
`...\Software\Google\Update\ClientState`. These are unconditionally located in
the 32-bit registry hive, so `KEY_WOW64_32KEY` must always be used when
accessing them.

By and large, new state should we written to Chrome's ClientState key. The
Clients key should be used for the version number and app commands (both as
required by Google Update) and Chrome's channel.

### setup.exe as a Helper

`setup.exe` is run to perform a variety of one-off tasks. In all cases, these
invocations must carry certain command line switches from the original
installation. These include `--system-level`, `--channel=FOO`, and one of the
install mode switches, if used (e.g., `--chrome-sxs`). This ensures that the
proper install is used and that metrics and crashes have the proper annotations.

## Executables

* `setup.exe` (//chrome/installer/setup): The workhorse installer.
* `mini_installer.exe` (//chrome/installer/mini_installer): Carries setup.exe
  and the Chrome 7zip archive as resources; knows how to extract them and run
  `setup.exe`.
* Chrome "standalone" (`.exe`) installer: A self-contained executable that
  installs Google Update and then runs Chrome's `mini_installer`. The tooling to
  build this is not in the Chromium repository.
* Chrome "enterprise" (`.msi`) installer: A self-contained Windows Installer
  file that runs a standalone installer (yielding Google Update and Chrome). The
  tooling to build this is not in the Chromium repository.

## Code Layers

From the bottom heading up:

* [//chrome/install_static](../../install_static): Core functionality with
  minimal dependencies used by `chrome.exe`, `chrome_elf.dll`, `chrome.dll`,
  `setup.exe`, and any other binary that must be aware of install level,
  channel, stats collection, etc.
* [//chrome/installer/util](../util): Higher-level functionality used by
  `chrome.exe`, `chrome.dll`, and `setup.exe`. This is for code shared by the
  browser and the installer.
* [//chrome/installer/setup](./): Handles installation, uninstallation, and
  various helper operations.
* [//chrome/installer/mini_installer](../mini_installer): Outer installer that
  extracts resources and runs the contained `setup.exe`. This also has minimal
  dependencies.

## Testing

* Unit tests in `//chrome/install_static`, `//chrome/installer/util`, and
  `//chrome/installer/setup`.
* End-to-end integration tests that run/verify the installer and ensure that the
  browser runs in [//chrome/test/mini_installer](../../test/mini_installer).

## Development Tips

### Where should my code go?

Generally, prefer `//chrome/installer/setup` if the new code is only for use by
the installer. If it must be shared by Chrome, put it in
`//chrome/installer/util`.