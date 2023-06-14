## Background

Preferences are a generic system for storing configuration parameters in Chrome,
see [Preferences] and [chrome/browser/prefs/README.md].

Individual preferences may be declared as syncable, meaning they will be
uploaded to the user's Google Account and propagate across signed-in/syncing
devices from the same user.

This folder contains the code for synchronizing preferences. See
[components/sync/README.md] for background about Sync itself.

## Adding syncable preferences

Making a pref syncable requires a few things:
* Specify appropriate [PrefRegistrationFlags] to the `Register*Pref` call.
* Add an entry to the appropriate [SyncablePrefsDatabase]:
  `ChromeSyncablePrefsDatabase` if the pref is in `chrome/`,
  `IOSChromeSyncablePrefsDatabase` if it's in `ios/chrome/`, or
  `CommonSyncablePrefsDatabase` if it's cross-platform.

**Important**: Adding syncable prefs may have privacy impact. It's the
  **responsibility of the code reviewer** to ensure that new syncable prefs
  don't have undue privacy impact. In particular:
* If the pref contains URLs (example: site permissions), it **must** be marked
  as `is_history_opt_in_required = true`, and it will only be synced if the
  user has opted in to history sync.
* If the pref is marked as "priority" (`syncer::PRIORITY_PREFERENCES` or
  `syncer::OS_PRIORITY_PREFERENCES`), then it will not be encrypted. Carefully
  consider if it actually needs to be "priority". (The most common reason for
  this is when the pref needs to be consumed on the server side.)
* In any other cases that are unclear or questionable, reach out to
  chrome-privacy-core@google.com, or to rainhard@ directly.

[Preferences]: https://www.chromium.org/developers/design-documents/preferences/
[chrome/browser/prefs/README.md]: ../../chrome/browser/prefs/README.md
[components/sync/README.md]: ../../components/sync/README.md
[PrefRegistrationFlags]: https://source.chromium.org/chromium/chromium/src/+/main:components/pref_registry/pref_registry_syncable.h?q=PrefRegistrationFlags
[SyncablePrefsDatabase]: https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:components/sync_preferences/syncable_prefs_database.h
