# Critical Actions Internals WebUI

This directory contains the WebUI controller and Mojo interface for
`chrome://critical-actions-internals`, a developer debug page for inspecting and
managing the Critical Actions SQLite database.

## Purpose

This page is strictly an internal debug UI intended for Chromium developers
working on Critical Action Logging, Privacy, and related features. It allows
developers to:

-   Inspect records stored in the `CriticalActions` database table.
-   Test query pagination, text search, and action-type filtering.
-   Inspect structured JSON metadata associated with recorded actions.
-   Manually delete individual entries or clear the entire table during testing.

## Access & Prerequisites

-   **Platforms**: Desktop only (Windows, Mac, Linux, ChromeOS). Not available
    on Android or iOS.
-   **Internal Only**: Because it inherits
    `content::DefaultInternalWebUIConfig`, it is guarded by the
    `kInternalOnlyUisEnabled` preference.
-   **Feature Flag**: Logging requires the `CriticalActionHistory` feature:

    ```bash
    --enable-features=CriticalActionHistory
    ```

    (or enabled via `chrome://flags/#critical-action-history`). If the feature
    is disabled, the page will load with a warning banner indicating that action
    recording is inactive.

## Verification for Horizontal Updates

Developers performing horizontal infrastructure updates (e.g., Lit upgrades,
TypeScript/Rollup toolchain changes, `cr_elements` refactorings) can verify this
page using the following methods:

### 1. Automated Tests

Run the backend and page handler unit tests:

```bash
autoninja -C out/Default unit_tests && out/Default/unit_tests --gtest_filter="CriticalActions*"
```

Verify URL registration and histogram checks in browser tests:

```bash
autoninja -C out/Default browser_tests && out/Default/browser_tests --gtest_filter="WebUIUrlBrowserTest.UrlsInTestList:WebUIUrlHashesBrowserTest.*"
```

### 2. Manual Verification

1.  **Basic Load & Console Check**:

    -   Run Chrome: `out/Default/chrome --enable-features=CriticalActionHistory`
    -   Navigate to `chrome://critical-actions-internals`.
    -   Verify the page loads cleanly with no console errors in DevTools.
    -   If the database is empty, the table displays the placeholder message:
        `"No critical action records found in the database."`.

2.  **Populating Test Data & Testing Interactive Controls**:

    -   Trigger a critical action from Autobrowse/Actor (glic):
        -   **Password Manager (GPM)**: Autofill or save credentials on a login
            form.
        -   **Autofill**: Fill an address or payment form.
        -   **File download**: Ask actor to download any file from any website.
    -   Alternatively, call `CriticalActionService::AddCriticalAction()` in C++
        / test fixtures.
    -   Click **Refresh** to load the newly recorded actions.
    -   Verify table interactions:
        -   **Filtering**: Type a query into the search box or select an action
            type from the dropdown.
        -   **Metadata Viewer**: Click on the metadata cell preview to
            expand/collapse the formatted JSON inspector.
        -   **Pagination**: Use Prev/Next/First/Last buttons and page size
            selector when multiple entries exist.
        -   **Deletion**: Click **Delete** on a row to remove an entry, or click
            **Clear Table** to truncate the database.
