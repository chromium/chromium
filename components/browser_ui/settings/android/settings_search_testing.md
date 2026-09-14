# Android Settings Search Testing Guide

The settings search index and screen UI must be kept in sync to provide a good user experience and prevent broken or missing search results (see [common failures](#3-common-failures)).

During index resolution, `SettingsIndexData.resolveIndex()` builds a settings graph and performs a Breadth-First Search (BFS) traversal back to root `MainSettings`. Disconnected entries are pruned from the index. To prevent silent regressions, Settings Search uses a two-tier testing model:

1. **Implicit Tests**: Invariants that automatically crawl and validate basic search infrastructure and graph connectivity across all registered screens.
2. **Explicit Tests**: Parity and end-to-end UI tests added by feature developers to their test suites (recommended for complete coverage).

---

## 1. Implicit Tests

* **Static XML & Provider Invariant (`SearchIndexProviderRegistryTest`)**:
  Validates that referenced XML resources exist and parse cleanly, and asserts that every preference linking to a subpage via `android:fragment` maps to a registered provider in `SearchIndexProviderRegistry.ALL_PROVIDERS`.
* **Graph Reachability & Orphan Detection (`SettingsSearchOrphanTest`)**:
  Resolves the settings graph across signed-out and signed-in states, asserting zero unexpected orphaned preferences (`MISSING_PARENT_LINK`, `BROKEN_ANCESTOR_PATH`, or `VALID_PARENT_REMOVAL`).

---

## 2. Explicit Tests

### Screen Parity Tests (`assertPreferenceScreenMatchesIndex`)
Verifies bidirectional consistency between a live settled `PreferenceScreen` and `SettingsIndexData` under different flags or account states:

* **Forward Check (UI &rarr; Index)**: Every visible, actionable preference exists in the search index with `isSearchable = true` and matching title text.
* **Reverse Check (Index &rarr; UI)**: Every searchable entry in the index is visible on the live screen, preventing ghost search results.
* **Allowlist**: Ephemeral items (e.g. personal user data entries or promo banners) can be passed in `allowlistedKeys`. Summaries are excluded from title parity assertions.

```java
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.assertPreferenceScreenMatchesIndex;

@Test
@SmallTest
public void testPreferenceScreenMatchesSearchIndex() {
    // Set up preconditions (accounts, mocks, feature flags) and launch activity
    mSettingsTestRule.startSettingsActivity();
    MySettingsFragment fragment = mSettingsTestRule.getFragment();

    assertPreferenceScreenMatchesIndex(fragment);
}
```

### End-to-End (E2E) UI Tests
Adding a basic 1–2 end-to-end UI tests is recommended to make sure all works well:

```java
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.clickSearchResult;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.highlighted;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.typeSearchQuery;

@Rule
public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
        new SettingsActivityTestRule<>(null); // null sets up the search bar

@Test
@SmallTest
public void testSearchAndNavigateToSetting() {
    mSettingsActivityTestRule.startSettingsActivity();

    // 1. Search for preference
    typeSearchQuery("autofill");

    // 2. Click matching result
    clickSearchResult(R.string.autofill_options_title);

    // 3. Assert destination screen opens and target preference is highlighted
    onView(highlighted(R.string.autofill_options_title)).check(matches(isDisplayed()));
}
```

---

## 3. Common Failures

| Symptom | Root Cause | Fix |
| :--- | :--- | :--- |
| **Missing Search Provider** | Fragment has no `SEARCH_INDEX_DATA_PROVIDER` or is omitted from `SearchIndexProviderRegistry.ALL_PROVIDERS`. | Implement `SEARCH_INDEX_DATA_PROVIDER` in the fragment (e.g. `new BaseSearchIndexProvider(R.xml.my_preferences)`) and register it in `SearchIndexProviderRegistry.ALL_PROVIDERS` (or use `INDEX_OPT_OUT` if intentionally unsearchable). |
| **`MISSING_PARENT_LINK`** | Subpage fragment is not linked in XML or dynamically. | Add `android:fragment` in parent XML or call `addChildParentLink()`. |
| **`BROKEN_ANCESTOR_PATH`** | Parent preference exists, but its path to `MainSettings` is severed. | Ensure intermediate parent preferences are registered and reachable. |
| **Missing Search Entry** | Preference visible on screen but missing from index. | Add `android:title` in XML or register in `updateDynamicPreferences()`. |
| **Ghost Search Entry** | Setting is indexed, but hidden on screen (`setVisible(false)`). | Call `indexData.removeEntry(getUniqueId(KEY))` when inactive. |
| **Title Mismatch** | Runtime UI title differs from static XML title. | Sync title in `updateDynamicPreferences()` or align XML string. |
