# Security Model for Visited Links (`//components/visitedlink`)

This component maintains the hash table that Chromium uses to decide which links
to style as `:visited`. The Browser process builds and writes the table. Each
Renderer process maps a read-only copy and queries it when Blink styles links.
By default the table is partitioned: each entry is keyed by
`<link URL, top-level site, frame origin>` and salted per frame origin, so a
document can read only entries recorded from its own top-level site and frame
origin. See [README.md](README.md) and the
[Partitioning :visited links history explainer](https://github.com/explainers-by-googlers/Partitioning-visited-links-history).

## Scope

This policy applies to code in `//components/visitedlink/` and its
subdirectories: `browser/`, `common/` (including `visitedlink.mojom`), `core/`,
and `renderer/`.

It does not apply to `//components/visitedlink/test/`.

The code below sits on the other side of boundaries described in this file, but
is outside its scope. Findings there fall under the owning directory's policy
and the global policy; this file does not exclude them.

- Navigation-time salt delivery:
  `//components/history/content/browser/visited_link_navigation_throttle.cc` and
  `NavigationRequest::SetVisitedLinkSalt()` in `//content/browser/`.
- Building the partition key when a visit is recorded: `HistoryTabHelper` in
  `//chrome/browser/history/` and `ContentVisitDelegate` in
  `//components/history/content/browser/`.
- Persistence: `VisitedLinkDatabase` in `//components/history/core/browser/`.
- Lookups and styling in Blink: `VisitedLinkState` in
  `//third_party/blink/renderer/core/dom/`, including its suppression of
  `:visited` styling in credentialless iframes and fenced frames.
- Android WebView integration in `//android_webview/`.

## Security Boundaries & Threat Model

### Process type & sandbox

This code is linked into more than one process:

| Code | Process | Sandbox | Reaches this code via |
| --- | --- | --- | --- |
| `browser/` | Browser | None | `ContentVisitDelegate` in Chrome; `AwBrowserContext` in Android WebView |
| `renderer/` | Renderer | Renderer sandbox | `VisitedLinkNotificationSink` messages from the Browser; Blink lookups of link URLs from web content |
| `common/`, `visitedlink.mojom` | Browser and Renderer | Renderer Sandbox | Dependencies of `browser/` and `renderer/` |
| `core/` | Browser (all platforms, including iOS through `//components/history/core/browser`) and Renderer (through `common/`) | Renderer Sandbox | `VisitedLink` keys built by History and by `common/` |

The threat model for `common/` and `core/` assumes the most privileged host, the
Browser process.

Renderer isolation varies by platform. Android WebView has no Site Isolation and
usually runs a single renderer process, and low-memory Android devices also run
without Site Isolation. See
[Process Model and Site Isolation](https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md).

The table runs in one of three modes:

| Mode | Writer | Salt | Where it is used |
| --- | --- | --- | --- |
| Partitioned | `PartitionedVisitedLinkWriter` | A random 64-bit salt per frame origin, held by the Browser and never stored in the table | All platforms except iOS and Android WebView, by default: `PartitionVisitedLinkDatabaseWithSelfLinks` has a `"default": "stable"` status in `runtime_enabled_features.json5`, and an empty status on iOS |
| Unpartitioned (legacy) | `VisitedLinkWriter` | One salt for the whole table, stored in the table's shared header | Android WebView by default (`aw_field_trials.cc` disables partitioning there) until it migrates to the pseudo-partitioned mode (https://crbug.com/506963484); Chrome only if the feature above is disabled. `VisitedLinkWriter` is to be removed once that migration has landed (https://crbug.com/517136103) |
| Pseudo-partitioned | `PartitionedVisitedLinkWriter` with `use_constant_salt` | `kPseudoPartitionedConstantSalt` (0) | Android WebView, only when `kWebViewMigrateVisitedLinks` (`FEATURE_DISABLED_BY_DEFAULT`) is enabled |

### Inputs

- **Untrusted, in the Renderer:** link URLs and document state from web content,
  which Blink hashes and looks up through `VisitedLinkReader`.
- **Untrusted, validated before reaching this component:** navigations that web
  content initiates. They determine which keys History asks the writers to add,
  and which origins commit in which process (and therefore which salts each
  process receives). `HistoryTabHelper` takes a key's frame origin from the
  navigation's initiator origin, or from the referrer if there is none.
  `//content` checks the initiator origin against the initiating process:
  `VerifyInitiatorOrigin()` reports `bad_message::INVALID_INITIATOR_ORIGIN` on a
  mismatch.
- **Trusted, in the Browser:** the History database and, for the unpartitioned
  writer only, the `Visited Links` file in the profile directory. The Browser
  writes both. `VisitedLinkWriter::ReadFileHeader()` still checks the file's
  signature, version, size, and used-item count, and a failed check falls back
  to rebuilding the table from history.
- **Trusted, in the Renderer:** everything received over
  `VisitedLinkNotificationSink`, and the salt in navigation commit params. All
  of it comes from the Browser process.

### Trust relationships

- Defends against a compromised Renderer: Yes.

This applies to the partitioned table, which Chrome uses by default. The
unpartitioned and pseudo-partitioned tables do not keep history confidential
from a compromised renderer; see Known Issues.

The defence rests on four Browser-side properties:

- **No Renderer-to-Browser interface.** `visitedlink.mojom` defines only
  `VisitedLinkNotificationSink`. The Renderer implements it
  (`VisitedLinkReader`), and the Browser binds it through
  `RenderProcessHost::BindReceiver()` (`VisitedLinkUpdater`). Code in `browser/`
  receives no messages from renderers through this component, so there is no
  Browser-side Mojo argument or origin validation to do here.
- **Read-only transport.** Renderers receive the table as a
  `base::ReadOnlySharedMemoryRegion`. Only the Browser can write to it.
- **Salts follow commits.** The partitioned table's header holds no salt. A
  renderer process receives a frame origin's salt only when the Browser
  commits, or is about to commit, that origin in one of the process's frames.
  The salt arrives in the navigation's commit params
  (`VisitedLinkNavigationThrottle::WillProcessResponse()`, using
  `NavigationHandle::GetOriginToCommit()`). For navigations that happened while
  the table was being built, it arrives from
  `VisitedLinkEventListener::UpdateOriginSalts()`, which reads each
  `RenderFrameHost`'s `GetLastCommittedOrigin()`, or the
  `GetOriginToCommit()` of its pending navigations, on the Browser side. Without
  a frame origin's salt, `VisitedLinkReader::ComputePartitionedFingerprint()`
  returns the null fingerprint, so that origin's entries read as unvisited.
- **Profile separation.** `VisitedLinkEventListener` sends a table only to
  renderer processes of its own `BrowserContext`, and both writers reach
  `NOTREACHED()` if asked to record a visit for an off-the-record context.

### Rule of Two

The
[Rule of 2](https://chromium.googlesource.com/chromium/src/+/main/docs/security/rule-of-2.md)
allows at most two of: untrustworthy inputs, an unsafe implementation language,
and high privilege.

- **Browser:** C++ at high privilege. It does not parse web content. The only
  web-derived values it handles are keys made of `GURL`, `url::Origin`, and
  `net::SchemefulSite` values, which it hashes into 64-bit fingerprints. The
  Rule of 2 lists `GURL` and `url::Origin` as safe types. Shared table sizes are
  computed with `base::CheckedNumeric`.
- **Renderer:** C++ that handles untrustworthy link URLs, at low privilege
  inside the Renderer sandbox.

## Security Boundaries & Explicit Non-bugs

### Security boundaries

These are in scope:

- **Salt or state disclosure across origins.** A renderer process obtaining the
  salt for a frame origin, or the `:visited` state of that origin's entries,
  when the Browser would not commit that origin in the process. For a
  compromised renderer, this includes any origin outside its Site Isolation
  lock.
- **Web-exposed leaks across partitions.** Without a renderer compromise, a
  document must learn nothing about entries outside its own top-level site and
  frame origin, other than the self-links described below. Leaks through a wrong
  partition key or a wrong salt are in scope.
- **Table integrity.** Any way for a renderer to change the Browser's table
  or another renderer's copy.
- **Profile separation.** A table or salt from one `BrowserContext` reaching a
  renderer process of another (for example, an off-the-record one), or an
  off-the-record context recording visits.
- **Memory safety** in the Browser (creating, resizing, and loading tables) and
  in the Renderer (lookups on web-supplied URLs).

### Out of scope (non-bugs)

This section is **additive** to //docs/security/security-for-agents.md and
//docs/security/rules.md. Absence from this list does not imply a finding is
out of scope.

- **A document reading its own partition, and self-links.** A document can
  observe the `:visited` state of links recorded with its own top-level site and
  frame origin; the README describes this as the core design. It can also
  observe self-links: when a navigation to URL L starts from a frame whose
  origin equals the origin of its top-level site,
  `VisitedLink::MaybeCreateSelfLink()` also records `<L, site of L, origin of
  L>`, which frames of L's origin under L's site can read. The
  explainer's rationale is that a site already knows which links were
  followed from its own frames and which of its own pages were visited. Reading
  any other entry is in scope.
- **Renderer trust in the Browser-written table.** `VisitedLinkReader` takes the
  table length from the header of the read-only region the Browser created and
  does not compare it with the mapping size, and
  `VisitedLinkCommon::FingerprintAt()` does not bounds-check its index. Reports
  that assume control of the region's contents without first compromising the
  Browser are not bugs in this component. In scope: any Browser-side path,
  including one that web content can trigger (such as a resize or rebuild), that
  writes an inconsistent header or table, and any way for a process other than
  the Browser to write the region.
- **MD5 as the fingerprint hash.** Fingerprints are 64-bit values derived from
  MD5 through `MakeMd5HasherForVisitedLink()`, an approved
  `crypto::obsolete::Md5` friend. Replacing MD5 is tracked in
  https://crbug.com/427437222, and reports whose only finding is that MD5 is
  weak or deprecated duplicate it. The README (Tradeoffs, Details) explains that
  the salt, not the hash, keeps an attacker from targeting a user's
  fingerprints. In scope: a practical way to recover a salt, or to test entries
  for a frame origin whose salt the attacker's process does not hold.
- **Top-level-site bypass by a compromised renderer, for its own origins.**
  Salts are keyed by frame origin only (`VisitedLinkReader::salts_`), and on
  lookup the Renderer supplies the top-level site itself. So a renderer that is
  already compromised can read the entries of any frame origin committed in its
  own process, under every top-level site. The README states the design goal as
  keeping other origins' entries unreadable, and
  [//content/SECURITY.md](https://chromium.googlesource.com/chromium/src/+/main/content/SECURITY.md)
  treats storage partitioning and fenced frames the same way: as privacy
  features, not process-isolated boundaries. In scope: any bypass that works
  without a compromised renderer, and reading entries for a frame origin that
  the Browser would not commit in the attacker's process.

## Design Justifications & Accepted Risks

These entries give design rationale with citations. A risk called accepted here
was accepted by the component owners; none records a separate security review.
Known gaps with a tracking bug are under Known Issues, and unreviewed trade-offs
are under [Open Questions](#open-questions).

### A read-only copy of the table in every renderer

Blink queries `:visited` state many times during page load, so an IPC per query
would be too slow. Each renderer instead maps a read-only copy of the table, and
the Browser is the only writer (README, "General Overview").

### Per-origin salts, sent only where the origin commits

Partitioning salts each entry with its frame origin's salt, and a renderer
receives only the salts for origins it hosts, so that after a renderer
compromise other origins' entries stay unreadable (README, "General
Overview"; explainer, "Implementation Details"). The code does this in
`VisitedLinkNavigationThrottle::WillProcessResponse()` and
`VisitedLinkEventListener::UpdateOriginSalts()`.

### Self-links for a site's own pages

Partitioning alone would stop a site's own links from showing as `:visited`
when the user first reached those pages from another site, because the keys
differ. To preserve that history, `VisitedLink::MaybeCreateSelfLink()` also
records `<L, site of L, origin of L>` when a visit to L is recorded. This is an
accepted risk: a site can already tell which of its own pages a user has
visited, whichever site the user came from, so reading a self-link reveals
nothing new (explainer, "An exception: self-links"). The function creates a
self-link only when the frame origin matches the origin of the top-level site,
which its comment says mitigates cross-site leaks.

## Open Questions

These trade-offs have **not** been security reviewed. Owners should confirm each
one and cite a decision, or file a bug.

- **Every renderer receives every fingerprint.** Each renderer process of a
  `BrowserContext` maps the whole partitioned table and receives every
  `AddVisitedLinks()` batch. Without the salts, a compromised renderer cannot
  tell which links those entries represent. It can, however, observe the
  table's size and occupancy, and when and how many visits happen anywhere in
  the profile.

## Known Issues

These gaps have a tracking bug. A report that only restates one duplicates that
bug.

- **No per-origin secrecy outside partitioned mode.** The unpartitioned table
  keeps its single salt in `SharedHeader`, and the pseudo-partitioned table uses
  the constant `kPseudoPartitionedConstantSalt`. In either mode, any renderer of
  the `BrowserContext` can test arbitrary URLs. The unpartitioned table is
  Android WebView's default, and Chrome's only when
  `PartitionVisitedLinkDatabaseWithSelfLinks` is disabled. Because the
  pseudo-partitioned salt is the same for every user, that mode also lacks the
  random per-table salt that the README relies on against crafted MD5
  collisions. WebView does not partition `:visited` links in either mode, and
  pseudo-partitioning only moves it onto the partitioned table's
  infrastructure. That migration is tracked in https://crbug.com/506963484, and
  removing `VisitedLinkWriter` and its table-wide salt once it has landed is
  tracked in https://crbug.com/517136103.
- **One salt per origin for the whole profile.** A profile can have many
  `StoragePartition`s, but `PartitionedVisitedLinkWriter` keeps one salt per
  origin for all of them, so a frame origin gets the same salt in every
  `StoragePartition` of the profile. Whether to take `StoragePartition` into
  account is tracked in https://crbug.com/330548738.

## Reporting

Report security issues through [Chrome VRP](https://bughunters.google.com/report/vrp).
Before filing, review
[Security for Agents](https://chromium.googlesource.com/chromium/src/+/main/docs/security/security-for-agents.md)
and the
[Severity Guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/security/severity-guidelines.md).

## Further Documentation

- [README.md](README.md): how the unpartitioned and partitioned tables are
  built, persisted, and shared.
- [Partitioning :visited links history explainer](https://github.com/explainers-by-googlers/Partitioning-visited-links-history)
- [Threat Model and Defenses Against Compromised Renderers](https://chromium.googlesource.com/chromium/src/+/main/docs/security/compromised-renderers.md)
- [Stop leaks policy](https://chromium.googlesource.com/chromium/src/+/main/docs/security/stop-leaks-policy.md),
  which uses `:visited` history leaks as an example.

Generated with the use of AI according to the guidelines in
[docs/security/security_md_guidelines.md](https://chromium.googlesource.com/chromium/src/+/main/docs/security/security_md_guidelines.md).
