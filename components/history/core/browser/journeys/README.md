# History Journeys Component

[TOC]

The `journeys` directory implements client-side storage, sync ingestion, and
querying for **server-generated** journeys: groups of history visits, each with
a title and summaries, that are produced on the server and delivered to clients
over Chrome Sync as the read-only `syncer::JOURNEY` data type.

**Current status: behind a disabled-by-default flag.** The storage and sync
pipeline described here is gated by `syncer::kSyncJourney`.

This document covers the backend only: storage, sync, and the query APIs on
`HistoryService` (`GetJourney` and `GetAllJourneys`). UI surfaces that consume
these APIs are documented separately.

## Summary

| Design decision | Implementation | Why |
|---|---|---|
| **Journeys store visit timestamps, not URLs** | `JourneyRow` holds a `base::Time` per visit. | Keeps `visits`/`urls` as the single source of truth, avoids duplicating page data, and keeps sync payloads small. |
| **Sync is read-only on the client** | The bridge does not support commits. | Journeys are produced on the server; there is no client-side authoring path. |
| **A custom passphrase stops the data type** | The controller reports that sync must stop and clear data. | The data type is not offered when everything is encrypted, so local journeys are cleared rather than left behind. |
| **A journey resolves to the end of a redirect chain** | Resolution picks the last visit recorded for a given timestamp. | Hops of one navigation share that navigation's commit timestamp; taking the last one yields the page the user actually landed on. |
| **Unresolvable journeys are dropped whole** | A journey is skipped if any of its visits cannot be resolved. | Callers never receive a journey whose visit list is incomplete relative to what the server described. |

## Architecture Overview

The Journeys subsystem spans two execution contexts: the **UI sequence** (client
services and sync controllers) and the **background history sequence** (SQLite
operations, sync bridge mutations, and visit resolutions). Journeys does not run
code on Chrome Sync's internal sync sequence; instead, the sync framework calls
into the subsystem from the outside: invoking the data type controller on the UI
sequence and delivering remote updates to the bridge on the backend sequence.

```text
╔════════════════════════════════════════════════════════════════════════════╗
║ UI SEQUENCE (browser / profile)                                            ║
║                                                                            ║
║   UI consumers                              SyncService                    ║
║        │                                         │ owns                    ║
║    (1) │ GetAllJourneys() / GetJourney()         ▼                         ║
║        │                                JourneyDataTypeController          ║
║        ▼                                         │                         ║
║   HistoryService ◄───────────────────────────────┘                         ║
║        │      (A) GetJourneysSyncControllerDelegate() returns a            ║
║        │          syncer::ProxyDataTypeControllerDelegate, which re-posts  ║
║        │          every controller call to the backend sequence            ║
╚════════╪═════════════════════════════════════════╪═════════════════════════╝
     (2) │ tracker->PostTaskAndReplyWithResult( (B) │ OnSyncStarting,
         │   backend_task_runner_, ...)             │ ApplyUpdates, ...
         ▼                                          ▼
╔════════════════════════════════════════════════════════════════════════════╗
║ BACKEND SEQUENCE (backend_task_runner_: a base::ThreadPool sequence)       ║
║                                                                            ║
║                                      (C) remote JourneySpecifics updates   ║
║                                          from the sync processor           ║
║                                               │                            ║
║    (3)  GetAllJourneysWithVisits()            ▼                            ║
║         │                              JourneysSyncBridge                  ║
║         │                                  │         ▲                     ║
║         │      AddOrUpdateJourneyRows()    │         │ OnHistoryDeletions  ║
║         │      DeleteJourneys()            │         │ (acts only when     ║
║         ▼                                  ▼         │  all_history)       ║
║    HistoryBackend ◄─────────────────────────┘        │                     ║
║      │    │                                          │                     ║
║      │    └──────────────────────────────────────────┘                     ║
║      │   owns the bridge; implements HistoryBackendForJourneysSync         ║
║      │                                                                     ║
║  (4) │ journeys::GetAllJourneysWithResolvedVisits(*db_)                    ║
║      │   resolves every visit_time to a (URL, title) pair                  ║
║      ▼                                                                     ║
║   HistoryDatabase                                                          ║
║     │                                                                      ║
║     ├── is-a ──► JourneysDatabase                                          ║
║     │              journeys                                                ║
║     │              journey_history_entries                                 ║
║     │              journey_continuation_queries                            ║
║     │              journey_collections, journey_collection_items           ║
║     │                                                                      ║
║     ├── owns ──► JourneysSyncMetadataDatabase                              ║
║     │              journey_sync_metadata                                   ║
║     │              meta_table["journey_data_type_state"]                   ║
║     │                                                                      ║
║     └── is-a ──► URLDatabase / VisitDatabase                               ║
║                    urls, visits (the source for visit resolution)          ║
╚════════════════════════════════════════════════════════════════════════════╝
     (5) │ reply hops back to the UI sequence: std::vector<Journey>
         ▼
     UI consumers
```

