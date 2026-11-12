---
id: 2026-09-18-manifest-validating-type
title: 'Design: Validating Web App Manifest Mojo Type'
project: components/webapps
author: dmurph@chromium.org, AI Assistant
status: implemented
date: '2026-09-18'
completion_date: '2026-11-10'
bug: crbug.com/485890278
---

<!--
**Agent Preamble:**
> **CRITICAL:** Before reading this design or writing any code, you MUST read
> the project's AGENTS.md (if it exists).

**Execution Plans:**
*   [2026-09-18-manifest-validating-type-plan.md](../plans/2026-09-18-manifest-validating-type-plan.md)
-->

# Design: Validating Web App Manifest Mojo Type

## 1. Context and Goals

### Problem Formulation

Web App Manifests are parsed from untrusted JSON in the renderer
([manifest_parser.cc](/third_party/blink/renderer/modules/manifest/manifest_parser.cc))
and sent over
[blink::mojom::ManifestManager](/third_party/blink/public/mojom/manifest/manifest_manager.mojom)
to the browser
([manifest_manager_host.cc](/content/browser/manifest/manifest_manager_host.cc)).
Before this design, `blink.mojom.Manifest` mapped in the browser to the
generated `blink::mojom::ManifestPtr`: an open struct with public mutable
fields, an "empty" default state that was not a real manifest, and no
invariants. This caused four problems:

1. **Invalid manifest states were representable in browser code.** Nothing
   guaranteed that `start_url`, `id`, and `scope` were valid and same-origin, or
   that handler URLs belonged to the app's origin. Code in
   [components/webapps/](/components/webapps/) and
   [chrome/browser/web_applications/](/chrome/browser/web_applications/) held,
   passed, and mutated manifests freely, and defensively checked for the "empty"
   sentinel (~20 production `blink::IsEmptyManifest` call sites).

2. **The manual hardening treadmill.** `MaybeGetBadMessageStringForManifest`
   (~190 lines in `manifest_manager_host.cc`) was the only line of defense. It
   grew one CL at a time as gaps were found, over ~8 CLs (e.g. `0b080cee89a1a`
   start_url / id / scope same-origin, `7cb6a9b01576b` icon URL schemes,
   `f89efb408bbd1` control characters in file extensions). Because validation
   was a procedure callers had to remember to run, two paths skipped it:

   - `RequestManifestDebugInfo` forwarded the renderer's manifest to DevTools
     with **no** browser-side validation.
   - `ContentBrowserClient::MaybeOverrideManifest` mutated the manifest
     **after** validation (policy-provided `name` / `icons` in
     [web_app_policy_manager.cc](/chrome/browser/web_applications/policy/web_app_policy_manager.cc),
     a rewritten `id` in
     [chromeos_web_app_experiments.cc](/chrome/browser/web_applications/chromeos_web_app_experiments.cc)),
     and the result was never re-checked.

3. **Inconsistent encapsulation.** `Manifest::LaunchHandler` and
   `Manifest::DisplayOverride` were encapsulated classes with `StructTraits`.
   Other sub-types (`ImageResource`, `ShortcutItem`, ...) were typemapped open
   structs, and the top-level `Manifest` plus eight sub-types (`FileHandler`,
   `ProtocolHandler`, `MigrateFrom`, ...) were not typemapped at all.

4. **Testing was expensive.** Every validation rule was tested by injecting bad
   messages into a full browser
   ([manifest_browsertest.cc](/content/browser/manifest/manifest_browsertest.cc)),
   although most rules depended only on the manifest itself.

### Background

