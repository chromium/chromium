# IWA Key Distribution Preload & Browser Integration (`chrome/browser/web_applications/isolated_web_apps/key_distribution`)

This directory contains browser-side configuration and preloaded protobuf data
for the Isolated Web App Key Distribution component.

## Structure

- [`preload/`](preload/README.md): Preloaded `key_distribution.textproto` and
  `manifest.json` (compiled at build time into `iwa-key-distribution.pb`)
  bundled with Chrome so key rotations, special permissions, and access control
  rules are available prior to the first Component Updater fetch.
- `iwa_key_distribution_component_installer_browsertest.cc`: End-to-end browser
  tests verifying
  `component_updater::IwaKeyDistributionComponentInstallerPolicy`
  (`//chrome/browser/component_updater/iwa_key_distribution_component_installer.h`).

See
[`//components/webapps/isolated_web_apps/key_distribution`](/components/webapps/isolated_web_apps/key_distribution/README.md)
for the core `IwaKeyDistributionInfoProvider` runtime implementation.
