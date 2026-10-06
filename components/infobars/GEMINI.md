# InfoBars Component Guidelines

These instructions apply to the shared InfoBars component in
`components/infobars/`.

Read the [component README](README.md) and inspect the affected platform's
callers before changing shared interfaces or behavior.

## Preserve Component Layering

This component provides shared InfoBar infrastructure used by multiple
platforms. Keep Chrome-specific feature registration and browser-process
integration in `chrome/browser/`.

- Do not introduce dependencies from this component on `chrome/browser/`.
- Do not move Desktop registry logic into the shared component.
- When changing shared delegates, managers, or interfaces, inspect their
  callers across supported platforms.
- Do not apply Desktop feature-creation rules as a blanket prohibition on
  maintaining shared delegate infrastructure or platform implementations.

## Desktop Chrome

New Desktop feature InfoBars must use the centralized framework:

- [InfoBarSpec](/chrome/browser/infobars/infobar_spec.h) for appearance and
  behavior.
- [BrowserInfoBarManager](/chrome/browser/infobars/browser_infobar_manager.h)
  for registration, display, and dismissal.
- `InfoBarShowParams` for runtime values and per-instance callbacks.

Do not create new Desktop feature subclasses of `ConfirmInfoBarDelegate`
or `SimpleAlertInfoBarDelegate`, or directly use legacy creation helpers
such as `CreateConfirmInfoBar()`, `CreateSimpleAlertInfoBar()`, or
`ConfirmInfoBar::Create()`.

These rules do not prohibit maintaining framework internals or an existing
legacy implementation that has not yet been migrated.

### New InfoBars Versus Migrations

A new Desktop InfoBar does not need a migration study or legacy fallback.
Register its specification without `IsInfoBarMigrated()` and do not add
migration parameters to `infobar_features.h` or `.cc`, or a migration study
to `fieldtrial_testing_config.json`.

Preserve required platform and build guards. Product rollout and feature
eligibility checks remain separate from migration controls.

Existing legacy InfoBars undergoing migration may temporarily retain
`IsInfoBarMigrated()` and a fallback. Keep registration and display
conditions consistent and prevent duplicate display across the two paths.

Follow the complete [Desktop InfoBar guide](/chrome/browser/infobars/GEMINI.md)
and [registry guidelines](/chrome/browser/ui/infobars/GEMINI.md) when
implementing Desktop features. Those documents describe startup timing,
scope, callback ownership, and the correct dismissal overloads.

## Android

InfoBars are deprecated on Android in favor of the Message UI. Do not add
new Android InfoBars; use the
[Message UI guidance](/components/messages/README.md).

When maintaining existing Android InfoBars or shared infrastructure,
preserve existing behavior unless the requested change explicitly includes
a migration or removal.

## iOS

The Desktop framework guidance does not require iOS code to depend on
`BrowserInfoBarManager` or the Chrome Desktop registry.

Follow the existing iOS InfoBar architecture and platform guidance.
Preserve shared interfaces needed by iOS when changing component code.
Do not migrate or remove an iOS delegate solely to comply with Desktop
feature-creation rules.

## Identifiers and Metrics

When adding a value to `InfoBarDelegate::InfoBarIdentifier` in
[infobar_delegate.h](core/infobar_delegate.h):

- Append a fresh numeric value.
- Do not renumber existing values or reuse removed values.
- Add the matching value and label to the `InfoBarIdentifier` metrics enum
  in [enums.xml](/tools/metrics/histograms/metadata/browser/enums.xml).
- Follow `IfChange` / `ThenChange` requirements and the enum's
  platform naming conventions.
- Account for generated consumers, including the Java counterpart, when
  changing the enum or its annotations.

## Shared Behavior and Verification

For shared executable changes, inspect and test affected behavior such as
delegate ownership, duplicate detection, navigation expiration, user
actions, removal, and observer notification ordering.

Select tests based on the actual change and affected platforms. Desktop
validation alone does not establish compatibility for Android or iOS
callers of modified shared code.

For documentation-only changes, verify paths and API statements and keep
platform distinctions explicit. Keep this file consistent with the
[component README](README.md).
