`activity_reporter` is responsible for reporting when the browser (or other
embedder) is actively used to the browser's vendor. This enables the vendor to
count the number of active users of the browser.

Historically, this data was reported only through the updater: the browser would
signal the updater that it had been used, and when the updater next checked for
updates, it would tell the update server whether or not the browser had been
actively used. This report happens regardless of usage-stat opt-in, in a
de-identified and privacy-preserving way.

`activity_reporter` re-implements that same function, but separate from the
updater, to better isolate functionality. It may be disabled by the
`report_activity=false` GN arg.

The [update protocol documentation](//docs/updater/protocol_4.md) describes the
transmitted information and deduplication algorithm in more detail.

## Incognito Mode and Privacy

### Interaction with Incognito Browsing
`activity_reporter` operates as a process-wide, browser-level service
(`g_browser_process->activity_reporter()`) rather than a per-profile service.
User engagement is detected globally across all browser windows by session
duration trackers (`DesktopSessionDurationTracker` on desktop,
`UmaSessionStats` on Android) observing raw user inputs (keyboard, mouse,
touch).

As a result:
* Active user input inside an Incognito window triggers `ReportActive()` just
  as it would in a normal browsing window.
* The reporter is unaware of whether the user is browsing in normal, Guest, or
  Incognito mode. The transmitted payload only reports that the browser binary
  itself was used. It contains no profile attributes, URLs, tabs, or indicators
  of Incognito usage.
* Network requests use the system network context
  (`SystemNetworkContextManager`) with cookies explicitly disabled.

### Persistence Implementation
`activity_reporter` does not touch profile directories or profile preferences
(`Profile::GetPrefs()`). Instead:
* All persisted state—specifically the `DateLastActive` and
  `DateLastRollCall` timestamps managed by `update_client::PersistedData`—is
  stored exclusively in `Local State` (`g_browser_process->local_state()`),
  located at the root of the User Data Directory.
* Because state is stored at the browser level and no profile-specific data is
  created or retained, browsing in Incognito mode leaves no browsing traces in
  profile storage while still allowing the browser installation to track its
  last active date.

### Impact of Incognito and Normal Browsing on Reports
* **Single Installation Metric**: Both normal and Incognito browsing report
  under the same single application identifier (`kChromeActivityId`).
* **Deduplication**: In-memory throttling drops subsequent reports triggered
  within a 5-hour window, and the update server aggregates reports by calendar
  date.
* **Count Aggregation**: Browsing in normal mode, Incognito mode, or a
  combination of both on the same day counts as **exactly one active user (1
  DAU)** for that installation. Using Incognito mode neither creates a separate
  active user nor leaks that Incognito was used.
