# Signing Scripts for Chrome on macOS

This directory contains Python modules that modify the Chrome application bundle
for various release channels, sign the resulting bundle, package it into
`.dmg`/`.pkg` files for distribution, and sign those resulting `.dmg`/`.pkg`
files.

## Invoking

Signing requires a statically linked build (i.e. `is_component_build = false`),
which you can set up in a new GN out directory with the following args:

    is_debug = false
    is_component_build = false

The scripts are invoked using the driver located at
`//chrome/installer/mac/sign_chrome.py`. In order to sign a binary, a signing
identity is required. Googlers can use the [internal development
identity](https://goto.google.com/ioscerts); otherwise you must supply your
own. Note that a
[self-signed](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html)
identity is incompatible with the _library validation_ signing option that
Chrome uses.

A sample invocation to use the
[internal Google development identity](https://goto.google.com/appledev/book/getting_started/provisioning/index.md#googles-development-certificate) during development would be:

    $ ninja -C out/release chrome chrome/installer/mac
    $ ./out/release/Chromium\ Packaging/sign_chrome.py --input out/release --output out/release/signed --identity 'Google Development' --development --disable-packaging

The `--disable-packaging` flag skips the creation of DMG and PKG files, which
speeds up the signing process when one is only interested in a signed .app
bundle. The `--development` flag skips over code signing requirements and checks
that do not work without the official Google signing identity, and it injects
the `com.apple.security.get-task-allow` that lets the app be debugged.

## The Installer Identity

The above section speaks of the `--identity` parameter to `sign_chrome.py`, and
how the normal development identity will do, and how a self-signed identity will
not work. However, the identity used for Installer (.pkg) files is different.

Installer files require a special Installer Package Signing Certificate, which
is different than a normal certificate in that it has a special Extended Key
Usage extension.

For the normal identity, Apple provides both a development and a deployment
certificate, and while the deployment certificate can be (and should be)
carefully guarded, the development certificate can be more widely used by the
development team. However, Apple provides _only_ a deployment installer
certificate. For development purposes, you must self-sign your own.

Directions on how to create a self-signed certificate with the special Extended
Key Usage extension for installer use can be found on
[security.stackexchange](https://security.stackexchange.com/a/47908).

You will need to explicitly mark the certificate as trusted. This can be done
with
`sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain my_installer_cert.crt`.
Be sure that `sudo security -v find-identity` lists this new certificate as a
valid identity.

## Chromium

There are slight differences between the official Google Chrome signed build and
a development-signed Chromium build. Specifically, the entitlements will vary
because the default
[chrome/app/app-entitlements.plist](../../../app/app-entitlements.plist) omits
[specific entitlements](../../../app/app-entitlements-chrome.plist) that are
tied to the official Google signing identity.

In addition, the Chromium [code sign
config](https://cs.chromium.org/chromium/src/chrome/installer/mac/signing/chromium_config.py)
only produces one Distribution to sign just the .app. An
`is_chrome_branded=true` build produces several Distributions for the official
release system.

## Google Chrome

If you attempt to sign an `is_chrome_branded=true` build locally, the app will
fail to launch because certain entitlements are tied to the official Google code
signing identity/certificate. To test an `is_chrome_branded=true` build locally,
build with `include_branded_entitlements=false` or replace the contents of
[`app-entitlements-chrome.plist`](../../../app/app-entitlements-chrome.plist)
with an empty plist.

### TCC Permissions

MacOS grants applications access to privileged resources using the TCC
(Transparency, Consent, and Control) subsystem. TCC records user authorization
decisions, in part, based on the code signing identity of the responsible
application.

One important point, as discussed in the [debugging
tips](../../../../docs/mac/debugging.md#system-permission-prompts_transparency_consent_and-control-tcc)
is if Chrome/Chromium is launched as a subprocess of another GUI application
(such as Terminal), the parent GUI process – not the browser – is considered the
responsible application for TCC's purposes.

An authorization decision can be reset manually using the `tccutil(1)` command.
For example, this would reset the microphone access permission:

    tccutil reset Microphone org.chromium.Chromium

Unfortunately there is not an authoritative list of service names for resetting,
but the value `All` will remove all decisions. The decisions are recorded in a
SQLite database, which can be inspected using the command below. This requires
granting the **Full Disk Access** permission in System Settings to the Terminal
or disabling System Integrity Protection.

    sqlite3 ~"/Library/Application Support/com.apple.TCC/TCC.db"

The `access.service` column's values corresponds to the `tccutil reset` service,
sans the `kTCCService` prefix.

### System Detached Signatures

MacOS may itself sign Chromium build binaries when it needs to record a
signature for certain OS operations. The signature is not attached to the
application bundle, as the signing scripts do, but it is instead recorded in a
_detached signature database_. This happens, e.g. when a network request
is filtered by the Application Firewall.

If you get errors saying the build is already signed, before signing the build
yourself, this is likely the issue. To fix it:

1. Disable the Application Firewall in **System Preferences > Security &
    Privacy > Firewall > Turn Off Firewall**.
2. `sudo rm /var/db/DetachedSignatures`
3. Reboot

## Running Tests

The `signing` module is thoroughly unit-tested. When making changes to the
signing scripts, please be sure to add new tests too. To run the tests, simply
run the wrapper script at
`//chrome/installer/mac/signing/run_mac_signing_tests.py`.

You can pass `--coverage` or `-c` to show coverage information. To generate a
HTML coverage report and Python coverage package is available (via `pip install
coverage`), run:

    coverage3 run -m unittest discover -p '*_test.py'
    coverage3 html

## Formatting

The code is automatically formatted with YAPF. Run:

    git cl format --python
