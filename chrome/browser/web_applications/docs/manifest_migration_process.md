# Manifest Migration Process

The manifest migration process allows developers to transition users from an
older app (the "source") to a newer app (the "target") by specifying the
`migrate_from` field in the target app's manifest. This is used when an app's
identity or manifest properties change significantly enough.

## High-level Overview

A manifest migration happens in several phases:

1. Detection & Target Installation: The browser detects a migration based on the
   manifest of an app, and whether it has the `migrate_from` and `migrate_to`
   fields.

   - Internally, metadata is saved on both the source and target apps about the
     migration.
   - If the target app is not already installed, it is installed with a
     SUGGESTED_FROM_MIGRATION install state, which is used to filter the app
     from being shown to the user or installed on the user's OS.

2. Metadata Resolution: The system establishes a link between the source app and
   the target app by updating the source app's metadata, following the
   [2-way handshake specified in the security section below](#security).

3. UX Affordance: A notification is surfaced to the user in the source app
   window, indicating that a migration update is available.

4. Review Dialog: The user reviews the migration via a comparison dialog showing
   the "Before" (source) and "After" (target) states.

5. Finalization: If accepted, the target app is installed with the same OS
   integration states as the source app, and the source app is uninstalled.

## Things to note

### Migration Behaviors

- `kSuggest`: The user is notified and must approve the migration.
- `kForce`: Unlike suggested migrations, the user cannot "Ignore" the migration
  dialog, and the dialog is triggered immediately when the source app is
  launched.

### Security

To ensure a secure transition, the migration must be validated by both apps:

- The target app must list the source app in its manifest's `migrate_from`
  field.
- The source app must signal that the target origin is allowed to migrate from
  it. This is achieved via a `web-app-origin-association` file.

## Steps to trigger a manifest migration

1. **Manifest Parsing on page load**:

   - The migrate_from field is parsed by
     [blink::ManifestParser](https://source.chromium.org/search?q=ParseMigrateFrom%20f:manifest_parser.cc).
   - It contains a list of manifest IDs that the app can migrate from and the
     behavior (suggest or force).

2. **Detection & Target Installation**: The system can discover a migration
   requirement through two fields:

   - **`migrate_from`** field: Found in the target app's manifest. The
     [`ManifestUpdateManager`](https://source.chromium.org/search?q=ManifestUpdateManager&sq=&ss=chromium%2Fchromium%2Fsrc)
     triggers the
     [`WebAppInstallFromMigrateFromFieldCommand`](https://source.chromium.org/search?q=WebAppInstallFromMigrateFromFieldCommand&ss=chromium%2Fchromium%2Fsrc),
     which verifies if the source app is installed and uses the
     `MigrationTargetInstallJob` to install the target app.
   - **`migrate_to`** field: Found in the source app's manifest. This triggers
     the
     [`InstallMigrateToAppCommand`](https://source.chromium.org/search?q=InstallMigrateToAppCommand&sq=&ss=chromium%2Fchromium%2Fsrc),
     which allows the target app to be installed (in a hidden state) without the
     user having to visit the target app's URL. In both cases, the target app is
     installed with the `SUGGESTED_FROM_MIGRATION` install state, keeping it
     hidden from the user initially.

3. **Metadata Resolution**: After a target app is installed (even in its hidden
   state), the system must establish the link between the apps in the database.

   - **Timing**: This resolution occurs during browser startup and whenever an
     app is installed.
   - **Operation**: The
     [`ResolveWebAppPendingMigrationInfoCommand`](https://source.chromium.org/search?q=ResolveWebAppPendingMigrationInfoCommand)
     ensures that source apps point to their respective target apps if a pending
     migration exists, or clears the pending migration metadata if the target
     app is uninstalled.
   - **Outcome**: Valid links result in `pending_migration_info` (containing the
     target's manifest ID and behavior) being set on the source app.
   - **Notification**: The `WebAppRegistrar` notifies observers of the change,
     allowing UI components (such as the `WebAppTabHelper`) to surface the
     migration affordance to the user.

## Steps to apply pending migrations

1. **UX Affordance (Entry Point)** Once an app has `pending_migration_info`, the
   system surfaces a notification to the user.

   - **Logic**: The
     [`WebAppBrowserController`](https://source.chromium.org/search?q=f:web_app_browser_controller.h)
     is responsible for parsing the launched app and showing the migration
     dialog if a pending migration exists.
   - **UX**: If valid, the App Menu
     ([`WebAppMenuModel`](https://source.chromium.org/search?q=f:web_app_menu_model.cc))
     displays an "App Update Available" label.
   - **Trigger**: The
     [`WebAppBrowserController`](https://source.chromium.org/search?q=f:web_app_browser_controller.h)
     triggers the migration dialog after the user clicks the label.

2. **The Review Dialog** The user is presented with a comparison between the old
   and new app states.

   - **Data Gathering**: The system calls
     [`ReadAppMigrationDataFromDisk`](https://source.chromium.org/search?q=ReadAppMigrationDataFromDisk)
     to fetch the "before" (source) and "after" (target) metadata.
   - **UI**: The
     [`WebAppUpdateReviewDialog`](https://source.chromium.org/search?q=WebAppUpdateReviewDialog)
     is displayed, allowing the user to review icons, names, and other changes.

3. **Execution and Finalization** If the user chooses to proceed, the system
   executes the final migration command.

   - **Trigger**: The
     [`WebAppBrowserController`](https://source.chromium.org/search?q=f:web_app_browser_controller.h)
     initiates the final migration steps when the user clicks "Accept" in the
     dialog.
   - **Command**: This triggers the
     [`ApplyManifestMigrationCommand`](https://source.chromium.org/search?q=ApplyManifestMigrationCommand).
   - **Outcome**: The command performs the critical finalization steps:
     - **OS Integration**: It synchronizes OS integration (such as shortcuts and
       file handlers) from the source to the target app.
     - **Uninstallation**: It uninstalls the source app.
   - **Migration Complete**: The target app is now fully installed and available
     to the user.

## Syncing migrations

Manifest migrations are synchronized across devices to ensure a seamless
transition. If a user accepts a migration on one device (Device A), the state is
synced to their other devices (Device B), where the target app is automatically
installed and the source app is removed.

- **OS Integration Mirroring**: When the target app is installed via sync on
  Device B, the
  [`InstallFromSyncCommand`](https://source.chromium.org/search?q=InstallFromSyncCommand)
  uses the
  [`GatherMigrationSourceInfoJob`](https://source.chromium.org/search?q=GatherMigrationSourceInfoJob)
  to locate the source app and retrieve its specific OS integration settings.
- **Finalization**: These retrieved settings are applied to the target app
  during its installation on Device B. This ensures that the target app's
  presence on the new device matches the user's configuration of the source app
  on the original device.
- **Silent Background Update**: For migrated apps that are locally installed,
  the system triggers a silent background update (via
  [`FetchManifestAndUpdate`](https://source.chromium.org/search?q=FetchManifestAndUpdate&ss=chromium%2Fchromium%2Fsrc))
  to ensure the target app's metadata is fully synchronized and up-to-date with
  its latest manifest.

# Testing

- **Integration Tests**:

  - [`WebAppInstallFromMigrateFromFieldCommandBrowserTest`](https://source.chromium.org/search?q=WebAppInstallFromMigrateFromFieldCommandBrowserTest):
    Tests the initial detection and target installation.
  - [`WebAppInstallMigrationBrowserTest`](https://source.chromium.org/search?q=WebAppInstallMigrationBrowserTest):
    End-to-end migration scenarios.
  - [`ApplyManifestMigrationCommandBrowserTest`](https://source.chromium.org/search?q=ApplyManifestMigrationCommandBrowserTest):
    Tests the full migration flow including OS integration and uninstallation.
  - [`InstallMigrateToAppCommandBrowserTest`](https://source.chromium.org/search?q=InstallMigrateToAppCommandBrowserTest):
    Tests the installation of the target app when the `migrate_to` field is
    present.
  - [`InstallFromSyncCommandBrowserTest`](https://source.chromium.org/search?q=InstallFromSyncCommandBrowserTest):
    Tests migration propagation across synced devices.

- **Unit Tests**:

  - [`ResolveWebAppPendingMigrationInfoCommandTest`](https://source.chromium.org/search?q=ResolveWebAppPendingMigrationInfoCommandTest):
    Tests metadata establishment.
  - [`WebAppInstallFromMigrateFromFieldCommandTest`](https://source.chromium.org/search?q=WebAppInstallFromMigrateFromFieldCommandTest):
    Tests detection and initial installation.
  - [`InstallMigrateToAppCommandTest`](https://source.chromium.org/search?q=InstallMigrateToAppCommandTest):
    Tests the command logic for `migrate_to` installations.
  - [`InstallFromSyncCommandTest`](https://source.chromium.org/search?q=InstallFromSyncCommandTest):
    Tests the retrieval of source app metadata during sync.

- **UI Tests**:

  - [`WebAppUpdateReviewDialogBrowserTests`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/web_apps/web_app_update_review_dialog_browsertest.cc):
    Tests the migration UX and dialog interactions.
