## Background

Preferences are a generic system for storing configuration parameters in Chrome,
see [Preferences] and [chrome/browser/prefs/README.md].

Individual preferences may be declared as syncable, meaning they will be
uploaded to the user's Google Account and propagate across signed-in/syncing
devices from the same user.

This folder contains the code for synchronizing preferences. See
[components/sync/README.md] for background about Sync itself.

## Adding syncable preferences

### For authors

Making a pref syncable requires a few things:
* Specify appropriate [PrefRegistrationFlags] to the `Register*Pref` call.
  * Consider whether your pref should be synced as a browser pref (common case)
    or as an OS pref (for use on ChromeOS-Ash). Note that it must be one or the
    other.
  * Consider whether it needs to be a "priority" pref. The answer is most likely
    "no"; typically only prefs that need to be consumed on the server should be
    marked "priority". Be aware that choosing "priority" has privacy
    implications, so you'll get extra scrutiny.
* Add an entry to the appropriate [SyncablePrefsDatabase]:
  `ChromeSyncablePrefsDatabase` if the pref is in `chrome/`,
  `IOSChromeSyncablePrefsDatabase` if it's in `ios/chrome/`, or
  `CommonSyncablePrefsDatabase` if it's cross-platform.
  * Specify the matching pref type (browser or OS, priority or not).
  * Consider whether your pref is particularly privacy-sensitive, and if so,
    point this out to the reviewer. The most common case of this is when a pref
    records URLs or other history-like data.
* Add an entry to the `SyncablePref` enum in
  tools/metrics/histograms/metadata/sync/enums.xml.

### For reviewers

**Important**: Adding syncable prefs may have privacy impact. It's the
  **responsibility of the code reviewer** to ensure that new syncable prefs
  don't have undue privacy impact. In particular:
* If the pref contains URLs (example: site permissions), it **must** be marked
  as `is_history_opt_in_required = true`, and it will only be synced if the
  user has opted in to history sync (in addition to preferences sync).
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
