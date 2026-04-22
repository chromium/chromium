# Design - Decouple Install and Reparenting (Closure Passing)

**Last Modified**: 2026-04-20 **Editor**: dmurph@chromium.org

This document describes the design for splitting the 'reparenting' or
'launching' portion of the web app install process from the 'install' portion on
desktop, by passing a closure to the dialog/caller.

## Background

Currently, the web app install process (e.g., in
`FetchManifestAndInstallCommand`) often automatically reparents the web contents
used for installation into the newly installed app's window. This is handled via
direct calls to `WebAppUiManager`. Similarly, `WebInstallFromUrlCommand`
launches the app directly using `WebAppUiManager` after installation.

For multi-step install flows, we need to decouple these operations. The install
commands should complete their primary task (installation) and then provide a
closure to the dialog/caller to handle the launch/reparenting step using the
command system.

## Goal

Modify the install process to allow separating the installation from the
reparenting/launching step, by passing a closure to the dialog/caller.

## Proposed Changes

### Callback Refactoring

We change how the install dialog communicates with the install command.

Instead of the dialog calling a callback that completes everything, the dialog
receives a callback that, when invoked to accept the install:

1. Schedules the installation.
2. Returns (via a reply callback) a boolean for success and a
   `base::OnceClosure`.
3. This `base::OnceClosure`, when called, will schedule a new command to handle
   the reparenting or launching.

The install command will complete and call its own callback immediately after
installation is complete, independent of whether the launch closure is ever
called. This avoids the "dead end" where the command's completion callback was
tied to the launch command.

Callers who do not want to trigger the launch (e.g., DevTools protocol handler)
can simply ignore the closure provided in the callback.

#### Callback Signature Change

In `chrome/browser/web_applications/web_app_install_params.h`:

```cpp
using WebAppInstallationAcceptanceResultCallback =
    base::OnceCallback<void(bool install_success, base::OnceClosure reparent_closure)>;

using WebAppInstallationAcceptanceCallback = base::OnceCallback<void(
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo>,
    WebAppInstallationAcceptanceResultCallback result_callback)>;
```

### New Command: `LaunchOrReparentWebContentsIntoAppCommand`

A new command is created to handle the post-install action for
`FetchManifestAndInstallCommand`.

**Name**: `LaunchOrReparentWebContentsIntoAppCommand`

**Behavior**:

- It takes the `AppId` and the `WebContents` that was used for installation.
- It uses `WebContentsManager` to check if the current web contents is in-scope
  of the app.
- If it is in-scope, it attempts to reparent the web contents into the app
  window.
- If it is NOT in-scope (or cannot reparent), it falls back to launching the app
  at its `start_url`.
- If the app is not configured to open in a dedicated window (e.g., opens in a
  browser tab), it does nothing and succeeds.

### `WebInstallFromUrlCommand`

- It will also use the new callback signature.
- The closure it returns will schedule a normal `LaunchWebAppCommand` (via
  `WebAppCommandScheduler::LaunchApp`) with
  `apps::LaunchSource::kFromWebInstallApi`.
- It will **never** use an existing web contents for reparenting.

### Impact on Existing Dialogs and Test Helpers

Existing install dialogs that want to maintain the current behavior can use the
`AdaptToLaunchOnInstallSuccess` helper to wrap their callback, which
automatically runs the `reparent_closure` immediately on success.

Test helpers like `AutoAcceptDialogCallback` have been updated to automatically
run the closure on success to ensure tests that expect auto-launch still work.

## Verification Plan

### Automated Tests

- Unit tests and browser tests for `LaunchOrReparentWebContentsIntoAppCommand`.
- Updated unit tests and browser tests for `FetchManifestAndInstallCommand` and
  `WebInstallFromUrlCommand` to handle the new callback signature.

### Manual Verification

- Verify standard install flows still result in reparenting or launching as
  before.
