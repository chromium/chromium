# Desktop InfoBar Registry Guidelines

These instructions apply to Desktop InfoBar registration and related code in
`chrome/browser/ui/infobars/`.

Follow the [Desktop InfoBar guide](/chrome/browser/infobars/GEMINI.md) for
framework usage, identifiers, runtime parameters, callback ownership, and
dismissal. Keep this file focused on registry responsibilities.

## Inspect the Registry Before Editing

Read [browser_infobar_registry.cc](browser_infobar_registry.cc) and the
current contracts in:

- [infobar_spec.h](/chrome/browser/infobars/infobar_spec.h)
- [browser_infobar_manager.h](/chrome/browser/infobars/browser_infobar_manager.h)

Inspect the feature's display call sites to confirm when registration is
needed and which platforms and configurations must support it.

## Register New InfoBars Without Migration Gates

For a new Desktop InfoBar:

- Build an `infobars::InfoBarSpec` and register it with
  `infobars::BrowserInfoBarManager`.
- Do not guard registration with `IsInfoBarMigrated()`.
- Do not add migration parameters to `infobar_features.h` or `.cc`.
- Do not add a migration study to `fieldtrial_testing_config.json`.
- Do not create a legacy delegate or fallback path.

Unconditional registration means registration without a migration gate.
Preserve necessary platform and build guards. Product rollout and feature
eligibility checks are separate from migration controls; do not remove
them merely because a new InfoBar uses the centralized framework.

## Choose the Correct Registration Phase

- Use `RegisterInfoBars()` for normal registration.
- Use `RegisterPreProfileInitInfoBars()` when display can occur before
  profile initialization.
- Register each identifier exactly once, not in both functions.
- Ensure registration occurs before any matching `Show()` or
  `ShowGlobally()` call.
- Do not access profile-dependent state during pre-profile registration.

Keep registration compatible with the registry's existing manager
availability checks and startup lifecycle.

## Keep Specifications Safe for Process-Wide Storage

Specifications are stored by a browser-process-wide manager. Define stable
appearance and behavior at registration time. Pass dynamic message text,
substitutions, and per-instance callbacks through `InfoBarShowParams` at
the display call site.

- Use localized resources for user-visible strings.
- Do not capture short-lived tab, controller, or profile objects by
  reference in registered callbacks.
- Follow the callback lifetime and teardown contracts in `infobar_spec.h`.
- Choose tab or global scope deliberately and configure lifecycle behavior
  when the defaults do not match the feature.
- For global InfoBars, check browser eligibility and use a browser filter
  when required by the feature.

## Preserve Migration Behavior for Existing InfoBars

`IsInfoBarMigrated()` is for existing legacy InfoBars undergoing migration
that temporarily retain a fallback.

When modifying a migration:

- Keep registration and display conditions compatible.
- Ensure the centralized path cannot run without its specification being
  registered.
- Ensure a single trigger cannot show both implementations.
- Preserve the legacy fallback until its removal is part of the intended
  change.
- Preserve existing actions, eligibility, lifecycle, and metrics unless
  the change explicitly modifies them.

## Keep Identifiers and Metrics Synchronized

For a new identifier, update both:

- `InfoBarDelegate::InfoBarIdentifier` in
  [infobar_delegate.h](/components/infobars/core/infobar_delegate.h).
- The corresponding `InfoBarIdentifier` metrics enum in
  [enums.xml](/tools/metrics/histograms/metadata/browser/enums.xml).

Append a fresh numeric value. Do not renumber existing values or reuse
removed values. Follow the enum's `IfChange` / `ThenChange`
requirements.

## Verify the Changed Registration Path

For executable changes, use focused tests appropriate to the change to
verify registration timing, supported configurations, and display behavior.
Exercise both migration paths when a legacy fallback remains.

For documentation-only changes, check examples and API claims against the
current implementation. Keep shared framework guidance in the
[Desktop InfoBar guide](/chrome/browser/infobars/GEMINI.md) rather than
duplicating its full examples here.
