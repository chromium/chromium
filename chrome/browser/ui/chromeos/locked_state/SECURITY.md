# Security Model for ChromeOS Locked Fullscreen & Locked State (`//chrome/browser/ui/chromeos/locked_state`)

Consult [chrome/browser/ui/SECURITY.md][browser-ui-sec] and
[Security for Agents][sec-agents] for broader Browser UI and Chromium security
rules.

## Scope

This policy applies to `//chrome/browser/ui/chromeos/locked_state/` (excluding
test files `*_browsertest.cc`) and to ChromeOS locked fullscreen / trusted-pinned
browser window confinement enforced across `//chrome/browser/ui/` (including
`chromeos::LockedStateController`, `platform_util::IsBrowserLockedFullscreen()`,
and `lockWindowFullscreenPrivate` /
`chrome.windows.update(..., {state: 'locked-fullscreen'})`).

## Threat Model

### Process Type & Sandbox

Runs in the unsandboxed Browser process on the UI thread, coordinating ChromeOS
window pinning (`ash::PinWindow(..., /*trusted=*/true)` /
`chromeos::WindowPinType::kTrustedPinned`), browser commands, tab-strip state,
and navigation/UI restrictions.

### Security Boundary Classification

**ChromeOS locked fullscreen (`kExtensionLocked`, `kOnTaskLocked`,
`kOnTaskLockedPaused`, `WindowPinType::kTrustedPinned`) is NOT a security
boundary.**

Locked fullscreen and locked-window modes are UX and workflow confinement
features (e.g., for proctoring extensions and managed classroom sessions)
intended to restrict casual local user interactions by hiding browser chrome and
disabling user-initiated shortcuts and commands. They do not establish a
security boundary against a local user or pre-existing browser/web state, nor do
they override Chromium's browser security invariants.

## Explicit Non-Bugs (Out of Scope)

Bypassing or escaping ChromeOS locked-fullscreen / locked-state confinement is
**out of scope** of Chromium's security threat model. Reports describing the
behaviors below are **not** security vulnerabilities (though unintended behavior
may be filed as functional bugs):

- **Programmatic tab or window activation while locked:** Background tabs, web
  content, extensions, or browser features activating or switching tabs or
  focusing windows while locked fullscreen or locked-state confinement is active
  (e.g., via `WebContentsDelegate::ActivateContents()`, `beforeunload` or
  other JavaScript app-modal dialogs, `chrome.tabs` APIs, or browser UI
  triggers).
- **Pre-staged content or user activation:** The local user opening helper
  tabs, acquiring sticky user activation, opening bubbles/dialogs/side panels,
  or staging browser state prior to entering locked fullscreen and interacting
  with it afterward.
- **Browser UI surfaces, bubbles, dialogs, commands, and shortcuts:** Opening,
  retaining, or interacting with browser UI (such as JavaScript app-modal
  dialogs, bubbles, side panels, Tab Search, Glic, or context menus), or
  invoking commands, hotkeys, global accelerators, or active-user switches
  while a window is trusted-pinned / locked fullscreen.
- **Navigating or retaining access within locked windows:** Reaching existing
  tabs, popups, or new navigations inside a locked window without memory
  corruption, sandbox escape, or cross-origin data theft.

## Design Justifications

### Precedence of Browser Security Invariants

Where locked-fullscreen confinement conflicts with Chromium browser security
defenses, browser security defenses take precedence and must not be disabled to
preserve confinement. For example, displaying a `beforeunload` or other
JavaScript app-modal dialog activates the requesting `WebContents`
(`CreateViewsJavaScriptDialog()` calling `ActivateContents()`) so a background
tab cannot spoof an app-modal dialog over a different foreground tab's origin.

## Reporting

Before filing a security bug affecting this area, review
[Security for Agents][sec-agents] to verify that the issue represents an
in-scope Chromium security vulnerability rather than a locked-fullscreen
confinement bypass.

[browser-ui-sec]: /chrome/browser/ui/SECURITY.md
[sec-agents]: /docs/security/security-for-agents.md