`(1)`-`(5)` trace the read path (a UI query down to SQLite and back).
`(A)`-`(C)` trace the sync path (controller wiring and remote updates).

### Threading & Concurrency Boundaries

- **UI sequence**: [`HistoryService`][history-service] and
  [`JourneyDataTypeController`][controller] run on the browser UI sequence.
  Asynchronous client queries take a `base::CancelableTaskTracker*` so that a
  caller which goes away before its query finishes does not receive a reply.
- **Background history sequence (`backend_task_runner_`)**:
  [`HistoryBackend`][history-backend], [`JourneysSyncBridge`][bridge],
  [`JourneysDatabase`][journeys-db], and
  [`JourneysSyncMetadataDatabase`][metadata-db] run on `backend_task_runner_`.
  All SQLite work and visit resolution happens here. `backend_task_runner_` is a
  sequence from `base::ThreadPool`, not a dedicated thread: tasks run in order
  and never concurrently, but not necessarily on the same physical thread. Only
  the bridge enforces this at runtime with a sequence checker; the backend and
  the database mixins rely on convention, so always reach the backend through
  `HistoryService` rather than calling it directly.
- **Crossing between them**:
  - Read queries go through `base::CancelableTaskTracker`, which posts to the
    backend sequence and replies on the UI sequence.
  - Sync commands go through
    [`syncer::ProxyDataTypeControllerDelegate`][proxy-delegate], which re-posts
    them to the backend sequence, so callers on the UI sequence never handle a
    backend pointer themselves.
  - The bridge observes history changes as a
    [`HistoryBackendObserver`][backend-observer], directly on the background
    sequence.
  - Chrome Sync's internal sequence communicates with these two contexts from
    the outside: the sync engine talks to the controller on the UI sequence and
    dispatches remote updates to the bridge on the backend sequence. Journeys
    itself executes no tasks on the sync sequence.

## Domain Models vs. Storage Models

The subsystem keeps persisted records and in-memory presentation models
separate:

| Dimension | Storage model ([`JourneyRow`][journey-row]) | Domain model ([`Journey`][journey]) |
|---|---|---|
| **Header** | `journey_row.h` | `journey.h` |
| **Visit entry** | [`JourneyHistoryEntry`][journey-history-entry], a visit timestamp | [`JourneyVisit`][journey-visit], a URL, title, and visit timestamp |
| **Coupling** | References `visits` by timestamp | Self-contained, already resolved |
| **Sync parity** | Mirrors `sync_pb::JourneySpecifics` | In-memory only; never sent over the wire |
| **Usage** | SQLite persistence and the sync bridge | Callers of `HistoryService::GetJourney` and `GetAllJourneys` |

### Why journeys store timestamps, not URLs

1. **One source of truth**: URLs, titles and visit counts are owned by
   `URLDatabase` and `VisitDatabase`. Duplicating them into journey rows would
   let the two copies drift apart.
2. **Results follow the history tables**: a journey is materialized by looking
   its visits up at query time, so whatever the history tables currently contain
   is what callers see — and if any one of its visits is gone, the whole journey
   drops out of the results rather than coming back shorter. Note this is a
   query-time behavior: rows in `journey_history_entries` are not themselves
   removed when the visit they point at goes away.