This design ([crbug.com/485890278](https://crbug.com/485890278)) typemaps the
top-level `blink.mojom.Manifest` to a validating C++ class, following the
pattern used for sub-types in
[manifest.h](/third_party/blink/public/common/manifest/manifest.h). Two facts
shape the design:

- **Every document has a manifest.** If the document specifies none, or it fails
  to fetch or parse, the renderer produces the spec default (`start_url` =
  document URL, `id` = `start_url` without the fragment, `scope` = `id` without
  the filename; `ManifestManager::DefaultManifest()` in
  [manifest_manager.cc](/third_party/blink/renderer/modules/manifest/manifest_manager.cc)).
  Prior to nullable responses, the renderer sent an "empty" value only when
  there was nothing to evaluate: `kUnexpectedFailure` and `kNoManifestAllowed`
  (opaque origins, `about:`). Those are errors, not manifests, as
  `RequestManifestAndErrors => result<Manifest, RequestManifestError>` models.
- **Almost every check can be phrased without the document.** Every host check
  against the document origin ran after establishing
  `document_origin.IsSameOriginWith(start_url)`. Given that, comparing a field
  against the document origin is equivalent to comparing it against
  `start_url`'s origin, for every scheme (see the
  [equivalence argument](#equivalence-argument)). Only that first comparison
  needs the document.

### Goals

1. **Every `blink::Manifest` is valid.** A copyable, encapsulated value type
   (private members, const accessors, a `Builder`) whose **intrinsic
   invariants** (the checks that need only the manifest) hold for every
   instance, however it was created: Mojo, `Builder`, or embedder override.
   There is no empty state, so `start_url`, `id`, and `scope` are always valid.
   Where there may be no manifest, APIs return `std::optional` or
   `base::expected`.
2. **Complete representation.** `blink::Manifest` holds every field of
   `blink.mojom.Manifest`, nested types are C++ values (no `mojom::...Ptr`), and
   a fully populated manifest round-trips through Mojo without loss.
3. **Enforce at construction, not at call sites.** Intrinsic invariants run in
   `StructTraits<ManifestDataView, blink::Manifest>::Read` and
   `Manifest::Builder::Build`, sharing one implementation. The one
   document-dependent check (the manifest's origin equals the committed document
   origin) stays in `ManifestManagerHost`, the single place renderer manifests
   are handed to consumers.
4. **Cover embedder overrides.** `MaybeOverrideManifest` produces a new manifest
   through `Builder`, so overrides (including enterprise-policy customizations)
   satisfy the same invariants.
5. **Preserve prior enforcement exactly.** Same accept/reject behavior for
   legitimate renderers, and no new renderer-kill conditions. The one path that
   gained validation, `RequestManifestDebugInfo` (DevTools), carries the same
   parser output as `RequestManifest`.
6. **Fast unit tests.** Intrinsic rules are unit-tested through `Builder` and
   `mojo::test::SerializeAndDeserialize`. Browser tests cover only
   document-dependent checks and end-to-end wiring.

### Non-Goals

1. **Changing the Blink-side type.** The renderer keeps the generated
   `mojom::blink::Manifest` (only `cpp_typemaps` change, not
   `blink_cpp_typemaps`). The parser is unchanged, apart from sending null
   instead of an empty manifest for failure results.
2. **Removing or re-plumbing `WebAppInstallInfo`.** Preinstalled, placeholder,
   sync, and policy installs build `web_app::WebAppInstallInfo` directly and
   never create a `blink::Manifest`, so this design does not protect them.
   Making `WebAppInstallInfo` hold a `blink::Manifest` is a larger, separate
   project ([Future Work](#9_future-work-technical-debt)).
3. **Unifying with the persistence proto or the desktop model.**
   `web_app::proto::WebApp` (disk and sync format) and `web_app::WebApp` /
   `WebAppInstallInfo` (desktop install model) have different roles, and
   `//third_party/blink` cannot depend on `//chrome`. Android also uses
   `blink::Manifest` without the desktop web app system.
4. **New validation rules** (e.g. string length limits, fragment-free `id`).
   Each could kill renderers for legitimate sites unless the parser changes
   first ([Future Work](#9_future-work-technical-debt)).

______________________________________________________________________

## 2. Proposed Architecture

### Subsystem Boundaries

```
[Untrusted Renderer Process]
  │  ManifestParser: JSON -> mojom::blink::Manifest (generated, unchanged)
  ▼
[Mojo IPC Boundary]   blink.mojom.Manifest
  ▼
[Browser: StructTraits<ManifestDataView, blink::Manifest>::Read]
  │  structural checks (existing sub-type traits)
  │  + intrinsic invariants  -- fails -> DeserializationError::CustomCode
  ▼                                      (bad message, renderer killed)
[content/: ManifestManagerHost]
  │  contextual checks: opaque document origin, result consistency,
  │  manifest.GetOrigin() == committed document origin,
  │  manifest_url match (ParseManifestFromString)
  │  -> ReportBadMessage / error result
  │  MaybeOverrideManifest -> new blink::Manifest via Builder (re-validated)
  ▼
[Consumers: components/webapps, chrome/browser/web_applications, favicon,
 DevTools]  receive const blink::Manifest& -- always valid, same-origin with
            the document it came from
```

### Platforms Affected

- [x] Windows
- [x] Mac
- [x] Linux
- [x] ChromeOS
- *Android Form Factors:*
  - [x] Android (Smartphones/Tablets)
  - [x] Chrome Custom Tabs (CCT)
  - [x] Android WebView (where manifests are supported)
- [ ] iOS (N/A: does not use Blink manifest IPC)

### Process & Thread Model

The renderer parses on its main thread, unchanged apart from sending null for
failure results. The browser validates on the UI thread, inside `Read()` while
dispatching the message and then in `ManifestManagerHost`. All checks are
in-memory URL and string comparisons. No new IPCs.

______________________________________________________________________

### Validation Model

#### Intrinsic invariants (manifest only)

Implemented once in blink common (`manifest_invariants.{h,cc}`) and run by both
`Read()` and `Builder::Build()`. They are private (`Read()` is a friend), so
there is no public "validate" method to forget to call. A failure is a
`ManifestInvariantError`, with one enumerator per sub-check of the rules below
(e.g. `kFileHandlerInvalidExtension`). Because it is recorded in UMA and crash
keys, the enum is append-only, mirrored in `enums.xml`, and guarded by an
IfThisThenThat block.

With `O = url::Origin::Create(start_url)`, every check from
`MaybeGetBadMessageStringForManifest` maps to an intrinsic rule with identical
accept/reject behavior (Goal 5):

| #   | Prior host check (vs. `document_origin`)                                                                                                                                 | Intrinsic rule (vs. `start_url` / `O`)                                                                         |
| :-- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :------------------------------------------------------------------------------------------------------------- |
| I1  | non-empty ⇒ `start_url`, `id`, `scope` valid                                                                                                                             | Always: `start_url`, `id`, `scope` valid (there is no empty state)                                             |
| I2  | `start_url` same-origin with document (implies `O` non-opaque)                                                                                                           | `O` non-opaque (the document comparison moves to C3)                                                           |
| I3  | `id`, `scope` same-origin with document                                                                                                                                  | `id`, `scope` same-origin with `O`                                                                             |
| I4  | `share_target.action` same-origin with document                                                                                                                          | same-origin with `O`                                                                                           |
| I5  | `file_handlers[i].action` same-origin; parameterless `type/subtype` MIME with valid top-level; extensions non-empty, start with `.`, length > 1, no control/format chars | same, with the origin rule vs. `O` and a standard IANA top-level MIME type (excluding `x-*` pseudo-MIME types) |
| I6  | `protocol_handlers[i].url` same-origin; scheme valid for level from `IsIsolatedAppScheme(document_origin.scheme())`                                                      | same-origin with `O`; level from `IsIsolatedAppScheme(O.scheme())`                                             |
| I7  | `note_taking.new_note_url` **if valid**, same-origin with document                                                                                                       | **if valid**, same-origin with `O` (the parser sends an empty URL for `{}`)                                    |
| I8  | `lock_screen.start_url` **if valid**, same-origin with document                                                                                                          | **if valid**, same-origin with `O`                                                                             |
| I9  | `migrate_from[i].id` valid and same-site with document; `install_url` (if set) valid, same-site with document, same-origin with `id`                                     | same, with same-site vs. `SchemefulSite(O)`                                                                    |
| I10 | `migrate_to.id` valid and same-site; `install_url` valid, same-site, same-origin with `id`                                                                               | same, with same-site vs. `SchemefulSite(O)`                                                                    |
| I11 | `shortcuts[i].url` valid; same-origin with `scope` and `path().starts_with(scope.path())`                                                                                | identical; keep the plain, non-segment-aware prefix match                                                      |
| I12 | `scope_extensions[i].origin` not opaque and `https`                                                                                                                      | identical                                                                                                      |
| I13 | `icons[i].src` valid; scheme is http, https, data, or `document_origin.scheme()`                                                                                         | scheme is http, https, data, or `O.scheme()`                                                                   |

#### Contextual checks (need the document)

Performed only in `ManifestManagerHost`:

| #   | Rule                                                                                                                                    | Failure                                                                     |
| :-- | :-------------------------------------------------------------------------------------------------------------------------------------- | :-------------------------------------------------------------------------- |
| C0  | `ParseManifestFromString` precondition, **checked before sending the IPC**: the browser-supplied `document_url` is valid and non-opaque | Return `std::nullopt` asynchronously; no IPC is sent                        |
| C1  | `kSuccess` must come with a manifest                                                                                                    | `ReportBadMessage`                                                          |
| C2  | Committed document origin is not opaque (can fail on a race)                                                                            | Error result, `WebApp.Manifest.ForOpaqueOrigin` UMA (**not** a bad message) |
| C3  | `manifest.GetOrigin()` same-origin with the committed document origin (or `document_url`'s origin for `ParseManifestFromString`)        | `ReportBadMessage`                                                          |
| C4  | `ParseManifestFromString`: `manifest_url` equals the requested URL exactly                                                              | `ReportBadMessage`                                                          |
| C5  | `RequestManifestAndErrors`: an error result must not carry `kSuccess` or `kNoManifestSpecified` (existing check)                        | `ReportBadMessage`; error coerced to `kUnexpectedFailure`                   |

**Evaluation order.** Intrinsic checks always run first, inside `Read()`, before
any context is considered.

- `RequestManifest` / `RequestManifestAndErrors`: intrinsic, then C1 (or C5 for
  the error arm), C2, C3, then the override. Previously, the host ran the
  opaque-origin check first, then C1 and content checks, then the override.
- `ParseManifestFromString`: C0 (before the IPC), intrinsic, C4, C3. C0 makes C2
  impossible on this path, so it is a `CHECK`.
- `RequestManifestDebugInfo`: intrinsic only.

#### Equivalence argument

`url::Origin::IsSameOriginWith(const GURL&)` is exactly
`!opaque() && *this == Origin::Create(url)`. So the prior
`document_origin.IsSameOriginWith(start_url)` holds iff the document origin is
non-opaque and equals `O`. Given that (C2 + C3), every rule stated against `O`,
`SchemefulSite(O)`, or `O.scheme()` accepts and rejects exactly what the prior
rule stated against `document_origin` does. This includes `blob:` /
`filesystem:` (inner origin on both sides), `isolated-app:` /
`chrome-extension:` (standard schemes), `file:`, and embedder local schemes.

**Why running intrinsic checks first is safe, and why C0 exists.** A renderer
can only fail an intrinsic rule by sending an internally inconsistent manifest,
which the parser does not produce from a valid, non-opaque document URL (except
for over-long URLs, below). `RequestManifest*` is gated by
`ManifestManager::CanFetchManifest` (no opaque origins, no `about:`).
`ParseManifestFromString` has no such gate: the browser supplies `document_url`,
and the parser falls back to it for `start_url`. A `data:` or invalid
`document_url` would make a benign renderer fail I1 / I2 (valid `start_url`,
non-opaque `O`) and get killed. C0 rejects these inputs before sending the IPC,
replacing the prior empty-manifest return. The same reasoning assumes Blink and
`//url` agree on scheme registration, which holds because embedder schemes are
registered on both sides through `ContentClient`.

> **Note (existing exception: over-long URLs):** The KURL Mojo traits send a URL
> longer than `url::kMaxURLChars` (2 MiB) as an empty URL. `ManifestManager`
> guards only `manifest_url`, `id`, `start_url`, and `scope`, and only on the
> `RequestManifest*` path. Any other URL field over 2 MiB (e.g. a `data:` icon)
> arrives empty and terminates the renderer (`ImageResource` traits reject an
> empty `src`). This design keeps that behavior; fixing it in the parser is
> [Future Work](#9_future-work-technical-debt).

#### Failure reporting

- **Intrinsic failure in `Read()`:** returns
  `base::unexpected(mojo::DeserializationError::CustomCode(static_cast<int>(error)))`.
  Mojo reports a validation error, and `RenderProcessHostImpl::OnMojoError`
  terminates the renderer with a browser-side `DumpWithoutCrashing`. The crash
  keys `mojo-bad-message-trace-1..4` hold the file:line and custom code, which
  identify the exact rule. `WebApp.Manifest.InvariantViolation` is also
  recorded.
- **Intrinsic failure in `Builder::Build()`:** returns
  `base::unexpected(ManifestInvariantError)`; the caller decides what to do (see
  [Embedder overrides](#embedder-overrides): CHECK for code-built values, drop
  for policy values).
- **Contextual failure:** `mojo::ReportBadMessage` with a **static** string.
  This keeps URLs out of crash keys, avoids allocating on error paths, and
  groups crash reports cleanly. The two formerly dynamic messages (`start_url`
  spec and protocol handler security level) use static strings, and triage uses
  the rule code and file:line instead.

______________________________________________________________________

### Data Models & Schemas

#### `blink::Manifest`

`blink::Manifest` wraps a non-null `scoped_refptr<const Data>`
(`base::RefCountedThreadSafe`), where `Data` holds all 40 fields of
`manifest.mojom` plus the cached `origin`. This keeps
`sizeof(Manifest) == sizeof(void*)` with `O(1)` copy and move while preserving
value semantics (`operator==` compares `Data` fields after a pointer fast path).
Reference-returning accessors and `GetOrigin()` carry `LIFETIME_BOUND`
([compiler_specific.h](/base/compiler_specific.h)) so Clang diagnoses dangling
references to temporaries across the `Data` pointer indirection. Sketch:

```cpp
namespace blink {

class BLINK_COMMON_EXPORT Manifest {
 public:
  // Existing nested types (ImageResource, ShortcutItem, ShareTarget,
  // LaunchHandler, DisplayOverride, ...) are unchanged. New nested value types
  // replace mojom::...Ptr: Screenshot, FileHandler, ProtocolHandler,
  // ScopeExtension, LockScreen, NoteTaking, MigrateFrom, MigrateTo. E.g.:
  struct BLINK_COMMON_EXPORT FileHandler {
    using LaunchType = mojom::ManifestFileHandler_LaunchType;
    bool operator==(const FileHandler&) const = default;

    GURL action;
    std::u16string name;
    base::flat_map<std::u16string, std::vector<std::u16string>> accept;
    LaunchType launch_type = LaunchType::kSingleClient;
  };

  class BLINK_COMMON_EXPORT Builder;

  // The spec-defined default manifest for `document_url`, computed exactly as
  // the renderer's `ManifestManager::DefaultManifest()` does (scope from
  // `KURL::BaseAsString()` semantics, *not* `GURL::GetWithoutFilename()`,
  // which differs for e.g. `blob:` URLs). Returns std::nullopt for documents
  // that have no default manifest (invalid, opaque origin, `about:`), the same
  // ones the renderer refuses via `CanFetchManifest`.
  static std::optional<Manifest> CreateDefault(const GURL& document_url);

  // Computes the spec-defined default scope for `document_url` using
  // `KURL::BaseAsString()` semantics.
  static GURL GetDefaultScope(const GURL& document_url);

  // Only Mojo can construct an uninitialized instance
  // (`default_constructible = false`). There is no public default constructor
  // and no empty state.
  explicit Manifest(mojo::DefaultConstruct::Tag);
  // Copyable and movable (O(1) via scoped_refptr<const Data>); has operator==.

  // Starts a Builder initialized with all fields of this manifest.
  Builder ToBuilder() const;

  // url::Origin::Create(start_url()). Never opaque.
  const url::Origin& GetOrigin() const LIFETIME_BOUND;

  // One const accessor per mojom field, for example:
  const GURL& start_url() const LIFETIME_BOUND;  // Always valid.
  const GURL& id() const LIFETIME_BOUND;         // Always valid, same-origin.
  const GURL& scope() const LIFETIME_BOUND;      // Always valid, same-origin.
  const std::optional<std::u16string>& name() const LIFETIME_BOUND;
  const std::vector<FileHandler>& file_handlers() const LIFETIME_BOUND;
  std::optional<SkColor> theme_color() const;
  // ...

 private:
  struct Data;  // RefCountedThreadSafe<Data>; holds all 40 fields + `origin`.

  friend struct mojo::StructTraits<mojom::ManifestDataView, Manifest>;
  friend class Builder;

  std::optional<ManifestInvariantError> ValidateInvariants() const;

  scoped_refptr<const Data> data_;
};

}  // namespace blink
```

#### `blink::Manifest::Builder`

```cpp
class BLINK_COMMON_EXPORT Manifest::Builder {
 public:
  Builder(GURL start_url, GURL id, GURL scope);
  // Initializes `start_url`, defaulting `id` to `start_url.GetWithoutRef()`
  // and `scope` to `Manifest::GetDefaultScope(start_url)`.
  explicit Builder(GURL start_url);

  Builder& SetName(std::optional<std::u16string> name);
  Builder& SetIcons(std::vector<ImageResource> icons);
  // ... one setter per field ...

  // Runs the intrinsic checks and consumes the builder's internal `Data`
  // reference (subsequent calls CHECK). The primary way to construct a
  // Manifest outside Mojo.
  base::expected<Manifest, ManifestInvariantError> Build();

  // CHECKs that the intrinsic checks pass. Only for inputs that are valid by
  // construction (tests, code-built overrides such as the ChromeOS `id`
  // rewrite).
  Manifest BuildChecked();

 private:
  scoped_refptr<Data> data_;
};
```

Because `Build()` returns the failure reason, every intrinsic rule can be
unit-tested through the public `Builder`, with no test-only back door.

______________________________________________________________________

### API Surface & Mojo Interfaces

#### Mojo interface changes

In
[manifest_manager.mojom](/third_party/blink/public/mojom/manifest/manifest_manager.mojom),
the two responses that previously used an empty manifest to mean "no manifest"
are nullable:

```
RequestManifest()
    => (ManifestRequestResult result, url.mojom.Url url, Manifest? manifest);
RequestManifestDebugInfo()
    => (url.mojom.Url url, Manifest? parsed_manifest, ManifestDebugInfo debug_info);
// Unchanged:
RequestManifestAndErrors() => result<Manifest, RequestManifestError>;
ParseManifestFromString(...) => (Manifest? parsed_manifest);
```

The renderer sends `null` where it previously sent an empty manifest
(`kUnexpectedFailure`, `kNoManifestAllowed`). The `?` on `RequestManifest` is
transitional: `Page::GetManifest`, its only user, is being replaced by
`PageManifestManager` ([crbug.com/452053908](https://crbug.com/452053908)),
after which `RequestManifest` can be deleted.

#### Typemap

Extend the **existing** manifest typemap block in
[third_party/blink/public/mojom/BUILD.gn](/third_party/blink/public/mojom/BUILD.gn)
(`cpp_typemaps` only):

```gn
{
  mojom = "blink.mojom.Manifest"
  cpp = "::blink::Manifest"
  default_constructible = false
},
# Plus one entry per newly typemapped nested type, e.g.:
{ mojom = "blink.mojom.ManifestFileHandler" cpp = "::blink::Manifest::FileHandler" },
```

`Manifest?` maps to `std::optional<blink::Manifest>`, and
`result<Manifest, RequestManifestError>` maps to
`base::expected<blink::Manifest, blink::mojom::RequestManifestErrorPtr>`.

#### Content public API

| API                                                                                               | Before                               | After                                                                   |
| :------------------------------------------------------------------------------------------------ | :----------------------------------- | :---------------------------------------------------------------------- |
| [`Page::GetManifestCallback`](/content/public/browser/page.h)                                     | `(result, const GURL&, ManifestPtr)` | `(result, const GURL&, std::optional<blink::Manifest>)` (until removed) |
| [`PageManifestManager::ManifestResult`](/content/public/browser/page_manifest_manager.h)          | `expected<ManifestPtr, ErrorPtr>`    | `expected<blink::Manifest, ErrorPtr>`                                   |
| `PageManifestManager::ParseManifestCallback`                                                      | `(ManifestPtr)` (empty on failure)   | `(std::optional<blink::Manifest>)`                                      |
| [`ContentBrowserClient::MaybeOverrideManifest`](/content/public/browser/content_browser_client.h) | `void(RFH*, ManifestPtr&)`           | `std::optional<blink::Manifest>(RFH*, const blink::Manifest&)`          |

#### Embedder overrides

```cpp
// Returns std::nullopt if no override applies.
virtual std::optional<blink::Manifest> MaybeOverrideManifest(
    RenderFrameHost* render_frame_host, const blink::Manifest& manifest);
```

Implementations build the new manifest with `manifest.ToBuilder()`, so it passes
through the intrinsic checks.

- **Chaining:** `ChromeContentBrowserClient` runs the ChromeOS experiment first,
  then the policy override, which looks the app up by the (possibly rewritten)
  `id`. The policy step takes the ChromeOS result if there is one, otherwise the
  original; the combined result is returned, or `std::nullopt` if neither
  applied.
- **ChromeOS experiment** (`id` rewrite): the new `id` is same-origin with
  `start_url` by construction, so it uses `BuildChecked()`.
- **Policy override** (`WebAppPolicyManager`, `name` and `icons` from
  `CustomManifestValues`):
  - `WebAppPolicyManager` populates `name` from `CustomManifestValues` and
    retains `ToBuilder().SetIcons(...)` support for policy custom icons across
    manifest updates ([crbug.com/568065357](https://crbug.com/568065357)).
  - Policy values are external input, so this step uses `Build()`. Custom icon
    URLs are already required to be https when the policy is read, which
    satisfies I13 (icon scheme). If `Build()` still fails, the policy override
    is dropped: the manifest is delivered without it, and UMA records the
    failure. It never crashes or calls `ReportBadMessage`, because the renderer
    did nothing wrong.
- **Host invariant:** `ManifestManagerHost` CHECKs that an override does not
  change `GetOrigin()`; doing so is an embedder bug.

#### `ManifestManagerHost`

```cpp
// Returns the manifest to deliver, or an error result.
base::expected<blink::Manifest, blink::mojom::ManifestRequestResult>
ManifestManagerHost::ValidateAndMaybeOverrideManifest(
    blink::mojom::ManifestRequestResult result,
    std::optional<blink::Manifest> manifest) {
  // Intrinsic invariants already hold (enforced in Read()).
  if (!manifest) {
    if (result == blink::mojom::ManifestRequestResult::kSuccess) {  // C1
      mojo::ReportBadMessage(
          "RequestManifest reported success but didn't return a manifest");
      return base::unexpected(kUnexpectedFailure);
    }
    return base::unexpected(result);
  }
  const url::Origin& document_origin =
      page().GetMainDocument().GetLastCommittedOrigin();
  if (document_origin.opaque()) {  // C2: race, not a bad message.
    base::UmaHistogramBoolean("WebApp.Manifest.ForOpaqueOrigin", true);
    return base::unexpected(kUnexpectedFailure);
  }
  if (!document_origin.IsSameOriginWith(manifest->GetOrigin())) {  // C3
    mojo::ReportBadMessage("Manifest origin must match the document origin.");
    return base::unexpected(kUnexpectedFailure);
  }
  if (std::optional<blink::Manifest> overridden =
          GetContentClient()->browser()->MaybeOverrideManifest(
              &page().GetMainDocument(), *manifest)) {
    CHECK(overridden->GetOrigin() == manifest->GetOrigin());
    return *std::move(overridden);
  }
  return *std::move(manifest);
}
```

`ParseManifestFromString` checks C0 (valid, non-opaque `document_url`) and posts
`std::nullopt` without an IPC if it fails. Its response handler runs C4
(`manifest_url` match), `CHECK`s that the `document_url` origin is non-opaque
(guaranteed by C0), runs C3 against that origin, and returns `std::nullopt` on
any failure. `MaybeGetBadMessageStringForManifest` is deleted.

> **Note (result-code change for `Page::GetManifest` callers):** Previously,
> when C2 or C3 failed, callers received the renderer's result (e.g. `kSuccess`)
> with an empty manifest. `ValidateAndMaybeOverrideManifest` instead returns
> `kUnexpectedFailure` and `std::nullopt`.

**Browser-created empty manifests** also became `std::nullopt` or an error
result: `DispatchManifestNotFound` (pending callbacks on destruction or
connection error), the `about:` early returns in `GetManifest` and
`RequestManifestDebugInfo`, and the `ParseManifestFromString` default
(previously `WrapCallbackWithDefaultInvokeIfNotRun(..., ManifestPtr())`).
DevTools (`page_handler.cc`, `GotManifest`) handles a `std::nullopt` manifest.

______________________________________________________________________

## 3. Alternatives Considered

### A. Keep the generated struct and add a free validation function

Move `MaybeGetBadMessageStringForManifest` into `manifest_util.h`.

- **Pros:** Minimal churn.
- **Cons:** Validation is still a procedure callers must remember; the DevTools
  and override paths stay unvalidated; consumers still check for emptiness.
- **Verdict:** Rejected. It moves code without guaranteeing anything.

### B. Validating class, with intrinsic checks run by the host after deserialization

`Read()` does only structural checks; the host calls public
`ValidateContextFree()` / `ValidateAgainstDocument()` and reports descriptive
`ReportBadMessage` strings.

- **Pros:** Keeps the host's explicit-error path and descriptive diagnostics.
- **Cons:** Unvalidated `blink::Manifest` instances exist in the browser (fresh
  out of `Read()`, the DevTools path, overrides), so the type guarantees
  nothing. It is the same treadmill wearing a class.
- **Verdict:** Rejected. Diagnostics are covered by
  `DeserializationError::CustomCode` plus file:line crash keys.

### C. Distinct `ValidatedManifest` type produced only by the host

An unvalidated `blink::Manifest` for the wire, plus a separate type (or passkey)
that only `ManifestManagerHost` / `Builder` can produce.

- **Pros:** Also captures C3 (origin matches the document) in the type system.
- **Cons:** Two public manifest types and more conversion code. Intrinsic checks
  don't need the document, so enforcing them in `Read()` gives almost the same
  guarantee with one type, and consumers with a document can check C3 in one
  line with `GetOrigin()`.
- **Verdict:** Rejected. Revisit only if consumers need a type-level "checked
  against this document" guarantee.

### D. Send `document_origin` in the message so `Read()` can do everything

- **Cons:** The origin would come from the untrusted renderer, so the browser
  would still have to compare it with `GetLastCommittedOrigin()`. Manifests also
  don't always have a document (`ParseManifestFromString`).
- **Verdict:** Rejected. It looks like security without providing any.

### E. Keep an "empty" state in `blink::Manifest`

Keep `CreateEmpty()` / `is_empty()` so the existing responses and consumer "not
fetched yet" sentinels work unchanged.

- **Pros:** No mojom change; less consumer churn.
- **Cons:** Every consumer still has to check `is_empty()` before trusting
  `start_url()`. It contradicts the spec model, where every document has a
  manifest and "no manifest" is an error.
- **Verdict:** Rejected. "No manifest" is `std::nullopt` / `base::unexpected`.

### F. Wait for crbug.com/452053908 instead of making `RequestManifest` nullable

Migrate every `Page::GetManifest` caller to `PageManifestManager` and delete
`RequestManifest`, so no mojom nullability change is needed.

- **Pros:** No transitional `?`.
- **Cons:** Puts a separate, larger migration on this project's critical path.
- **Verdict:** Not required. Making the response nullable kept this project
  independent of crbug.com/452053908.

______________________________________________________________________

## 4. Core Principle Considerations

### Speed & Efficiency

Neutral to positive. The same checks (< 0.1 ms) run inside `Read()` off startup
and render-blocking paths. When `migrate_from` and `migrate_to` are absent,
`ValidateInvariants()` skips `net::SchemefulSite` construction. Nested
`StructPtr` heap allocations became inline values inside one shared immutable
`RefCountedThreadSafe` `Data` payload. This keeps `sizeof(blink::Manifest)` at
`sizeof(void*)` (8 bytes on 64-bit) instead of ~2.2 KiB inline, with `O(1)`
copies and moves. Deleting the host validation and `IsEmptyManifest` plumbing
partly offsets the binary size added by the traits and `Builder`.

### Security

- **Threat model:** A compromised renderer sends arbitrary
  `blink.mojom.Manifest` data (cross-origin start or handler URLs, malformed
  file extensions, restricted protocol schemes) to get privileged browser
  behavior (installation, OS registration, protocol handling).
- **Rule of Two:** JSON parsing stays in the sandboxed renderer; the browser
  only deserializes typed Mojo data.
- **Attack surface:** Reduced. Every `blink::Manifest` satisfies the intrinsic
  invariants, whether it came from the wire, the DevTools path, or an embedder
  override, and the single contextual origin check (C3) sits where renderer
  manifests are handed to consumers.

### Stability & Simplicity

- **Failure modes:** See [Failure reporting](#failure-reporting) (renderer
  killed with a rule-identifying crash dump, or a static `ReportBadMessage`) and
  [Embedder overrides](#embedder-overrides) (`BuildChecked()` CHECKs for the
  ChromeOS rewrite; failed policy overrides are dropped).
- **Simplicity:** One implementation of the invariants, shared by `Read()` and
  `Builder`. Deleted ~190 lines of host validation and ~20 consumer emptiness
  checks.

### Caveats & Risks

1. **Different failure path for intrinsic violations.** Previously, a bad
   manifest received `ReportBadMessage` and the host still delivered an explicit
   error. Because a failure in `Read()` closes the pipe rather than invoking the
   response callback, `OnConnectionError` notifies pending `GetManifest`
   callbacks and in-flight `GetSpecifiedManifest` / `GetAllSpecifiedManifests`
   subscribers with `kUnexpectedFailure` before deleting the host.
2. **Exact parity.** Paraphrasing a rule (e.g. making the shortcut scope check
   segment-aware, or dropping the `is_valid()` guard on `note_taking` /
   `lock_screen`) would kill benign renderers. A temporary parity test verified
   agreement on every return path before `MaybeGetBadMessageStringForManifest`
   was deleted.
3. **Consumer churn.** About 147 non-renderer files referenced
   `blink::mojom::Manifest[Ptr]`. Consumers that used an empty manifest as a
   "not fetched yet" sentinel (`InstallablePageData`, `SiteManifestMetricsTask`,
   `MLInstallabilityPromoter`, `FakeInstallableManager`, ...) switched to
   `std::optional<blink::Manifest>`.
4. **Test construction of invalid manifests.** Browser tests that previously
   injected internally inconsistent manifests through
   `ValidateAndMaybeOverrideManifestForTesting(ManifestPtr)` can't be written
   with `blink::Manifest`. Those cases live in unit tests; browser tests keep
   only contextual cases (a valid manifest for a different origin).

______________________________________________________________________

## 5. Privacy, Enterprise & A11y

- **Privacy:** N/A: no user data is collected or transmitted. Static bad-message
  strings slightly reduce URL data in crash reports.
- **Enterprise:** No policy behavior changes. The `WebAppPolicyManager` `name` /
  `icons` override uses the value-returning hook and drops the override if it
  fails validation; the `icons` override stays supported for
  [crbug.com/568065357](https://crbug.com/568065357) (see
  [Embedder overrides](#embedder-overrides)).
- **Accessibility:** N/A: no UI changes.

______________________________________________________________________

## 6. Metrics & Rollout Plan

### Success & Regression Metrics

- New: `WebApp.Manifest.InvariantViolation` (enumeration of
  `ManifestInvariantError`), recorded on `Read()` failure, with a
  `.PolicyOverride` variant recorded when a policy override is dropped.
- Existing: `WebApp.Manifest.ForOpaqueOrigin`,
  `Stability.BadMessageTerminated.Content`. Monitored alongside the typemap
  rollout.

### Rollout & Finch

The typemap and `StructTraits` are compile-time and can't be put behind a
`base::Feature`. Safety came from exact parity with prior rules (verified by the
parity test), landing the checks host-side first with the prior explicit-error
path and moving them into `Read()` last (see
[Detailed Implementation Breakdown](#8_detailed-implementation-breakdown)), and
monitoring the metrics above. Any future **new** invariant goes behind a feature
in report-only mode (`DumpWithoutCrashing` + UMA) before it is enforced.

______________________________________________________________________

## 7. Testing Plan

### Unit tests

- **`manifest_unittest.cc` (blink_common_unittests):**
  - For every row I1–I13: an input that violates it makes `Build()` return that
    exact `ManifestInvariantError`, and the boundary case passes (e.g. empty
    `note_taking.new_note_url`; shortcut `/app` vs. scope `/application` follows
    non-segment-aware prefix semantics; percent-encoding).
  - `ManifestTest.CreateDefault` verifies `CreateDefault` passes all invariants
    for `http:`, `https:`, `file:`, `blob:`, and `filesystem:` URLs and returns
    `std::nullopt` for empty/invalid, `about:blank`, and `data:` URLs.
    `ManifestManagerTest.DefaultManifestParity` verifies exact parity with the
    renderer's `ManifestManager::DefaultManifest()` across `http:` and `https:`
    URLs, including directory paths and `#section` fragments.
  - `m.ToBuilder().BuildChecked() == m`.
- **`manifest_mojom_traits_unittest.cc`:**
  - A manifest with **every** field populated survives `SerializeAndDeserialize`
    unchanged (catches missing fields).
  - An invalid generated `blink::mojom::Manifest` fails to deserialize into
    `blink::Manifest`.
- **Parity test (temporary):** for inputs covering every return path of
  `MaybeGetBadMessageStringForManifest`, the intrinsic checks accepted or
  rejected the same inputs. Verified before deleting
  `MaybeGetBadMessageStringForManifest`.
- **`web_app_policy_manager_unittest.cc`:** Policy overrides with `icons`
  populated (`MaybeOverrideManifestWithIcons`) and invalid policy overrides that
  fail `Build()` being dropped while recording
  `WebApp.Manifest.InvariantViolation.PolicyOverride` (no crash or bad message).

### Browser tests

- **`manifest_browsertest.cc`:** End-to-end wiring and host checks — C0 (no IPC
  sent), C1–C5 (valid-but-cross-origin manifests triggering `ReportBadMessage`
  with static strings, opaque-origin race, `ParseManifestFromString` URL
  mismatch), default manifest delivery, the override origin CHECK, and
  connection-error notification of subscribers.
- **`chromeos_web_app_experiments_browsertest.cc`:** ChromeOS `id` rewrite via
  `Manifest::ToBuilder()`.
- **`web_app_policy_manager_browsertest.cc`:** Force-installed custom name and
  icon policy overrides.

```bash
tools/autotest.py -C out/Default third_party/blink/common/manifest/manifest_unittest.cc
tools/autotest.py -C out/Default third_party/blink/common/manifest/manifest_mojom_traits_unittest.cc
tools/autotest.py -C out/Default content/browser/manifest/manifest_browsertest.cc
```

______________________________________________________________________

## 8. Detailed Implementation Breakdown

Subsystems changed, implemented in dependency order across staged CLs:

1. **C0 (content).** Add the `ParseManifestFromString` `document_url`
   precondition. No renderer-visible change; it removed the renderer-kill hazard
   before intrinsic checks moved into `Read()`.
2. **Mojo nullability (renderer + browser).** `RequestManifest` and
   `RequestManifestDebugInfo` return `Manifest?`; the renderer sends null for
   `kUnexpectedFailure` / `kNoManifestAllowed`, and the browser maps null to its
   prior empty handling.
3. **`blink::Manifest` type (blink common).** All fields, nested value types,
   `Builder`, `ToBuilder`, `CreateDefault`, the intrinsic invariants and
   `ManifestInvariantError`, plus temporary bridges
   `base::expected<Manifest, ManifestInvariantError> ManifestFromMojom(const mojom::Manifest&)`
   and `mojom::ManifestPtr ManifestToMojom(const Manifest&)`. Unit tests,
   including round-trip, parity, and `CreateDefault` tests.
4. **Content layer (internal).** `ManifestManagerHost` converts incoming
   manifests (including the `RequestManifestDebugInfo` response) with
   `ManifestFromMojom`; a failure calls `ReportBadMessage` with
   `ManifestInvariantErrorToString(error)`, keeping the host's explicit-error
   path. It then runs C1–C5. Content public APIs remained Mojo-typed during
   consumer migration (the host called `ManifestToMojom()` at the boundary), so
   consumers didn't change. `MaybeGetBadMessageStringForManifest` was deleted
   once parity tests passed.
5. **Content public API, one API per change, with its consumers.**
   `MaybeOverrideManifest` (with explicit chaining and the ChromeOS and policy
   overrides), `PageManifestManager`, `Page::GetManifest` (2 production
   callers), and the DevTools `page_handler.cc` callback each switch to
   `blink::Manifest`, moving the `ManifestToMojom()` shim into consumers that
   aren't migrated yet. Then consumers in `components/webapps` (installable,
   banners, Android shortcut / WebAPK), `chrome/browser/web_applications`,
   `chrome/browser/ui/web_applications`, and `components/favicon` drop their
   shims and take `const blink::Manifest&`, in independent changes.
   Empty-manifest sentinels become `std::optional`.
6. **Typemap.** Add the `cpp_typemaps` entries and the `StructTraits` (intrinsic
   checks move into `Read()`). Remove the bridges, have the host receive
   `blink::Manifest` directly, and make `OnConnectionError` notify in-flight
   subscriptions. Update `AGENTS.md` and `README.md` across
   `components/webapps/`, `content/browser/manifest/`, and
   `third_party/blink/public/common/manifest/`.

______________________________________________________________________

## 9. Future Work & Technical Debt

- **Debt paid down:** Deleted `MaybeGetBadMessageStringForManifest`, the
  empty-manifest sentinel, and most validation browser tests.
- **`WebAppInstallInfo` invariants (near term, optional):** Share the
  intrinsic-invariant helpers with `WebAppInstallInfo` construction CHECKs, so
  synthesized installs (preinstalled, placeholder, sync, policy) enforce the
  same rules.
- **`WebAppInstallInfo` holds a `blink::Manifest` (long term):** Remove its
  duplicated manifest-derived fields. Separate project.
- **Over-long URLs in the parser:** Drop (or treat as absent) any manifest URL
  field longer than `url::kMaxURLChars` on both request paths, fixing the
  existing renderer kill for, e.g., a `data:` icon over 2 MiB.
- **New invariants** (each needs a parser change first, then a report-only
  rollout):
  - A string length limit. The parser has none for top-level strings. The
    existing 4 KiB `TruncatedString16` limit (which rejects rather than
    truncates) covers only some sub-fields and is already a potential renderer
    kill for long shortcut descriptions.
  - Fragment-free `id` and `scope`. The parser already strips fragments, and
    `WebAppInstallInfo` CHECKs for them.
- **Remove `RequestManifest`:** crbug.com/452053908 removes the transitional
  `Manifest?`.
- **Encapsulate remaining open sub-structs** (`ImageResource`, `ShortcutItem`,
  `ShareTarget`, ...) so nested values are immutable too.

______________________________________________________________________

## 10. Implementation Retrospective

The [crbug.com/485890278](https://crbug.com/485890278) work landed across the
16-CL migration stack and post-cutover storage and fast-path refinements. Key
architectural lessons from the implementation:

1. **Intrinsic vs. contextual invariant split:** The split between intrinsic
   invariants (`I1`–`I13` against `start_url`) and contextual host checks
   (`C0`–`C5` in
   [ManifestManagerHost](/content/browser/manifest/manifest_manager_host.cc))
   held without exceptions. This model preserved exact behavioral parity across
   `https:`, `blob:`, `filesystem:`, `isolated-app:`, `chrome-extension:`, and
   `file:` origins with zero benign renderer kills. Routing DevTools
   (`RequestManifestDebugInfo`) through Mojo traits and embedder overrides
   (`MaybeOverrideManifest`) through `Manifest::Builder` closed both unvalidated
   paths by construction.
2. **Outward bridge rollout across ~150 files:** Temporary `ManifestFromMojom`
   and `ManifestToMojom` bridges let 10 incremental CLs migrate
   [content/](/content/browser/manifest/),
   [components/webapps/](/components/webapps/),
   [chrome/browser/web_applications/](/chrome/browser/web_applications/), and
   [components/favicon/](/components/favicon/) one API boundary at a time. Once
   all consumers used `blink::Manifest`, the final cutover activated 9 new Mojo
   `cpp_typemaps` (`blink.mojom.Manifest` with `default_constructible = false`,
   plus 8 nested value structs). Deleting `MaybeGetBadMessageStringForManifest`
   removed ~190 lines of manual host checks and ~20 `IsEmptyManifest` sites.
   Moving round-trip coverage from browser tests to unit-level
   `SerializeAndDeserialize` tests also caught a pre-existing dangling
   `std::u16string_view` bug in
   [`ConvertAndTruncateOptionalString`](/third_party/blink/public/common/manifest/manifest_mojom_traits.h).
3. **Reference-counted immutable storage and builder ergonomics:** Storing all
   40 fields inline duplicated declarations between `Manifest` and `Builder` and
   made `sizeof(blink::Manifest)` ~2.2 KiB across per-tab
   `std::optional<Manifest>` caches. Moving fields into a private immutable
   `RefCountedThreadSafe` `Data` struct shrank `Manifest` and `Builder` to
   `sizeof(void*)` with `O(1)` copy and move. Value semantics remained intact,
   and `LIFETIME_BOUND` annotations on reference getters let Clang catch
   dangling temporaries across the `Data` pointer indirection. A single-argument
   `Builder(GURL start_url)` constructor and non-ref-qualified `Build()` methods
   removed test boilerplate after empty default manifests disappeared.
4. **Symmetric lint guards and per-field mutation tests:** Keeping 40 fields
   synchronized across Mojom, C++ storage, `StructTraits`, `operator==`, and
   tests proved error-prone when mid-flight changes (`SkColor` in
   [crbug.com/479448266](https://crbug.com/479448266)) landed. A symmetric 6-way
   `LINT.IfChange` clique links
   [manifest.mojom](/third_party/blink/public/mojom/manifest/manifest.mojom),
   [manifest.h](/third_party/blink/public/common/manifest/manifest.h),
   [manifest.cc](/third_party/blink/common/manifest/manifest.cc),
   [manifest_mojom_traits.cc](/third_party/blink/common/manifest/manifest_mojom_traits.cc),
   [manifest_unittest.cc](/third_party/blink/common/manifest/manifest_unittest.cc),
   and
   [manifest_mojom_traits_unittest.cc](/third_party/blink/common/manifest/manifest_mojom_traits_unittest.cc).
   Combined with 40-field single-mutation inequality tests, these guards force
   future schema additions to update serialization, equality, and unit tests
   together.

______________________________________________________________________

## Appendix A: FAQ

**Q: Why not add a public `ValidateContextFree()` (or
`PerformContextIndependentValidation()`)?** Intrinsic checks run at construction
(`Read()` and `Builder::Build()`), so no caller ever holds an instance that
could fail them, and a public method would have nothing to do. The only public
validation-related API is `Builder::Build()` returning a
`ManifestInvariantError`.

**Q: Does this protect force installs, policy installs, and other custom
installs?** Partially. Policy *overrides of fetched manifests* are covered,
because they go through `Builder`. Installs that build `WebAppInstallInfo`
directly (preinstalled, placeholder, sync, policy fallback) never create a
manifest and are out of scope (Non-Goal 2); Future Work proposes sharing the
invariants with `WebAppInstallInfo`.

**Q: Could KURL vs. GURL canonicalization differences cause false positives?**
No. Identical same-origin and prefix checks run in both the parser (KURL /
`SecurityOrigin`) and the browser (GURL / `url::Origin`, which re-parses KURL's
canonical spec). The design keeps the browser-side semantics exact, including
the non-segment-aware shortcut prefix match (I11).
