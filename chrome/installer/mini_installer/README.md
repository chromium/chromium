# mini_installer.exe

This folder contains files to build `mini_installer.exe`, the Windows installer
for Chromium.

By default, running `mini_installer.exe` will do a per-user install of Chromium
into `%LOCALAPPDATA%\Chromium\Application`. You can use `--system-level` for a
per-machine install into `%ProgramFiles%`. Per-user installs are blocked if a
per-machine install is already present.

If installation is failing for some reason, you can see why in
`%TMP%\chromium_installer.log`(for a per-machine install,
`%systemroot%\SystemTemp\chromium_installer.log`). Include `--verbose-logging`
on the command line to make the log very verbose (and possibly informative).

## Google Chrome

If you're building Google Chrome, all of the above apply, except that the
default installation location is `%LOCALAPPDATA%\Google\Chrome\Application`, and
the default log file location is `%TMP%\chrome_installer.log`(for a per-machine
install, `%systemroot%\SystemTemp\chrome_installer.log`).

In addition, you can use one of the following options to install Google Chrome
for different channels:
- `--chrome-sxs` -> `%LOCALAPPDATA%\Google\Chrome SxS\Application`
- `--chrome-dev` -> `%LOCALAPPDATA%\Google\Chrome Dev\Application`
- `--chrome-beta` -> `%LOCALAPPDATA%\Google\Chrome Beta\Application`

Similarly, if there is already a per-machine canary/dev/beta/stable version
installed in `%ProgramFiles%`, the corresponding per-user installation will be
blocked. For example, if you already have Chrome Stable installed in
`%ProgramFiles%`, running `mini_installer.exe` directly will be a no-op. In this
case, assuming you don't have Chrome Beta installed, you can use `--chrome-beta`
to install it into `%LOCALAPPDATA%\Google\Chrome Beta\Application`.