3. **Smaller sync payloads**: the wire format carries integer timestamps rather
   than arbitrary-length URLs and titles.

## SQLite Storage Architecture

Journeys storage lives inside the main `History` SQLite database rather than in
a file of its own: [`HistoryDatabase`][history-database] inherits
[`JourneysDatabase`][journeys-db] and owns
[`JourneysSyncMetadataDatabase`][metadata-db]. That is why they are co-located:
a separate file could not be committed atomically with the surrounding history
changes.

A journey is a title, an optional emoji, a long and a short summary, a set of
visits, any number of suggested follow-up searches, and optional collections of
similar pages. That maps onto the tables below — see
[`InitJourneysTables`][schema] for the authoritative definitions:

| Table | Keyed by | Holds |
|---|---|---|
| **`journeys`** | `journey_id` | Journey metadata: title, creation time, emoji, and the two summaries. |
| **`journey_history_entries`** | journey + visit timestamp | Which visits belong to a journey. |
| **`journey_continuation_queries`** | grouped by `journey_id`, each row with its own id | Suggested follow-up searches. |
| **`journey_collections`** | journey + position | Titled collections of similar pages, in display order. |
| **`journey_collection_items`** | journey + collection position + item position | The items of each collection, as visit timestamps, in display order. |
| **`journey_sync_metadata`** | `storage_key` | Per-entity sync metadata. |
| **`meta_table`** | key-value | The per-data-type sync state, in the shared History meta table. |

The physical layout is deliberate. The rows of `journey_history_entries`,
`journey_collections` and `journey_collection_items` are small and keyed by a
composite primary key, so these tables are `WITHOUT ROWID` and the rows live
directly in the primary key index. Their primary keys start with `journey_id`,
so they also serve fetching and deleting a journey's rows. And indices back the
other lookups the schema is built around: listing journeys newest-first,
fetching and deleting a journey's continuation queries, and going from a visit
back to the journeys that reference it.

A *continuation query* is a server-suggested way to pick a journey back up: a
title shown to the user, and a prompt used to run the follow-up search. Unlike
visits these are stored in full, so they need no resolution against the history
tables. They get their own table because a journey may have any number of them.

A *collection* is a titled group of similar pages from a journey, shown as one
section of the journey's details page (e.g. "Boats you've visited"). Each item
is expected to reference one of the journey's visits by timestamp; this isn't
checked when storing. Collections are optional.
Display order is stored explicitly in `position` and `item_position` columns;
reads join items to their collection and don't rely on positions being
contiguous. A visit repeated within a collection is stored once, at its first
position.

Writes go through [`AddOrUpdateJourneys`][add-or-update], which replaces a
journey's child rows wholesale rather than merging into them, so an update that
shrinks a journey leaves no stale visits, queries or collections behind.

## Visit Resolution Pipeline (`journeys_backend_util.h`)

