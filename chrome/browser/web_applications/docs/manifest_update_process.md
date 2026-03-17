# Manifest Update Process

High level information: https://web.dev/manifest-updates/

The manifest update process is required whenever a new manifest is served to an existing web_app, which would mean that the database entries corresponding to that web application needs to be updated. With the [predictable app updating flag enabled](https://source.chromium.org/chromium/chromium/src/+/main:content/public/common/content_features.cc?q=%22kWebAppPredictableAppUpdating%22%20f:.*_features.cc&ss=chromium%2Fchromium%2Fsrc), the following information sheds some light on how the
manifest update system works in Chrome.

## Things to note:
- Updates to security sensitive fields [1] (like `name`, `icon` and `short_name`) require explicit user approval.
- Updates to the app's icon follow the `Cache-Control:immutable` behavior of HTML, where updates are triggered **ONLY** if the icon url has changed.
  - If the icon url **HAS** changed, but the a pixel by pixel comparison of the old and new icon shows a difference of <10%, the icon is updated silently.
- Updates to non-security sensitive fields are silent.

This helps ensure:
- Network resources are saved and that icons are downloaded ONLY if the corresponding metadata for them have changed.
- Developers have a way of choosing when to trigger an update to their app's identities.
- The UX affordances are designed in a way to provide more control to the user on
what to do, should there be user intervention required for an update.

## Steps to trigger a manifest update:
Whenever the tab has kicked off a [navigation to an url for a web_app](https://source.chromium.org/search?q=WebAppTabHelper::PrimaryPageChanged), a check is kicked off to determine if a manifest update is required or not. The update process is aborted if the following conditions are satisfied:
  - If the app is not installed.
  - If the installed app is a [System Web App](https://source.chromium.org/search?q=SystemWebAppManager), [placeholder app](https://source.chromium.org/search?q=Placeholder%20f:webapps%2Fconcepts.md) or [Isolated Web App](https://source.chromium.org/search?q=%22WebAppFilter::IsIsolatedApp%22&sq=&ss=chromium%2Fchromium%2Fsrc).

Once the update is allowed to proceed, the following steps happen:
  - Wait for the [page to finish loading and the manifest url to be loaded](https://source.chromium.org/search?q=ManifestUpdateManager%20PreUpdateWebContentsObserver&sq=).
  - On successful page load, the [`ManifestSilentUpdateCommand`](https://source.chromium.org/search?q=ManifestSilentUpdateCommand&sq=) takes over to perform the following tasks:
    - Fetching the manifest and all metadata defined in it for the url.
    - Identifying if the update can be done silently, or it requires user intervention, or both.
    - Parts of the update that happen silently is completed.
    - If user intervention is required, pending update metadata is stored in the web app.
      - As an implementation detail, for icon changes, the "new icons" are also stored on the disk inside the profile directory to conserve network resources and prevent [redownloading](https://source.chromium.org/search?q=WebAppIconManager::WritePendingIconData).

## Steps to apply pending updates:

If the update requires user intervention, that is surfaced to the user in a non-blocking way
by showing an expanded label saying [`App Update Available`](https://source.chromium.org/search?q=WebAppMenuButton::CanShowPendingUpdate&sq=) in a standalone app window. Apps running in a browser tab do not see that label.

Clicking that label triggers the three dot menu dropdown, that has a `Review App Update` entrypoint. Clicking on that triggers the [`WebAppUpdateReviewDialog`](https://source.chromium.org/search?q=WebAppUpdateReviewDialog&sq=&ss=chromium%2Fchromium%2Fsrc), which shows the before and after state of the app. The metadata to show on the dialog is collected by the [`AppUpdateDataReadCommand`](https://source.chromium.org/search?q=AppUpdateDataReadCommand&ss=chromium%2Fchromium%2Fsrc).

The user can choose to either:
- Ignore the update: The expanded label on the app window's three dot menu is no longer shown, unless a new update appears.
- Accept the update: The [`ApplyPendingManifestUpdateCommand`](https://source.chromium.org/search?q=ApplyPendingManifestUpdateCommand&sq=&ss=chromium%2Fchromium%2Fsrc) is triggered which applies any pending updates to the metadata and icons.
- Uninstall the app: Remove the app if they're not sure if the app update was malicious or not.

# Testing

- [`WebAppIntegrationTestDriver`](https://source.chromium.org/search?q=WebAppIntegrationTestDriver&ss=chromium%2Fchromium%2Fsrc) contains browser tests for the entire end-to-end working of the manifest update process. Please look at the [critical user journeys](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/webapps/data/critical_user_journeys.md?q=%22App%20identity%20updating%22%20f:critical_user_journeys.md&ss=chromium%2Fchromium%2Fsrc) and [integration testing framework](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/webapps/data/critical_user_journeys.md?q=%22App%20identity%20updating%22%20f:critical_user_journeys.md&ss=chromium%2Fchromium%2Fsrc) for more documentation on how to parse these
tests.
- [`ManifestSilentUpdateCommandTest`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/commands/manifest_silent_update_command_unittest.cc) contains unit tests for the determination
part of the manifest update operation that determines if the updates need to happen
silently, or if pending data needs to be stored.
- [`ManifestSilentUpdateCommandBrowserTest`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/commands/manifest_silent_update_command_browsertest.cc) contains browser tests for the determination part of the manifest update operation, that requires an active browser window to be available, for example, to check if the expanded label is available for pending updates.
- [`ApplyPendingManifestUpdateCommandTest`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/commands/apply_pending_manifest_update_command_unittest.cc) contains unit tests for the application of pending updates to the web app database as well as icons stored on the disk.
- [`WebAppUpdateReviewDialogBrowserTests`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/web_apps/web_app_update_review_dialog_browsertest.cc) contains browser tests for the end to end
flow, including the triggering of the dialog and verification of the expanded label
available on the web app.

