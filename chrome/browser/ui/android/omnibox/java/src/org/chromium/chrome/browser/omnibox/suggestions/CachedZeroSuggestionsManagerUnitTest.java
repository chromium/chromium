// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.KEY_JUMP_START_PAGE_CLASS;
import static org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.KEY_JUMP_START_URL;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_3_WITH_HEADER;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.SearchEngineMetadata;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link CachedZeroSuggestionsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedZeroSuggestionsManagerUnitTest {
    private static final int PAGE_CLASS = 0;
    private static final AutocompleteResult EMPTY_RESULT = AutocompleteResult.fromCache(null, null);

    /**
     * Compare two instances of CachedZeroSuggestionsManager to see if they are same, asserting if
     * they're not. Note that order is just as relevant as the content for caching.
     */
    void assertAutocompleteResultEquals(AutocompleteResult data1, AutocompleteResult data2) {
        assertEquals(data1.getSuggestionsList(), data2.getSuggestionsList());
        assertEquals(data1.getGroupsInfo(), data2.getGroupsInfo());
    }

    /**
     * Build a simple suggestions list.
     *
     * @param titlePrefix The prefix to use to populate suggestion data.
     * @param count How many suggestions to create.
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildSimpleSuggestionsList(String titlePrefix, int count) {
        List<AutocompleteMatch> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            list.add(
                    createSuggestionBuilder(
                                    titlePrefix, index + 1, OmniboxSuggestionType.HISTORY_URL)
                            .build());
        }

        return list;
    }

    /**
     * Create and partially initialize suggestion builder constructing simple OmniboxSuggestions.
     *
     * @param titlePrefix The prefix to use to populate suggestion data.
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @param type The type of suggestion to create.
     * @return AutocompleteMatchBuilder object to be used for further refinements.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(String titlePrefix, int id, int type) {
        return AutocompleteMatchBuilder.searchWithType(type)
                .setDisplayText(String.format("%s %d display text", titlePrefix, id))
                .setDisplayText(String.format("%s %d description", titlePrefix, id))
                .setUrl(new GURL("https://some.url/?q=" + titlePrefix));
    }

    @Test
    public void saveToCache_basicSaveRestoreTest() {
        var dataToCache = AutocompleteResult.fromCache(buildSimpleSuggestionsList("test", 2), null);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);
        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    public void saveToCache_dataIsAssociatedWithPageClass() {
        var dataToCache1 =
                AutocompleteResult.fromCache(buildSimpleSuggestionsList("test1", 2), null);
        var dataToCache2 =
                AutocompleteResult.fromCache(buildSimpleSuggestionsList("test2", 5), null);

        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache1);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 1, dataToCache2);

        var dataFromCache1 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        var dataFromCache2 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS + 1);
        assertAutocompleteResultEquals(dataToCache1, dataFromCache1);
        assertAutocompleteResultEquals(dataToCache2, dataFromCache2);
    }

    @Test
    public void saveToCache_DoNotCacheClipboardSuggestions() {
        var mix_list =
                Arrays.asList(
                        createSuggestionBuilder("test", 1, OmniboxSuggestionType.CLIPBOARD_IMAGE)
                                .build(),
                        createSuggestionBuilder("test", 2, OmniboxSuggestionType.HISTORY_URL)
                                .build(),
                        createSuggestionBuilder("test", 3, OmniboxSuggestionType.CLIPBOARD_TEXT)
                                .build(),
                        createSuggestionBuilder("test", 4, OmniboxSuggestionType.SEARCH_HISTORY)
                                .build());
        var expected_list =
                Arrays.asList(
                        createSuggestionBuilder("test", 2, OmniboxSuggestionType.HISTORY_URL)
                                .build(),
                        createSuggestionBuilder("test", 4, OmniboxSuggestionType.SEARCH_HISTORY)
                                .build());

        var dataToCache = AutocompleteResult.fromCache(mix_list, null);
        var dataToExpected = AutocompleteResult.fromCache(expected_list, null);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToExpected, dataFromCache);
    }

    @Test
    public void eraseCachedSuggestionsByPageClass() {
        var dataToCache1 =
                AutocompleteResult.fromCache(buildSimpleSuggestionsList("test1", 2), null);
        var dataToCache2 =
                AutocompleteResult.fromCache(buildSimpleSuggestionsList("test2", 5), null);

        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache1);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 1, dataToCache2);

        { // Confirmation check: no data lost.
            var dataFromCache1 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
            var dataFromCache2 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS + 1);
            assertAutocompleteResultEquals(dataToCache1, dataFromCache1);
            assertAutocompleteResultEquals(dataToCache2, dataFromCache2);
        }

        { // Erase first page class. Confirm first result is empty.
            CachedZeroSuggestionsManager.eraseCachedSuggestionsByPageClass(PAGE_CLASS);
            var dataFromCache1 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
            var dataFromCache2 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS + 1);
            assertAutocompleteResultEquals(EMPTY_RESULT, dataFromCache1);
            assertAutocompleteResultEquals(dataToCache2, dataFromCache2);
        }

        { // Erase second page class. Confirm both results are empty.
            CachedZeroSuggestionsManager.eraseCachedSuggestionsByPageClass(PAGE_CLASS + 1);
            var dataFromCache1 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
            var dataFromCache2 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS + 1);
            assertAutocompleteResultEquals(EMPTY_RESULT, dataFromCache1);
            assertAutocompleteResultEquals(EMPTY_RESULT, dataFromCache2);
        }
    }

    @Test
    public void eraseCachedData_removesAllCachedEntries() {
        var dataToCache = AutocompleteResult.fromCache(buildSimpleSuggestionsList("test", 3), null);

        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 2, dataToCache);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 4, dataToCache);

        var prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS)));
        assertTrue(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 2)));
        assertTrue(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 4)));

        CachedZeroSuggestionsManager.eraseCachedData();

        assertFalse(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS)));
        assertFalse(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 2)));
        assertFalse(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 4)));
    }

    @Test
    public void eraseCachedData_removesOldCachedEntries() {
        var prefs = ContextUtils.getAppSharedPreferences();
        // Pick some of the keys at random to verify all of them have been deleted.
        // We preserved up to 15 suggestions in the past, so index 14 is the last key that
        // realistically makes sense to be deleted.
        var keyToTest = ChromePreferenceKeys.KEY_ZERO_SUGGEST_GROUP_ID_PREFIX.createKey(14);

        // Put some irrelevant information in the cache using old keys.
        var editor = prefs.edit();
        editor.putInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, 0);
        editor.putInt(keyToTest, 0);
        editor.apply();

        // Confirmation check: keys are there.
        assertTrue(prefs.contains(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE));
        assertTrue(prefs.contains(keyToTest));

        // Erase data and confirm keys are absent.
        CachedZeroSuggestionsManager.eraseCachedData();
        assertFalse(prefs.contains(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE));
        assertFalse(prefs.contains(keyToTest));
    }

    @Test
    public void readFromCache_restoreDetailsFromEmptyCache() {
        CachedZeroSuggestionsManager.eraseCachedData();

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(EMPTY_RESULT, dataFromCache);
    }

    @Test
    public void readFromCache_restoreDetailsFromEmptyResult() {
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, EMPTY_RESULT);

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(EMPTY_RESULT, dataFromCache);
    }

    @Test
    public void saveToCache_preservesGroupsInfo() {
        var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(30, SECTION_3_WITH_HEADER)
                        .build();

        var dataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    private void saveJumpStartContext(String url, int pageClass) {
        CachedZeroSuggestionsManager.saveJumpStartContext(
                new CachedZeroSuggestionsManager.JumpStartContext(new GURL(url), pageClass));
    }

    @Test
    public void jumpStartContext_saveRestoreValidContext() {
        var prefs = ContextUtils.getAppSharedPreferences();

        saveJumpStartContext("https://abc.xyz", 123);

        assertEquals("https://abc.xyz/", prefs.getString(KEY_JUMP_START_URL, null));
        assertEquals(123, prefs.getInt(KEY_JUMP_START_PAGE_CLASS, 0));

        var jsContext = CachedZeroSuggestionsManager.readJumpStartContext();
        assertEquals("https://abc.xyz/", jsContext.url.getSpec());
        assertEquals(123, jsContext.pageClass);
    }

    @Test
    public void jumpStartContext_saveRestoreEmptyUrl() {
        var prefs = ContextUtils.getAppSharedPreferences();

        // Initialize with valid context to verify data being erased.
        saveJumpStartContext("https://abc.xyz", 123);
        assertTrue(prefs.contains(KEY_JUMP_START_URL));

        // Preserve empty URL
        saveJumpStartContext("", 456);
        assertFalse(prefs.contains(KEY_JUMP_START_URL));
        assertFalse(prefs.contains(KEY_JUMP_START_PAGE_CLASS));
    }

    @Test
    public void jumpStartContext_saveRestoreInvalidUrl() {
        var prefs = ContextUtils.getAppSharedPreferences();

        // Initialize with valid context to verify data being erased.
        saveJumpStartContext("https://abc.xyz", 123);
        assertTrue(prefs.contains(KEY_JUMP_START_URL));

        // Preserve invalid URL
        saveJumpStartContext("asdf", 456);
        assertFalse(prefs.contains(KEY_JUMP_START_URL));
        assertFalse(prefs.contains(KEY_JUMP_START_PAGE_CLASS));
    }

    @Test
    public void jumpStartContext_saveRestoreNullContext() {
        var prefs = ContextUtils.getAppSharedPreferences();

        // Initialize with valid context to verify data being erased.
        saveJumpStartContext("https://abc.xyz", 123);
        assertTrue(prefs.contains(KEY_JUMP_START_URL));

        // Preserve invalid URL
        CachedZeroSuggestionsManager.saveJumpStartContext(null);
        assertFalse(prefs.contains(KEY_JUMP_START_URL));
        assertFalse(prefs.contains(KEY_JUMP_START_PAGE_CLASS));
    }

    @Test
    public void jumpStartContext_eraseCachedData() {
        var prefs = ContextUtils.getAppSharedPreferences();

        // Initialize with valid context to verify data being erased.
        saveJumpStartContext("https://abc.xyz", 123);
        assertTrue(prefs.contains(KEY_JUMP_START_URL));

        // Preserve invalid URL
        CachedZeroSuggestionsManager.eraseCachedData();
        assertFalse(prefs.contains(KEY_JUMP_START_URL));
        assertFalse(prefs.contains(KEY_JUMP_START_PAGE_CLASS));
    }

    @Test
    public void dseMetadata_restoreFromEmptyCache() {
        var prefs = ContextUtils.getAppSharedPreferences();
        // Start with empty prefs.
        prefs.edit().clear().apply();
        assertNull(CachedZeroSuggestionsManager.readSearchEngineMetadata());
    }

    @Test
    public void dseMetadata_restoreFromEmptyKeyword() {
        var prefs = ContextUtils.getAppSharedPreferences();

        CachedZeroSuggestionsManager.saveSearchEngineMetadata(new SearchEngineMetadata(null));
        assertNull(CachedZeroSuggestionsManager.readSearchEngineMetadata());

        CachedZeroSuggestionsManager.saveSearchEngineMetadata(new SearchEngineMetadata(""));
        assertNull(CachedZeroSuggestionsManager.readSearchEngineMetadata());
    }

    @Test
    public void dseMetadata_restoreValidMetadata() {
        var prefs = ContextUtils.getAppSharedPreferences();

        CachedZeroSuggestionsManager.saveSearchEngineMetadata(new SearchEngineMetadata("keyword"));
        var persisted = CachedZeroSuggestionsManager.readSearchEngineMetadata();
        assertNotNull(persisted);
        assertEquals("keyword", persisted.keyword);
    }
}