These are free functions in `namespace history::journeys`, not members of a
class. Resolution turns storage records ([`JourneyRow`][journey-row]) into fully
populated domain models ([`Journey`][journey]) by looking up the local history
tables. The diagram shows the all-journeys path, which also records the
resolution histogram (see [Metrics](#metrics));
[`GetJourneyWithResolvedVisits`][get-one-resolved] (behind
`HistoryService::GetJourney`) loads a single row by `journey_id` and resolves it
through [`ResolveJourneyVisits`][resolve-visits], which returns
`std::optional<Journey>` and records nothing:

```text
HistoryBackend::GetAllJourneysWithVisits()
  │
  │  no database  ⇒  return {}                        ✗ bail out
  │
  ▼
journeys::GetAllJourneysWithResolvedVisits(HistoryDatabase& db)
  │
  │  db.GetAllJourneys()  →  vector<JourneyRow>, creation_time DESC
  │
  ├─► for each JourneyRow row:                       ┄ outer loop ┄
  │      │
  │      ▼
  │    ResolveJourneyVisitsWithResult(db, std::move(row))
  │      →  expected<Journey, SyncedJourneyResolutionResult>
  │      │
  │      ├─► for each JourneyHistoryEntry:           ┄ inner loop ┄
  │      │      │
  │      │      │  db.GetLastRowForVisitByVisitTime(entry.visit_time,
  │      │      │                                   &visit_row)
  │      │      │    miss  ⇒  VLOG(1), unexpected(kMissingVisit)  ✗ bail out
  │      │      │    hit   →  terminal visit of the redirect chain
  │      │      │
  │      │      │  db.GetURLRow(visit_row.url_id, &url_row)
  │      │      │    miss  ⇒  VLOG(1), unexpected(kMissingUrl)    ✗ bail out
  │      │      │    hit   →  visits.emplace_back(url_row.url(),
  │      │      │                                 url_row.title(),
  │      │      │                                 entry.visit_time)
  │      │      │
  │      │      ↺  next entry
  │      │
  │      ├─► after the loop: return Journey(journey_id, title,
  │                                         creation_time, emoji,
  │                                         overview, short_overview,
  │                                         visits, continuation_queries)
  │      │
  │      ▼
  │    UMA Resolution.Result  ←  kResolved, or the error
  │    error?  ⇒  drop the whole journey             (all-or-nothing)
  │    value?  →  push_back
  │
  ▼
std::vector<Journey>   (size ≤ number of stored journeys)
```

### How resolution works

1. **Redirect chains**: all hops recorded by a single navigation are written
   with that navigation's commit timestamp, so a timestamp on its own does not
   identify one row. (Client redirects arrive as separate navigations and do get
   their own timestamps.) [`GetLastRowForVisitByVisitTime`][get-last-row] takes
   the last visit recorded for that timestamp, which for such a chain is the
   page the user ended up on — the URL and title worth showing. The lookup keys
   only on the timestamp, so it assumes no unrelated visit shares that exact
   microsecond.
2. **Unresolvable journeys are dropped whole**: if any visit or URL for a
   journey is missing, [`ResolveJourneyVisits`][resolve-visits] gives up on that
   journey and returns nothing, and
   [`GetAllJourneysWithResolvedVisits`][get-all-resolved] leaves it out of the
   result. Callers therefore never see a journey with a partial visit list, and
   the returned vector may be shorter than the number of stored journeys.

## Sync Integration & Lifecycle (`syncer::JOURNEY`)

The wire format is `sync_pb::JourneySpecifics`, defined in
[`journey_specifics.proto`][specifics], with nested `HistoryEntry` and
`ContinuationQuery` messages.

```text
sync engine ── ClientTagBasedDataTypeProcessor
  │
  │  ══ PHASE 1: metadata, written while the processor streams updates ══
  │
  ├─► JourneysSyncBridge::CreateMetadataChangeList()
  │      →  syncer::SyncMetadataStoreChangeList(sync_metadata_db,
  │                                             syncer::JOURNEY)
  │      Writes go straight to SQLite; nothing is buffered in memory.
  │
  ├─► per entity:  ChangeList::UpdateMetadata(storage_key, EntityMetadata)
  │      →  JourneysSyncMetadataDatabase::UpdateEntityMetadata()
  │            →  INSERT OR REPLACE INTO journey_sync_metadata
  │
  ├─► once:        ChangeList::UpdateDataTypeState(DataTypeState)
  │      →  JourneysSyncMetadataDatabase::UpdateDataTypeState()
  │            →  meta_table.SetValue("journey_data_type_state", ...)
  │
  │  ══ PHASE 2: entities, applied as one batch ══
  │
  ├─► MergeFullSyncData(...)           first sync; nothing local to merge
  │      │
  │      ▼  forwards verbatim
  └─► ApplyIncrementalSyncChanges(metadata_change_list, entity_changes)
         │
         │  partition entity_changes by EntityChange::type()
         │
         ├─► ACTION_DELETE  →  journey_ids_to_delete
         │      backend_->DeleteJourneys(ids)
         │        →  ScheduleCommit(), then DELETE FROM
         │              journeys
         │              journey_history_entries
         │              journey_continuation_queries
         │              journey_collections
         │              journey_collection_items
         │
         ├─► ACTION_ADD / ACTION_UPDATE
         │      JourneyRowFromSpecifics()   (deserialize)
         │      backend_->AddOrUpdateJourneyRows(rows)
         │        →  ScheduleCommit(), then batched prepared-statement
         │           upsert (child rows are purged and re-inserted)
         │
         │  any DB failure returns early:
         │    ✗ ModelError(FROM_HERE, Type::kJourneysDatabaseError)
         ▼
       return change_processor()->GetError()   // nullopt when healthy

Both phases run inside the long-running SQLite transaction held by
HistoryBackend, so metadata and entity rows are committed together.
```

### Sync behavior and policies

- **Read-only on the client**: journeys are produced on the server and the
  client only consumes them. There is no path that commits a locally created or
  edited journey, and [`MergeFullSyncData`][merge-full] simply forwards to the
  incremental path because there is never local data to merge.
- **Identity**: `journey_id`, generated by the server, is used as both the sync
  client tag and the SQLite storage key. A journey therefore has one stable
  identity across devices, and the processor can address rows without a
  translation layer.
- **Conflict resolution**: not applicable. Since the client never commits, there
  is no local change that could conflict with an incoming update.
- **Communication direction**: `JOURNEY` is registered in
  [`data_type.cc`][data-type-registration] as `kRegularTwoWay`. That does not
  contradict the read-only behavior — the enum only distinguishes two-way from
  commit-only and has no download-only value. Read-only is a property of the
  bridge, not of the registration.
- **How updates are written**:
  1. *Metadata, as updates stream in*:
     [`CreateMetadataChangeList`][create-metadata-changelist] returns a change
     list that writes straight through to SQLite instead of buffering in memory.
  2. *Entities, as one batch*:
     [`ApplyIncrementalSyncChanges`][apply-incremental] splits the incoming
     changes, applies deletions first, then the additions and updates together.

  Both happen inside the transaction `HistoryBackend` already holds, so metadata
  and entity rows are committed as a unit. The commit itself is batched rather
  than immediate.
- **Managed accounts and policy**: enterprise-managed accounts other than
  `@google.com` do not sync journeys, and the data type also stops if the
  `SavingBrowserHistoryDisabled` policy is set.
- **Custom passphrase**: the data type does not support custom-passphrase
  encryption, so when "encrypt everything" is on the controller reports that it
  must stop and clear its data. Local journey tables are emptied rather than
  left stale.
- **Teardown and errors**:
  - *Sync disabled*: clears the stored sync metadata and progress marker, and
    deletes all journeys.
  - *History cleared*: when all history is cleared, the bridge untracks every
    entity and wipes its metadata. Clearing a subset of history is not acted on
    by the bridge; the affected journeys simply stop resolving, and their rows
    remain until the server sends a deletion.
  - *Database error*: the bridge drops its database pointer and reports a model
    error so the sync processor disconnects cleanly. The outcome of every
    journeys table write the bridge issues, including the one when sync is
    disabled, is recorded in
    `History.SyncedJourneys.DatabaseOperationSuccess.*`.

## Public APIs

### `HistoryService::GetAllJourneys`

```cpp
// components/history/core/browser/history_service.h
using GetAllJourneysCallback =
    base::OnceCallback<void(std::vector<journeys::Journey>)>;

virtual base::CancelableTaskTracker::TaskId GetAllJourneys(
    GetAllJourneysCallback callback,
    base::CancelableTaskTracker* tracker);
```

- Called on the UI sequence; the callback runs back on that same sequence.
- Posts to the backend sequence, where
  [`GetAllJourneysWithVisits`][get-all-with-visits] does the work.
- Returns [`Journey`][journey] structs with every visit already resolved to a
  URL and title (plus its visit timestamp), newest journey first by creation
  time.
- Destroying the `base::CancelableTaskTracker` cancels the reply, so a UI
  surface that closes mid-query will not be called back.

### `HistoryService::GetJourney`

```cpp
// components/history/core/browser/history_service.h
using GetJourneyCallback =
    base::OnceCallback<void(std::optional<journeys::Journey>)>;

virtual base::CancelableTaskTracker::TaskId GetJourney(
    const std::string& journey_id,
    GetJourneyCallback callback,
    base::CancelableTaskTracker* tracker);
```

- Same threading and cancellation behavior as `GetAllJourneys`; the work runs
  in [`GetJourneyWithVisits`][get-one-with-visits] on the backend sequence.
- Replies with `std::nullopt` if no journey has that `journey_id`, or if it
  has an unresolvable visit — exactly the journeys `GetAllJourneys` leaves out.

### `HistoryBackendForJourneysSync`

[`JourneysSyncBridge`][bridge] does not depend on `HistoryBackend` directly. It
talks to the narrow [`HistoryBackendForJourneysSync`][backend-interface]
interface instead, which:

- keeps the bridge away from unrelated history features such as favicons and
  downloads, and
- lets `journeys_sync_bridge_unittest.cc` run against an in-memory fake, with no
  SQLite files or extra sequences involved.

### Teardown & Destruction Ordering

`HistoryBackend` tears the bridge down before the database it depends on, so the
bridge stops observing and goes away while the database it points at is still
alive.

Two related lifetime points are worth knowing before changing this code:

- **The backend outlives its pending tasks.** It is reference counted, and
  in-flight background queries hold their own reference.
  `HistoryService::Cleanup()` hands teardown to the background sequence, and the
  backend stays alive until every task holding a reference has finished.
- **`SyncService` must shut down before `HistoryService`.** The sync controller
  delegate posts to the backend without owning it, which is only safe because
  the sync service is torn down first. That ordering comes from the KeyedService
  dependency declared by `SyncServiceFactory`; changing it would let the
  delegate post against a backend that is already gone.

## Building, Testing & Debugging

### Build targets

Everything here is part of the `//components/history/core/browser` static
library, and the unit tests are part of its `unit_tests` source set, which links
into `components_unittests`. There is no separate GN target for Journeys.

### Running the tests

```sh
autoninja -C out/Default components_unittests
out/Default/components_unittests --gtest_filter='Journeys*:HistoryBackendJourneysSync*:HistoryBackendTest.JourneysSync*:HistoryServiceTest.GetJourney:HistoryServiceTest.GetAllJourneys:*HistoryBackendDBTest.InitJourneysTables*'
```

`Journeys*` covers the four test files below plus the journeys cases in
`history_database_unittest.cc`. The remaining patterns are needed because the
rest of the coverage lives in suites named after their host component rather
than after journeys: the sync-enabled and flag-off backend tests, the
end-to-end tests of the public APIs, and the schema-creation test. Avoid
`*Journey*`, which also pulls in unrelated suites elsewhere in
`components_unittests`.

| Test file | Covers |
|---|---|
| `journeys_database_unittest.cc` | Schema, upserts, cascading deletes |
| `journeys_backend_util_unittest.cc` | Visit resolution and the omission rules |
| `journeys_sync_bridge_unittest.cc` | Bridge behavior against a fake backend |
| `journeys_sync_metadata_database_unittest.cc` | Sync metadata persistence |

Shared test helpers, such as the gtest `PrintTo` printers for journey types,
live in `journeys_test_utils.{h,cc}`.

### Debugging

- **`chrome://sync-internals`** is the primary entry point. The bridge
  implements `GetAllDataForDebugging()`, so the *Sync Node Browser* tab shows
  the raw specifics for every journey the client has ingested without opening
  the SQLite file.
- The data type appears there as `Journey`, and its histogram suffix is
  `JOURNEY`.
- Visit resolution failures are logged at `VLOG(1)` in
  `journeys_backend_util.cc`. Run with `--v=1` to see which journeys are being
  dropped and why.
- To inspect storage directly, the `journeys`, `journey_history_entries`,
  `journey_continuation_queries`, `journey_collections`,
  `journey_collection_items`, and `journey_sync_metadata` tables all live in
  the profile's main `History` SQLite file.

### Metrics

The component's own histograms are prefixed `History.SyncedJourneys.` and
defined in `tools/metrics/histograms/metadata/history/histograms.xml`. The
prefix keeps them apart from the `History.Clusters.*` histograms of the old
Journeys UI. See the histogram descriptions there for details:

- `Resolution.Result`: outcome of resolving each stored journey in
  [`GetAllJourneysWithResolvedVisits`][get-all-resolved].
- `DatabaseOperationSuccess.*`: success of each journeys table write issued by
  [`JourneysSyncBridge`][bridge].

Everything else comes from the sync infrastructure under the `JOURNEY`
histogram suffix and is not duplicated here:

- `Sync.DataTypeEntityChange.JOURNEY`: incoming updates and deletions.
- `Sync.DataTypeUpdateDrop.DroppedByBridge` (`JOURNEY` bucket): specifics
  rejected by `IsEntityDataValid()`.
- `Sync.ModelError.JOURNEY`: bridge errors such as `kJourneysDatabaseError`.
- `Sync.DataTypeCount.JOURNEY`: number of entities tracked by sync.

### Feature flags

The component is gated by `syncer::kSyncJourney`, which is **disabled by
default**. With the flag off, `HistoryBackend` never creates the bridge and the
controller is never built, so nothing is ingested and nothing appears in
`chrome://sync-internals`. Enable it locally with
`--enable-features=SyncJourney`.

What is unconditional is only the registration of `syncer::JOURNEY` itself —
its enum entry and metadata, and its membership in the History user-selectable
type. Beyond the flag, ingestion still depends on server availability and the
user's sync state.

### Schema changes

`JourneysDatabase` is a mixin, not a self-contained database, and does not carry
its own version number. Table creation and any future migrations are driven by
`HistoryDatabase`. `HistoryDatabase::Init()` calls `InitJourneysTables()` on
every open, before `EnsureCurrentVersion()`, and it creates any missing table.
So adding a table needs no version bump. Instead, add its `CREATE TABLE` to the
newest `components/test/data/history/history.N.sql`, and extend
`InitJourneysTablesOnExistingDatabase` (history_backend_db_unittest.cc) to cover
databases created before the table existed. Changing an existing table means
bumping the History database version and adding the migration alongside the
others.

[add-or-update]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_database.h?q=AddOrUpdateJourneys
[apply-incremental]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_sync_bridge.h?q=ApplyIncrementalSyncChanges
[backend-interface]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/history_backend_for_journeys_sync.h?q=HistoryBackendForJourneysSync
[backend-observer]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/history_backend_observer.h?q=HistoryBackendObserver
[bridge]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_sync_bridge.h?q=JourneysSyncBridge
[controller]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journey_data_type_controller.h?q=JourneyDataTypeController
[create-metadata-changelist]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_sync_bridge.h?q=CreateMetadataChangeList
[data-type-registration]: https://source.chromium.org/chromium/chromium/src/+/main:components/sync/base/data_type.cc?q=JOURNEY
[get-all-resolved]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_backend_util.h?q=GetAllJourneysWithResolvedVisits
[get-all-with-visits]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/history_backend.h?q=GetAllJourneysWithVisits
[get-last-row]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/visit_database.h?q=GetLastRowForVisitByVisitTime
[get-one-resolved]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_backend_util.h?q=GetJourneyWithResolvedVisits
[get-one-with-visits]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/history_backend.h?q=GetJourneyWithVisits
[history-backend]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/history_backend.h?q=HistoryBackend
[history-database]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/history_database.h?q=HistoryDatabase
[history-service]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/history_service.h?q=HistoryService
[journey]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journey.h?q=Journey
[journey-history-entry]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journey_row.h?q=JourneyHistoryEntry
[journey-row]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journey_row.h?q=JourneyRow
[journey-visit]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journey.h?q=JourneyVisit
[journeys-db]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_database.h?q=JourneysDatabase
[merge-full]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_sync_bridge.h?q=MergeFullSyncData
[metadata-db]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_sync_metadata_database.h?q=JourneysSyncMetadataDatabase
[proxy-delegate]: https://source.chromium.org/chromium/chromium/src/+/main:components/sync/model/proxy_data_type_controller_delegate.h?q=ProxyDataTypeControllerDelegate
[resolve-visits]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_backend_util.h?q=ResolveJourneyVisits
[schema]: https://source.chromium.org/chromium/chromium/src/+/main:components/history/core/browser/journeys/journeys_database.cc?q=InitJourneysTables
[specifics]: https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/journey_specifics.proto?q=JourneySpecifics
